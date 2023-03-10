/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2000, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/

#include "AutoMounter.h"
#include "AutoLock.h"
#include "AutoMounterSettings.h"
#include "Commands.h"
#include "DeskWindow.h"
#include "LanguageTheme.h"
#include "TFSContext.h"
#include "Tracker.h"
#include "Utilities.h"

#include <Alert.h>
#include <Directory.h>
#include <Drivers.h>
#include <Path.h>
#include <FindDirectory.h>
#include <NodeMonitor.h>
#include <String.h>
#include <VolumeRoster.h>
#include <Volume.h>

#include <fs_info.h>
#include <string.h>
#include <stdio.h>

const uint32 kStartPolling = 'strp';
const char *kAutoMounterSettings = "automounter_settings";

struct OneMountFloppyParams {
	status_t result;
};

_DEVICE_MAP_ONLY(static bool gSilentAutoMounter;)
static BMessage gSettingsMessage;

#if xDEBUG
static Partition *
DumpPartition(Partition *_DEVICE_MAP_ONLY(partition), void*)
{
#if _INCLUDES_CLASS_DEVICE_MAP
	partition->Dump();
#endif
	return 0;
}
#endif


#if _INCLUDES_CLASS_DEVICE_MAP

struct MountPartitionParams {
	int32 uniqueID;
	status_t result;
};

/* Sets the Tracker Shell's AutoMounter to monitor a node.
 * n.b. Get's the one AutoMounter and uses Tracker's _special_ WatchNode.
 */

static status_t
AutoMounterWatchNode(const node_ref *nodeRef, uint32 flags)
{
	ASSERT(nodeRef != NULL);
	
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (tracker != NULL)
		return TTracker::WatchNode(nodeRef, flags, BMessenger(0, tracker->AutoMounterLoop()));
	
	return B_BAD_TYPE;
}
/* Tries to mount the partition and if it can it watches mount point.
 *
 */

static status_t
MountAndWatch(Partition *partition)
{
	ASSERT(partition != NULL);
	
	status_t status = partition->Mount();
	if (status != B_OK)
		return status;
  	 
	// Start watching this mount point
	node_ref nodeToWatch;
	status = partition->GetMountPointNodeRef(&nodeToWatch);
	if (status != B_OK) {
		PRINT(("Couldn't get mount point node ref: %s\n", strerror(status)));
		return status;
	}
  	 
	return AutoMounterWatchNode(&nodeToWatch, B_WATCH_NAME);
}

static Partition *
TryMountingEveryOne(Partition *partition, void *castToParams)
{
	MountPartitionParams *params = (MountPartitionParams *)castToParams;

	if (partition->Mounted() == kMounted) {
		if (!gSilentAutoMounter)
			PRINT(("%s already mounted\n", partition->VolumeName()));
	} else {
		status_t result = MountAndWatch(partition);

		// return error if caller asked for it
		if (params)
			params->result = result;

		if (!gSilentAutoMounter) {
			if (result == B_OK)
				PRINT(("%s mounted OK\n", partition->VolumeName()));
			else
				PRINT(("Error '%s' mounting %s\n",
					strerror(result), partition->VolumeName()));
		}

		if (params && result != B_OK)
			// signal an error
			return partition;
	}
	return NULL;
}

static Partition *
OneTryMountingFloppy(Partition *partition, void *castToParams)
{
	OneMountFloppyParams *params = (OneMountFloppyParams *)castToParams;
	if (partition->GetDevice()->IsFloppy()){
		status_t result = MountAndWatch(partition);
		// return error if caller asked for it
		if (params)
			params->result = result;

		return partition;
	}
	return 0;
}

static Partition *
OneMatchFloppy(Partition *partition, void *)
{
	if (partition->GetDevice()->IsFloppy())
		return partition;
	
	return 0;
}

static Partition *
TryMountingBFSOne(Partition *partition, void *params)
{	
	if (strcmp(partition->FileSystemShortName(), "bfs") == 0) 
		return TryMountingEveryOne(partition, params);

	return NULL;
}

static Partition *
TryMountingRestoreOne(Partition *partition, void *params)
{
	Session *session = partition->GetSession();
	Device *device = session->GetDevice();

	// create the name for the virtual device
	char path[B_FILE_NAME_LENGTH];
	int len = (int)strlen(device->Name()) - (int)strlen("/raw");
	if (session->CountPartitions() != 1)
		sprintf(path, "%.*s/%ld_%ld", len, device->Name(), session->Index(), partition->Index());
	else
		sprintf(path, "%s", device->Name());

	// Find the name of the current device/volume in the saved settings
	// and open if found.
	const char *volumename;
	if (gSettingsMessage.FindString(path, &volumename) == B_OK
		&& strcmp(volumename, partition->VolumeName()) == 0)
		return TryMountingEveryOne(partition, params);

	return NULL;
}

static Partition *
TryMountingHFSOne(Partition *partition, void *params)
{	
	if (strcmp(partition->FileSystemShortName(), "hfs") == 0)
		return TryMountingEveryOne(partition, params);
	
	return NULL;
}


struct FindPartitionByDeviceIDParams {
	dev_t dev;
};

static Partition *
FindPartitionByDeviceID(Partition *partition, void *castToParams)
{
	FindPartitionByDeviceIDParams *params = (FindPartitionByDeviceIDParams*) castToParams;
	if (params->dev == partition->VolumeDeviceID())
		return partition;

	return 0;	
}


static Partition *
TryWatchMountPoint(Partition *partition, void *)
{
	node_ref nodeRef;
	if (partition->GetMountPointNodeRef(&nodeRef) == B_OK)
		AutoMounterWatchNode(&nodeRef, B_WATCH_NAME);

	return 0;
}


static Partition *
TryMountVolumeByID(Partition *partition, void *params)
{
	PRINT(("Try mounting partition %i\n", partition->UniqueID()));
	if (!partition->Hidden() && partition->UniqueID() == 
		((MountPartitionParams *)params)->uniqueID) {
		Partition *result = TryMountingEveryOne(partition, params);
		if (result)
			return result;

		return partition;
	}
	return NULL;
}

static Partition *
AutomountOne(Partition *partition, void *castToParams)
{
	PRINT(("Partition %s not mounted\n", partition->Name()));
	AutomountParams *params = (AutomountParams *)castToParams;

	if (params->mountRemovableDisksOnly
		&& (!partition->GetDevice()->NoMedia() 
			&& !partition->GetDevice()->Removable())) 
		// not removable, don't mount it
		return NULL;

	if (params->mountAllFS)
		return TryMountingEveryOne(partition, NULL);
	if (params->mountBFS)
		return TryMountingBFSOne(partition, NULL);
	if (params->mountHFS)
		return TryMountingHFSOne(partition, NULL);
	
	return NULL;
}

static Partition *
NotifyFloppyNotMountable(Partition *partition, void *)
{
	if (partition->Mounted() != kMounted
		&& partition->GetDevice()->IsFloppy()
		&& !partition->Hidden()) {
		(new BAlert("", LOCALE("The format of the floppy disk in the disk drive is "
			"not recognized or the disk has never been formatted."), LOCALE("OK")))->Go(0);
		partition->GetDevice()->Eject();
	}
	return NULL;
}

#endif

#ifdef MOUNT_MENU_IN_DESKBAR

// just for testing

Partition *
AddMountableItemToMessage(Partition *partition, void *castToParams)
{
	BMessage *message = static_cast<BMessage *>(castToParams);

	message->AddString("DeviceName", partition->GetDevice()->Name());
	const char *name;
	if (*partition->VolumeName())
		name = partition->VolumeName();
	else if (*partition->Type())
		name = partition->Type();
	else
		name = partition->GetDevice()->DisplayName();

	message->AddString("DisplayName", name);
	BMessage invokeMsg;
	if (partition->GetDevice()->IsFloppy())
		invokeMsg.what = kTryMountingFloppy;
	else
		invokeMsg.what = kMountVolume;
	invokeMsg.AddInt32("id", partition->UniqueID());
	message->AddMessage("InvokeMessage", &invokeMsg);
	return NULL;	
}

#endif // MOUNT_MENU_IN_DESKBAR

AutoMounter::AutoMounter(bool _DEVICE_MAP_ONLY(checkRemovableOnly),
	bool _DEVICE_MAP_ONLY(checkCDs), bool _DEVICE_MAP_ONLY(checkFloppies),
	bool _DEVICE_MAP_ONLY(checkOtherRemovable), bool _DEVICE_MAP_ONLY(autoMountRemovablesOnly),
	bool _DEVICE_MAP_ONLY(autoMountAll), bool _DEVICE_MAP_ONLY(autoMountAllBFS),
	bool _DEVICE_MAP_ONLY(autoMountAllHFS),
	bool initialMountAll, bool initialMountAllBFS, bool initialMountRestore, bool initialMountAllHFS)
	:	BLooper("DirPoller", B_LOW_PRIORITY),
		fInitialMountAll(initialMountAll),
		fInitialMountAllBFS(initialMountAllBFS),
		fInitialMountRestore(initialMountRestore),
		fInitialMountAllHFS(initialMountAllHFS),
		fSuspended(false),
		fQuitting(false)
{
#if _INCLUDES_CLASS_DEVICE_MAP
	fScanParams.shortestRescanHartbeat = 5000000;
	fScanParams.checkFloppies = checkFloppies;
	fScanParams.checkCDROMs = checkCDs;
	fScanParams.checkOtherRemovable = checkOtherRemovable;
	fScanParams.removableOrUnknownOnly = checkRemovableOnly;
	
	fAutomountParams.mountAllFS = autoMountAll;
	fAutomountParams.mountBFS = autoMountAllBFS;
	fAutomountParams.mountHFS = autoMountAllHFS;
	fAutomountParams.mountRemovableDisksOnly = autoMountRemovablesOnly;


	gSilentAutoMounter = true;
	
	if (!BootedInSafeMode()) {
		ReadSettings();
		thread_id rescan = spawn_thread(AutoMounter::InitialRescanBinder, 
			"AutomountInitialScan", B_DISPLAY_PRIORITY, this);
		resume_thread(rescan);
	} else {
		// defeat automounter in safe mode, don't even care about the settings
		fAutomountParams.mountAllFS = false;
		fAutomountParams.mountBFS = false;
		fAutomountParams.mountHFS = false;
		fInitialMountAll = false;
		fInitialMountAllBFS = false;
		fInitialMountRestore = false;
		fInitialMountAllHFS = false;
	}

	//	Watch mount/unmount
	TTracker::WatchNode(0, B_WATCH_MOUNT, BMessenger(0, this));
#endif

}

AutoMounter::~AutoMounter()
{
}

Partition* AutoMounter::FindPartition(dev_t _DEVICE_MAP_ONLY(dev))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	FindPartitionByDeviceIDParams params;
	params.dev = dev;
	return fList.EachMountedPartition(FindPartitionByDeviceID, &params);
#else
	return NULL;
#endif
}


void 
AutoMounter::RescanDevices()
{
#if _INCLUDES_CLASS_DEVICE_MAP
	stop_watching(0, this);
	fList.RescanDevices(true);
	fList.UpdateMountingInfo();
	fList.EachMountedPartition(TryWatchMountPoint, 0);
	TTracker::WatchNode(0, B_WATCH_MOUNT, BMessenger(0, this));	
	fList.EachMountedPartition(TryWatchMountPoint, 0);
#endif
}

void
AutoMounter::MessageReceived(BMessage *message)
{
	switch (message->what) {
#if _INCLUDES_CLASS_DEVICE_MAP
		case kAutomounterRescan:
			RescanDevices();
			break;

		case kStartPolling:	
			PRINT(("starting the automounter\n"));
			
			fScanThread = spawn_thread(AutoMounter::WatchVolumeBinder, 
#if DEBUG				
				"HiroshiLikesAtomountScan",	// long story
#else
				"AutomountScan",
#endif				
				B_LOW_PRIORITY, this);
			resume_thread(fScanThread);
			break;

		case kMountVolume:
			MountVolume(message);
			break;

		case kUnmountVolume:
			UnmountAndEjectVolume(message);
			break;

		case kSetAutomounterParams:
			{
				bool rescanNow = false;
				message->FindBool("rescanNow", &rescanNow);
				SetParams(message, rescanNow);
				WriteSettings();
				break;
			}

		case kMountAllNow:
			RescanDevices();
			MountAllNow();
			break;

		case kSuspendAutomounter:
			SuspendResume(true);
			break;

		case kResumeAutomounter:
			SuspendResume(false);
			break;

		case kTryMountingFloppy:
			TryMountingFloppy();
			break;

		case B_NODE_MONITOR:
			{
				int32 opcode;
				if (message->FindInt32("opcode", &opcode) != B_OK)
					break;
	
				switch (opcode) {
					case B_DEVICE_MOUNTED: {
						WRITELOG(("** Received Device Mounted Notification"));
						dev_t device;
						if (message->FindInt32("new device", &device) == B_OK) {
							Partition *partition =  FindPartition(device);
							if (partition == NULL || partition->Mounted() != kMounted) {
								WRITELOG(("Device %i not in device list.  Someone mounted it outside "
									"of Tracker", device));
		
								//
								// This is the worst case.  Someone has mounted
								// something from outside of tracker.  
								// Unfortunately, there's no easy way to tell which
								// partition was just mounted (or if we even know about the device),
								// so stop watching all nodes, rescan to see what is now mounted,
								// and start watching again.
								//
								RescanDevices();
							} else
								WRITELOG(("Found partition\n"));
						} else {
							WRITELOG(("ERROR: Could not find mounted device ID in message"));
							PRINT_OBJECT(*message);
						}
	
						break;
					}
	
	
					case B_DEVICE_UNMOUNTED: {
						WRITELOG(("*** Received Device Unmounted Notification"));
						dev_t device;
						if (message->FindInt32("device", &device) == B_OK) {
							Partition *partition = FindPartition(device);
	
							if (partition != 0) {
								WRITELOG(("Found device in device list. Updating state to unmounted."));
								partition->SetMountState(kNotMounted);
							} else
								WRITELOG(("Unmounted device %i was not in device list", device));
						} else {
							WRITELOG(("ERROR: Could not find unmounted device ID in message"));
							PRINT_OBJECT(*message);
						}
	
						break;
					}	
	
	
					//	The name of a mount point has changed
					case B_ENTRY_MOVED: {
						WRITELOG(("*** Received Mount Point Renamed Notification"));
					
						const char *newName;
						if (message->FindString("name", &newName) != B_OK) {
							WRITELOG(("ERROR: Couldn't find name field in update message"));
							PRINT_OBJECT(*message);
							break ;
						}
	
						//
						// When the node monitor reports a move, it gives the
						// parent device and inode that moved.  The problem is 
						// that  the inode is the inode of root *in* the filesystem,
						// which is generally always the same number for every 
						// filesystem of a type.
						//
						// What we'd really like is the device that the moved	
						// volume is mounted on.  Find this by using the 
						// *new* name and directory, and then stat()ing that to
						// find the device.
						//
						dev_t parentDevice;
						if (message->FindInt32("device", &parentDevice) != B_OK) {
							WRITELOG(("ERROR: Couldn't find 'device' field in update"
								" message"));
							PRINT_OBJECT(*message);
							break;
						}
		
						ino_t toDirectory;	
						if (message->FindInt64("to directory", &toDirectory)!=B_OK){
							WRITELOG(("ERROR: Couldn't find 'to directory' field in update"
							  "message"));
							PRINT_OBJECT(*message);
							break;
						}
		
						entry_ref root_entry(parentDevice, toDirectory, newName);
	
						BNode entryNode(&root_entry);
						if (entryNode.InitCheck() != B_OK) {
							WRITELOG(("ERROR: Couldn't create mount point entry node: %s/n", 
								strerror(entryNode.InitCheck())));
							break;
						}	
	
						node_ref mountPointNode;
						if (entryNode.GetNodeRef(&mountPointNode) != B_OK) {
							WRITELOG(("ERROR: Couldn't get node ref for new mount point"));
							break;
						}
				
	
						WRITELOG(("Attempt to rename device %li to %s", mountPointNode.device,
							newName));

						Partition *partition = FindPartition(mountPointNode.device);
						if (partition != NULL) {
							WRITELOG(("Found device, changing name."));

							BVolume mountVolume(partition->VolumeDeviceID());
							BDirectory mountDir;
							mountVolume.GetRootDirectory(&mountDir);
							BPath dirPath(&mountDir, 0);
					
							partition->SetMountedAt(dirPath.Path());
							partition->SetVolumeName(newName);					
							break;
						}
						else
							WRITELOG(("ERROR: Device %li does not appear to be present",
								mountPointNode.device));
					}
				}
			}
			break;


#endif
		default:
			BLooper::MessageReceived(message);
			break;
	}
}

status_t
AutoMounter::WatchVolumeBinder(void *_DEVICE_MAP_ONLY(castToThis))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	static_cast<AutoMounter *>(castToThis)->WatchVolumes();
	return B_OK;
#else
	return B_UNSUPPORTED;
#endif
}

void
AutoMounter::WatchVolumes()
{
#if _INCLUDES_CLASS_DEVICE_MAP
	for(;;) {
		snooze(fScanParams.shortestRescanHartbeat);
		
		AutoLock<BLooper> lock(this);
		if (!lock)
			break;
			
		if (fQuitting) {
			lock.Unlock();
			break;
		}

		if (!fSuspended && fList.CheckDevicesChanged(&fScanParams)) {
			fList.UnmountDisappearedPartitions();
			fList.UpdateChangedDevices(&fScanParams);
			fList.EachMountablePartition(AutomountOne, &fAutomountParams);
		}
	}
#endif
}

static Device *
FindFloppyDevice(Device *_DEVICE_MAP_ONLY(device), void *)
{
#if _INCLUDES_CLASS_DEVICE_MAP
	if (device->IsFloppy())
		return device;
#endif

	return 0;
}

void
AutoMounter::EachMountableItemAndFloppy(EachPartitionFunction _DEVICE_MAP_ONLY(func),
	void *_DEVICE_MAP_ONLY(passThru))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	AutoLock<BLooper> lock(this);

#if 0
	//
	//	Rescan now to see if anything has changed.  
	//
	if (fList.CheckDevicesChanged(&fScanParams)) {
		fList.UpdateChangedDevices(&fScanParams);
		fList.UnmountDisappearedPartitions();
	}
#endif

	//
	//	If the floppy has never been mounted, it won't have a partition
	// 	in the device list (but it will have a device entry).  If it has, the 
	//	partition will appear, but will be set not mounted.  Code here works 
	//	around this.
	//
	if (!IsFloppyMounted() && !FloppyInList()) {
		Device *floppyDevice = fList.EachDevice(FindFloppyDevice, 0);

		//
		//	See comments under 'EachPartition'
		//
		if (floppyDevice != 0) {
			Session session(floppyDevice, "floppy", 0, 0, 0);
			Partition partition(&session, "floppy", "unknown",
				"unknown", "unknown", "floppy", "", 0, 0, 0, false);

			(func)(&partition, passThru);
		}
	}
	
	fList.EachMountablePartition(func, passThru);
#endif
}

void
AutoMounter::EachMountedItem(EachPartitionFunction _DEVICE_MAP_ONLY(func),
	void *_DEVICE_MAP_ONLY(passThru))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	AutoLock<BLooper> lock(this);
	fList.EachMountedPartition(func, passThru);
#endif
}

Partition *
AutoMounter::EachPartition(EachPartitionFunction _DEVICE_MAP_ONLY(func),
	void *_DEVICE_MAP_ONLY(passThru))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	AutoLock<BLooper> lock(this);

	if (!IsFloppyMounted() && !FloppyInList()) {
		Device *floppyDevice = fList.EachDevice(FindFloppyDevice, 0);

		//
		//	Add a floppy to the list.  It normally doesn't appear
		//	there when a floppy hasn't been mounted because it
		//	doesn't have any partitions.  Note that this makes sure
		//	that a floppy device exists before adding it, because
		//	some systems don't have floppy drives (unbelievable, but
		//	true).
		//
		if (floppyDevice != 0) {
			Session session(floppyDevice, "floppy", 0, 0, 0);
			Partition partition(&session, "floppy", "unknown",
				"unknown", "unknown", "floppy", "", 0, 0, 0, false);
	
			Partition *result = func(&partition, passThru);
			if (result != NULL)
				return result;
		}
	}
	
	return fList.EachPartition(func, passThru);
#else
	return NULL;
#endif
}

void
AutoMounter::CheckVolumesNow()
{
#if _INCLUDES_CLASS_DEVICE_MAP
	AutoLock<BLooper> lock(this);
	if (fList.CheckDevicesChanged(&fScanParams)) {
		fList.UnmountDisappearedPartitions();
		fList.UpdateChangedDevices(&fScanParams);
		if (!fSuspended)
			fList.EachMountablePartition(AutomountOne, &fAutomountParams);
	}
#endif
}

void 
AutoMounter::SuspendResume(bool _DEVICE_MAP_ONLY(suspend))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	AutoLock<BLooper> lock(this);
	fSuspended = suspend;
	if (fSuspended)
		suspend_thread(fScanThread);
	else
		resume_thread(fScanThread);
#endif
}

void
AutoMounter::MountAllNow()
{
#if _INCLUDES_CLASS_DEVICE_MAP
	AutoLock<BLooper> lock(this);
	
	DeviceScanParams mountAllParams;
	mountAllParams.checkFloppies = true;
	mountAllParams.checkCDROMs = true;
	mountAllParams.checkOtherRemovable = true;
	mountAllParams.removableOrUnknownOnly = true;

	fList.UnmountDisappearedPartitions();
	fList.UpdateChangedDevices(&mountAllParams);
	fList.EachMountablePartition(TryMountingEveryOne, 0);
	fList.EachPartition(NotifyFloppyNotMountable, 0);
#endif
}

void 
AutoMounter::TryMountingFloppy()
{
#if _INCLUDES_CLASS_DEVICE_MAP
	AutoLock<BLooper> lock(this);
	
	DeviceScanParams mountAllParams;
	mountAllParams.checkFloppies = true;
	mountAllParams.checkCDROMs = false;
	mountAllParams.checkOtherRemovable = false;
	mountAllParams.removableOrUnknownOnly = false;

	fList.UnmountDisappearedPartitions();
	fList.UpdateChangedDevices(&mountAllParams);
	OneMountFloppyParams params;
	params.result = B_ERROR;
	fList.EachMountablePartition(OneTryMountingFloppy, &params);
	if (params.result != B_OK)
		(new BAlert("", LOCALE("The format of the floppy disk in the disk drive is "
			"not recognized or the disk has never been formatted."), LOCALE("OK")))
			->Go(0);
#endif
}

bool 
AutoMounter::IsFloppyMounted()
{
#if _INCLUDES_CLASS_DEVICE_MAP
	AutoLock<BLooper> lock(this);
	return fList.EachMountedPartition(OneMatchFloppy, 0) != NULL;
#else
	return false;
#endif
}

bool 
AutoMounter::FloppyInList()
{
#if _INCLUDES_CLASS_DEVICE_MAP
	AutoLock<BLooper> lock(this);
	return fList.EachPartition(OneMatchFloppy, 0) != NULL;
#else
	return false;
#endif
}

void
AutoMounter::MountVolume(BMessage *_DEVICE_MAP_ONLY(message))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	int32 uniqueID;
	if (message->FindInt32("id", &uniqueID) == B_OK) {

		if (uniqueID == kFloppyID) {
			TryMountingFloppy();
			return;
		}

		MountPartitionParams params;
		params.uniqueID = uniqueID;
		params.result = B_OK;
		
		if (EachPartition(TryMountVolumeByID, &params) == NULL) 
			(new BAlert("", LOCALE("The format of this volume is unrecognized, or it has "
				"never been formatted"), LOCALE("OK")))->Go(0);
		else if (params.result != B_OK)	{
			char buffer[1024];
			sprintf(buffer, LOCALE("Error mounting volume. (%s)"), strerror(params.result));
			(new BAlert("", buffer, LOCALE("OK")))->Go(0);
		}
	}
#endif
}

status_t 
AutoMounter::InitialRescanBinder(void *_DEVICE_MAP_ONLY(castToThis))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	// maybe this can help the strange Tracker lock-up at startup
	snooze(500000LL);	// wait half a second
	
	AutoMounter *self = static_cast<AutoMounter *>(castToThis);
	self->InitialRescan();

	// Start watching nodes that were mounted before tracker started
	(self->fList).EachMountedPartition(TryWatchMountPoint, 0);

	self->PostMessage(kStartPolling, 0);
#endif
	return B_OK;
}

void 
AutoMounter::InitialRescan()
{
#if _INCLUDES_CLASS_DEVICE_MAP
	AutoLock<BLooper> lock(this);
	
	fList.RescanDevices(false);
	fList.UpdateMountingInfo();

	// if called after spawn_thread, must lock fList
	if (fInitialMountAll) {
		PRINT(("mounting all volumes\n"));
		fList.EachMountablePartition(TryMountingEveryOne, NULL);
	}
	
	if (fInitialMountAllHFS) {
		PRINT(("mounting all hfs volumes\n"));
		fList.EachMountablePartition(TryMountingHFSOne, NULL);
	}
	
	if (fInitialMountAllBFS) {
		PRINT(("mounting all bfs volumes\n"));
		fList.EachMountablePartition(TryMountingBFSOne, NULL);
	}

	if (fInitialMountRestore) {
		PRINT(("restoring all volumes\n"));
		fList.EachMountablePartition(TryMountingRestoreOne, NULL);
	}
#endif
}

struct UnmountDeviceParams {
	dev_t device;
	bool dontEject;
	status_t result;
};

static Partition *
UnmountIfMatchingID(Partition *_DEVICE_MAP_ONLY(partition),
	void *_DEVICE_MAP_ONLY(castToParams))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	UnmountDeviceParams *params = (UnmountDeviceParams *)castToParams;
	
	if (partition->VolumeDeviceID() == params->device) {

		TTracker *tracker = dynamic_cast<TTracker *>(be_app);
		if (tracker && tracker->QueryActiveForDevice(params->device)) {
			char buffer[2048];
			sprintf(buffer, LOCALE("To unmount %s some query windows have to "
			"be closed. Would you like to close the query windows?"), partition->VolumeName());
			if ((new BAlert("", buffer, LOCALE("Cancel"), LOCALE("Close and unmount"), NULL,
				B_WIDTH_FROM_LABEL))->Go() == 0)
				return partition;
			tracker->CloseActiveQueryWindows(params->device);
		}

		params->result = partition->Unmount();
		Device *device = partition->GetDevice();
		bool deviceHasMountedPartitions = false;
		if (params->result == B_OK
			&& device->Removable()) {
			for (int32 sessionIndex = 0; ; sessionIndex++) {
				Session *session = device->SessionAt(sessionIndex);
				if (!session)
					break;
				
				for (int32 partitionIndex = 0; ; partitionIndex++) {
					Partition *partition = session->PartitionAt(partitionIndex);
					if (!partition)
						break;
					if (partition->Mounted() == kMounted) {
						deviceHasMountedPartitions = true;
						break;
					}
				}
			}
			if (!deviceHasMountedPartitions && !params->dontEject)
				params->result = partition->GetDevice()->Eject();
		}
		
		return partition;
	}

#endif
	return NULL;
}

void
AutoMounter::UnmountAndEjectVolume(BMessage *_DEVICE_MAP_ONLY(message))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	dev_t device;
	if (message->FindInt32("device_id", &device) != B_OK)
		return;
	
	bool dontEject;
	if (message->FindBool("dont_eject", &dontEject) != B_OK)
		dontEject = false;

	PRINT(("Unmount device %i\n", device));
	
	AutoLock<BLooper> lock(this);

	UnmountDeviceParams params;
	params.device = device;
	params.dontEject = dontEject;
	params.result = B_OK;
	Partition *partition = fList.EachMountedPartition(UnmountIfMatchingID, 
		&params);
	
	if (!partition) {
		PRINT(("Couldn't unmount partition.  Rescan and try again\n"));

		// could not find partition - must have been mounted by someone
		// else
		// sync up and try again
		// this should really be handled by watching for mount and unmount
		// events like the tracker does, not doing that because it is
		// a bigger change and we are close to freezing
		fList.UnmountDisappearedPartitions();
		
		DeviceScanParams syncRescanParams;
		syncRescanParams.checkFloppies = true;
		syncRescanParams.checkCDROMs = true;
		syncRescanParams.checkOtherRemovable = true;
		syncRescanParams.removableOrUnknownOnly = true;
		
		fList.UpdateChangedDevices(&syncRescanParams);
		partition = fList.EachMountedPartition(UnmountIfMatchingID, &params);	
	}
	
	if (!partition) {
		PRINT(("Device not in list, unmounting directly\n"));

		char path[B_FILE_NAME_LENGTH];

		BVolume vol(device);
		status_t err = vol.InitCheck();
		if (err == B_OK) {
			BDirectory mountPoint;		
			if (err == B_OK)
				err = vol.GetRootDirectory(&mountPoint);		
	
			BPath mountPointPath;
			if (err == B_OK)
				err = mountPointPath.SetTo(&mountPoint, ".");
	
			if (err == B_OK)
				strcpy(path, mountPointPath.Path());	
		}

		if (err == B_OK) {
			PRINT(("unmounting '%s'\n", path));
			err = unmount(path);
		}
		
		if (err == B_OK) {
			PRINT(("deleting '%s'\n", path));
			err = rmdir(path);
		}

		if (err != B_OK) {
		
			PRINT(("error %s\n", strerror(err)));
			(new BAlert("", LOCALE("Could not unmount disk."), LOCALE("OK"), NULL, NULL, B_WIDTH_AS_USUAL, 
				B_WARNING_ALERT))->Go(0);
		}
		
	} else if (params.result != B_OK) {
		char buffer[1024];
		sprintf(buffer, LOCALE("Could not unmount disk %s. An item on the disk is busy."), partition->VolumeName());
		(new BAlert("", buffer, LOCALE("OK"), NULL, NULL, B_WIDTH_AS_USUAL, 
			B_WARNING_ALERT))->Go(0);
	}
#endif
}

bool
AutoMounter::QuitRequested()
{
#if _INCLUDES_CLASS_DEVICE_MAP
	if (!BootedInSafeMode())
		// don't write out settings in safe mode - this would overwrite the
		// normal, non-safe mode settings
		WriteSettings();

#endif
	return true;
}

void
AutoMounter::ReadSettings()
{
#if _INCLUDES_CLASS_DEVICE_MAP
	BPath directoryPath;

	if (TFSContext::GetTrackerSettingsDir(directoryPath) != B_OK)
		return;
	
	BPath path(directoryPath);
	path.Append(kAutoMounterSettings);
	fPrefsFile.SetTo(path.Path(), O_RDWR);
	
	if (fPrefsFile.InitCheck() != B_OK) {
		// no prefs file yet, create a new one

		BDirectory dir(directoryPath.Path());
		dir.CreateFile(kAutoMounterSettings, &fPrefsFile);
		return;
	}

	ssize_t settingsSize = (ssize_t)fPrefsFile.Seek(0, SEEK_END);
	if (settingsSize == 0)
		return;

	ASSERT(settingsSize != 0);
	char *buffer = new char[settingsSize];
	
	fPrefsFile.Seek(0, 0);
	if (fPrefsFile.Read(buffer, (size_t)settingsSize) != settingsSize) {
		PRINT(("error reading automounter settings\n"));
		delete [] buffer;
		return;
	}

	BMessage message('stng');
	status_t result = message.Unflatten(buffer);
	
	if (result != B_OK) {
		PRINT(("error %s unflattening settings, size %d\n", strerror(result), 
			settingsSize));
		delete [] buffer;
		return;
	}
	
	delete [] buffer;
	PRINT(("done unflattening settings\n"));
	SetParams(&message, true);
#endif
}

void
AutoMounter::WriteSettings()
{
#if _INCLUDES_CLASS_DEVICE_MAP
	if (fPrefsFile.InitCheck() != B_OK)
		return;

	BMessage message('stng');
	GetSettings(&message);
	
	ssize_t settingsSize = message.FlattenedSize();

	char *buffer = new char[settingsSize];
	status_t result = message.Flatten(buffer, settingsSize);
		
	fPrefsFile.Seek(0, 0);
	result = fPrefsFile.Write(buffer, (size_t)settingsSize);
	
	if (result != settingsSize)
		PRINT(("error writing settings, %s\n", strerror(result)));
	
	delete [] buffer;
#endif
}

void
AutoMounter::GetSettings(BMessage *_DEVICE_MAP_ONLY(message))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	message->AddBool("checkRemovableOnly", fScanParams.removableOrUnknownOnly);
	message->AddBool("checkCDs", fScanParams.checkCDROMs);
	message->AddBool("checkFloppies", fScanParams.checkFloppies);
	message->AddBool("checkOtherRemovables", fScanParams.checkOtherRemovable);
	message->AddBool("autoMountRemovableOnly", fAutomountParams.mountRemovableDisksOnly);
	message->AddBool("autoMountAll", fAutomountParams.mountAllFS);
	message->AddBool("autoMountAllBFS", fAutomountParams.mountBFS);
	message->AddBool("autoMountAllHFS", fAutomountParams.mountHFS);
	message->AddBool("initialMountAll", fInitialMountAll);
	message->AddBool("initialMountAllBFS", fInitialMountAllBFS);
	message->AddBool("initialMountRestore", fInitialMountRestore);
	message->AddBool("initialMountAllHFS", fInitialMountAllHFS);
	message->AddBool("suspended", fSuspended);

	// Save mounted volumes so we can optionally mount them on next
	// startup
	BVolumeRoster volumeRoster;
	BVolume volume;
	while (volumeRoster.GetNextVolume(&volume) == B_OK) {
        fs_info info;
        if (fs_stat_dev(volume.Device(), &info) == 0
			&& info.flags & (B_FS_IS_REMOVABLE | B_FS_IS_PERSISTENT))
			message->AddString(info.device_name, info.volume_name);
	}
#endif
}

void 
AutoMounter::SetParams(BMessage *_DEVICE_MAP_ONLY(message),
	bool _DEVICE_MAP_ONLY(rescan))
{
#if _INCLUDES_CLASS_DEVICE_MAP
	bool result;
	if (message->FindBool("checkRemovableOnly", &result) == B_OK)
		fScanParams.removableOrUnknownOnly = result;
	if (message->FindBool("checkCDs", &result) == B_OK)
		fScanParams.checkCDROMs = result;
	if (message->FindBool("checkFloppies", &result) == B_OK)
		fScanParams.checkFloppies = result;
	if (message->FindBool("checkOtherRemovables", &result) == B_OK)
		fScanParams.checkOtherRemovable = result;
	if (message->FindBool("autoMountRemovableOnly", &result) == B_OK)
		fAutomountParams.mountRemovableDisksOnly = result;
	if (message->FindBool("autoMountAll", &result) == B_OK)
		fAutomountParams.mountAllFS = result;
	if (message->FindBool("autoMountAllBFS", &result) == B_OK)
		fAutomountParams.mountBFS = result;
	if (message->FindBool("autoMountAllHFS", &result) == B_OK)
		fAutomountParams.mountHFS = result;
	if (message->FindBool("initialMountAll", &result) == B_OK)
		fInitialMountAll = result;
	if (message->FindBool("initialMountAllBFS", &result) == B_OK)
		fInitialMountAllBFS = result;
	if (message->FindBool("initialMountRestore", &result) == B_OK) {
		fInitialMountRestore = result;
		if (fInitialMountRestore)
			gSettingsMessage = *message;
	}
	if (message->FindBool("initialMountAllHFS", &result) == B_OK)
		fInitialMountAllHFS = result;

	if (message->FindBool("suspended", &result) == B_OK)
		SuspendResume(result);
	
	if (rescan)
		CheckVolumesNow();
#endif
}
