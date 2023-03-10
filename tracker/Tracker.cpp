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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fs_attr.h>
#include <fs_info.h>
#include <Alert.h>
#include <Debug.h>
#include <MenuItem.h>
#include <image.h>
#include <Path.h>
#include <NodeInfo.h>
#include <FindDirectory.h>
#include <StopWatch.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <NodeMonitor.h>
#include <Autolock.h>
#include <Roster.h>

#include <memory>

#include "BackgroundImage.h"
#include "Bitmaps.h"
#include "Attributes.h"
#include "AutoLock.h"
#include "AutoMounter.h"
#include "AutoMounterSettings.h"
#include "MimeTypes.h"
#include "Commands.h"
#include "ContainerWindow.h"
#include "QueryContainerWindow.h"
#include "FindPanel.h"
#include "VolumeWindow.h"
#include "PoseView.h"
#include "OpenWithWindow.h"
#include "InfoWindow.h"
#include "LanguageTheme.h"
#include "FSStatusWindow.h"
#include "MimeTypeList.h"
#include "NodePreloader.h"
#include "DeskWindow.h"
#include "TFSContext.h"
#include "FSClipboard.h"
#include "FSUtils.h"
#include "Undo.h"
#include "Utilities.h"
#include "Tracker.h"
#include "Settings.h"
#include "TrackerSettings.h"
#include "TrackerSettingsWindow.h"
#include "TrashWatcher.h"
#include "FunctionObject.h"
#include "TaskLoop.h"
#include "ThreadMagic.h"

// PPC binary compatibility.
#include "AboutBox.cpp"

// prototypes for some private kernel calls that will some day be public
extern "C" _IMPEXP_ROOT int _kset_fd_limit_(int num);
extern "C" _IMPEXP_ROOT int _kset_mon_limit_(int num);
	// from priv_syscalls.h

const int32 DEFAULT_MON_NUM = 4096;
	// copied from fsil.c

const int8 kOpenWindowNoFlags = 0;
const int8 kOpenWindowMinimized = 1;
const int8 kOpenWindowHasState = 2;

const uint32 PSV_MAKE_PRINTER_ACTIVE_QUIETLY = 'pmaq';
	// from pr_server.h

namespace BPrivate {

NodePreloader *gPreloader = NULL;

#ifndef kDeskbarSignature // TODO: Why do we need this?
#define kDeskbarSignature "application/x-vnd.Be-TSKB"
#endif

void 
InitIconPreloader()
{
	static int32 lock = 0;

	if (atomic_add(&lock,1) != 0) {
		// Just wait for the icon cache to be instatiated
		int32 tries = 20;	
		while (IconCache::sIconCache == NULL && tries-- > 0)
			snooze(10000);
		return;
	}

	if (IconCache::sIconCache != NULL)
		return;
		
	// only start the node preloader if its Tracker or the Deskbar itself - don't
	// start it for file panels
	
	bool preload = dynamic_cast<TTracker*>(be_app) != NULL;
	if (!preload) {
		// check for Deskbar
		app_info info;
		if (be_app->GetAppInfo(&info) == B_OK
			&& !strcmp(info.signature, kDeskbarSignature))
			preload = true;
	}
	if (preload)
		gPreloader = NodePreloader::InstallNodePreloader("NodePreloader", be_app);

	IconCache::sIconCache = new IconCache();
	
	atomic_add(&lock, -1);
}

}	// namespace BPrivate

uint32
GetVolumeFlags(Model *model)
{
	fs_info info;
	if (model->IsVolume()) {
		// search for the correct volume
		int32 cookie = 0;
		dev_t device;
		while ((device = next_dev(&cookie)) >= B_OK) {
			if (fs_stat_dev(device,&info))
				continue;

			if (!strcmp(info.volume_name,model->Name()))
				return info.flags;
		}
		return B_FS_HAS_ATTR;
	}
	if (!fs_stat_dev(model->NodeRef()->device,&info))
		return info.flags;
	
	return B_FS_HAS_ATTR;
}

static void
HideVarDir()
{
	BPath path;
	status_t err = find_directory(B_COMMON_VAR_DIRECTORY, &path);
	
	if (err != B_OK){
		PRINT(("var err = %s\n", strerror(err)));
		return;
	}

	BDirectory varDirectory(path.Path());
	if (varDirectory.InitCheck() == B_OK) {
		PoseInfo info;
		// make var dir invisible
		info.fInvisible = true;
		info.fInitedDirectory = -1;
		
		if (varDirectory.WriteAttr(kAttrPoseInfo, B_RAW_TYPE, 0, &info, sizeof(info))
			== sizeof(info))
			varDirectory.RemoveAttr(kAttrPoseInfoForeign);
	}
}

TTracker::TTracker()
	:	BApplication(kTrackerSignature),
		fSettingsWindow(NULL)
{
	// set the cwd to /boot/home, anything that's launched 
	// from Tracker will automatically inherit this 
	BPath homePath;
	
	if (find_directory(B_USER_DIRECTORY, &homePath) == B_OK)
		chdir(homePath.Path());
	
	_kset_fd_limit_(512);
		// ask for a bunch more file descriptors so that nested copying
		// works well
	
	fNodeMonitorCount = DEFAULT_MON_NUM;

#ifdef CHECK_OPEN_MODEL_LEAKS
	InitOpenModelDumping();
#endif
	
	// initialize globals
	gLanguageTheme = new LanguageTheme();
	InitIconPreloader();

#ifdef LEAK_CHECKING
	SetNewLeakChecking(true);
	SetMallocLeakChecking(true);
#endif

	//This is how often it should update the free space bar on the volume icons
	SetPulseRate(1000000);
}

TTracker::~TTracker()
{
}

bool
TTracker::QuitRequested()
{
	// don't allow user quitting
	if (CurrentMessage() && CurrentMessage()->FindBool("shortcut"))
		return false;

	gStatusWindow().AttemptToQuit();
		// try to cancel any pending operation
				
	BMessage message;
	AutoLock<WindowList> lock(&fWindowList);
	// save open windows in a message inside an attribute of the desktop
	int32 count = fWindowList.CountItems();
	for (int32 i = 0; i < count; i++) {
		BContainerWindow *window = dynamic_cast<BContainerWindow *>
			(fWindowList.ItemAt(i));

		if (window && window->TargetModel()) {
			if (window->TargetModel()->IsRoot())
				message.AddBool("open_disks_window", true);
			else if (window->TargetModel()->IsVolume())
				message.AddBool("open_root_directory", true);
			else {
				BEntry entry;
				BPath path;
				if (entry.SetTo(window->TargetModel()->EntryRef()) == B_OK
					&& entry.GetPath(&path) == B_OK) {
					if (window != FindContainerWindow(window->TargetModel()->EntryRef())) {
						BMessage stateMessage;
						window->SaveState(stateMessage);
						window->SetSaveStateEnabled(false);
							// This is to prevent its state to be saved to the node when closed.
						message.AddMessage("window state", &stateMessage);
					}
					message.AddString("paths", path.Path());
					message.AddBool(path.Path(), window->IsMinimized());	
				}
			}	
		}
	}
	lock.Unlock();

	BDirectory deskDir;
	if (!BootedInSafeMode()
		&& message.CountNames(B_ANY_TYPE) 
		&& TFSContext::GetBootDesktopDir(deskDir) == B_OK) {
		size_t size = (size_t)message.FlattenedSize();
		char *buffer = new char[size];
		message.Flatten(buffer, (ssize_t)size);
		deskDir.WriteAttr(kAttrOpenWindows, B_MESSAGE_TYPE, 0, buffer, size);
		delete [] buffer;
	}

	for (int32 count = 0; count == 50; count++) {
		// wait 5 seconds for the copiing/moving to quit
		if (gStatusWindow().AttemptToQuit())
			break;

		snooze(100000);
	}

	return _inherited::QuitRequested();
}

NodePreloader *gPreloader = NULL;

void
TTracker::Quit()
{
	gTrackerSettings.SaveSettings(false);
	
	fAutoMounter->Lock();
	fAutoMounter->QuitRequested();	// automounter does some stuff in QuitRequested
	fAutoMounter->Quit();			// but we really don't care if it is cooperating or not

	fClipboardRefsWatcher->Lock();
	fClipboardRefsWatcher->Quit();

	fTrashWatcher->Lock();
	fTrashWatcher->Quit();

	WellKnowEntryList::Quit();
	
	delete gPreloader;
	delete fTaskLoop;
	delete IconCache::sIconCache;

	_inherited::Quit();
}

void
TTracker::MessageReceived(BMessage *message)
{
	if (HandleScriptingMessage(message))
		return;

	switch (message->what) {
		case kGetInfo:
			OpenInfoWindows(message);
			break;
		
		case kCloseInfo:
			CloseInfoWindows(message);
			break;

		case kMoveToTrash:
			MoveRefsToTrash(message);
			break;

		case kCloseWindowAndChildren:
			{
				const node_ref *itemNode;
				int32 bytes;
				message->FindData("node_ref", B_RAW_TYPE,
					(const void **)&itemNode, &bytes);
				CloseWindowAndChildren(itemNode);
				break;
			}
			
		case kCloseAllWindows:
			CloseAllWindows();
			break;

		case kFindButton:
			(new FindWindow())->Show();
			break;

		case kEditQuery:
			EditQueries(message);
			break;

		case kUnmountVolume:
			//	When the user attempts to unmount a volume from the mount
			//	context menu, this is where the message gets received.  Save
			//	pose locations and forward this to the automounter
			SaveAllPoseLocations();
			fAutoMounter->PostMessage(message);
			break;

		case kRunAutomounterSettings:
			AutomountSettingsDialog::RunAutomountSettings(fAutoMounter);
			break;

		case kShowSplash:
			{
				// The AboutWindow was moved out of the Tracker in preparation
				// for when we open source it. The AboutBox contains important
				// credit and license issues that shouldn't be modified, and
				// therefore shouldn't be open sourced. However, there is a public
				// API for 3rd party apps to tell the Tracker to open the AboutBox.
				run_be_about();
				break;
			}

		case kAddPrinter:
			// show the addprinter window
			run_add_printer_panel();
			break;
			
		case kMakeActivePrinter:
			// get the current selection
			SetDefaultPrinter(message);
			break;

#ifdef MOUNT_MENU_IN_DESKBAR
		case 'gmtv':
			{
				// Someone (probably the deskbar) has requested a list of
				// mountable volumes.
				BMessage reply;
				AutoMounterLoop()->EachMountableItemAndFloppy(&AddMountableItemToMessage,
				  &reply);
				message->SendReply(&reply);
				break;
			}

#endif

		case kMountVolume:
		case kMountAllNow:
			AutoMounterLoop()->PostMessage(message);
			break;


		case kRestoreBackgroundImage: {
				BDeskWindow *desktop = GetDeskWindow();
				AutoLock<BWindow> lock(desktop);
				desktop->UpdateDesktopBackgroundImages();
			}
			break;

 		case kShowSettingsWindow:
 			ShowSettingsWindow();
 			break;

		case kFavoriteCountChangedExternally:
			SendNotices(kFavoriteCountChangedExternally, message);
			break;

		case kStartWatchClipboardRefs: {
			BMessenger messenger;
			message->FindMessenger("target", &messenger);
			if (messenger.IsValid())
				fClipboardRefsWatcher->AddToNotifyList(messenger);
			break;
		}

		case kStopWatchClipboardRefs: {
			BMessenger messenger;
			message->FindMessenger("target", &messenger);
			if (messenger.IsValid())
				fClipboardRefsWatcher->RemoveFromNotifyList(messenger);
			break;
		}
		
		case kFSClipboardChanges:
			fClipboardRefsWatcher->UpdatePoseViews(message);
			break;
		
		case kUndoAction:
			if (gTrackerSettings.UndoEnabled())
				gUndoHistory.Undo(1);
			break;
		
		case kRedoAction:
			if (gTrackerSettings.UndoEnabled())
				gUndoHistory.Redo(1);
			break;

		case kShowVolumeSpaceBar:
		case kSpaceBarColorChanged: {
			gPeriodicUpdatePoses.DoPeriodicUpdate(true);
			break;
		}

		default:
			_inherited::MessageReceived(message);
			break;
	}
}

void
TTracker::Pulse()
{
	if (!gTrackerSettings.ShowVolumeSpaceBar())
		return;

	// update the volume icon's free space bars
	gPeriodicUpdatePoses.DoPeriodicUpdate(false);
}

void
TTracker::SetDefaultPrinter(const BMessage *message)
{
	//	get the first item selected
	int32 count = 0;
	uint32 type = 0;
	message->GetInfo("refs", &type, &count);

	if (count <= 0)
		return;

	//	will make the first item the default printer, disregards any other files
	entry_ref ref;
	ASSERT(message->FindRef("refs", 0, &ref) == B_OK);
	if (message->FindRef("refs", 0, &ref) != B_OK)
		return;

#if B_BEOS_VERSION_DANO
	set_default_printer(ref.name);
#else
	// 	create a message for the print server
	BMessenger messenger("application/x-vnd.Be-PSRV", -1);
	if (!messenger.IsValid())
		return;

	//	send the selection to the print server
	BMessage makeActiveMessage(PSV_MAKE_PRINTER_ACTIVE_QUIETLY);
	makeActiveMessage.AddString("printer", ref.name);

	BMessage reply;
	messenger.SendMessage(&makeActiveMessage, &reply);
#endif
}

void
TTracker::MoveRefsToTrash(const BMessage *message)
{
	int32 count;
	uint32 type;
	message->GetInfo("refs", &type, &count);

	if (count <= 0)
		return;

	auto_ptr<TFSContext> tfscontext(new TFSContext());
	
	back_insert_iterator<fs::EntryList> bii(tfscontext->EntryList());

	for (int32 index = 0; index < count; index++) {

		entry_ref ref;
		ASSERT(message->FindRef("refs", index, &ref) == B_OK);
		if (message->FindRef("refs", index, &ref) != B_OK)
			continue;

		AutoLock<WindowList> lock(&fWindowList);
		BContainerWindow *window = FindParentContainerWindow(&ref);
		if (window)
			// if we have a window open for this entry, ask the pose to
			// delete it, this will select the next entry
			window->PoseView()->MoveEntryToTrash(&ref);
		else {
			// add all others to a list that gets deleted separately
			*bii = ref;
			++bii;
		}
	}

	if (tfscontext->EntryList().size() > 0)
		// async move to trash
		tfscontext.release()->MoveToTrash(true);
}

template <class T, class FT>
class EntryAndNodeDoSoonWithMessageFunctor : public FunctionObjectWithResult<bool> {
public:
	EntryAndNodeDoSoonWithMessageFunctor(FT func, T *target, const entry_ref *child,
		const node_ref *parent, const BMessage *message)
		:	fFunc(func),
			fTarget(target),
			fNode(*parent),
			fEntry(*child)
		{
			fSendMessage = (message != NULL);
			if (message)
				fMessage = *message;
		}

	virtual ~EntryAndNodeDoSoonWithMessageFunctor() {}
	virtual void operator()()
		{ mResult = (fTarget->*fFunc)(&fEntry, &fNode, fSendMessage ? &fMessage : NULL); }

protected:
	FT fFunc;
	T *fTarget;
	node_ref fNode;
	entry_ref fEntry;
	BMessage fMessage;
	bool fSendMessage;
};

bool 
TTracker::LaunchAndCloseParentIfOK(const entry_ref *launchThis,
	const node_ref *closeThis, const BMessage *messageToBundle)
{
	BMessage refsReceived(B_REFS_RECEIVED);
	if (messageToBundle) {
		refsReceived = *messageToBundle;
		refsReceived.what = B_REFS_RECEIVED;
	}
	refsReceived.AddRef("refs", launchThis);	
	// synchronous launch, we are already in our own thread
	if (TrackerLaunch(&refsReceived, false) == B_OK) {
		// if launched fine, close parent window in a bit
		fTaskLoop->RunLater(NewFunctionObject(this, &TTracker::CloseParent, *closeThis),
			1000000);
	}
	return false;
}

status_t
TTracker::OpenRef(const entry_ref *ref, const node_ref *nodeToClose,
	const node_ref *nodeToSelect, OpenSelector selector,
	const BMessage *messageToBundle)
{
	Model *model = NULL;
	BEntry entry(ref, true);
	status_t result = entry.InitCheck();

	bool brokenLinkWithSpecificHandler = false;
	BString brokenLikPreferredApp;

	if (result != B_OK) {
		model = new Model(ref, false);
		if (model->IsSymLink() && !model->LinkTo()) {
			model->GetPreferredAppForBrokenSymLink(brokenLikPreferredApp);
			if (brokenLikPreferredApp.Length() && brokenLikPreferredApp != kTrackerSignature)
				brokenLinkWithSpecificHandler = true;
		}
		
		if (!brokenLinkWithSpecificHandler) {
			delete model;
			(new BAlert("", LOCALE("There was an error resolving the link."),
				LOCALE("Cancel"), 0, 0,
				B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go();
			return result;
		}
	} else 
		model = new Model(&entry);

	result = model->InitCheck();
	if (result != B_OK) {
		delete model;
		return result;
	}

	bool openAsContainer = model->IsContainer();
	
	if (openAsContainer && selector != kOpenWith) {
		// if folder or query has a preferred handler and it's not the
		// Tracker, open it by sending refs to the handling app
		
		// if we are responding to the final open of OpenWith, just
		// skip this and proceed to opening the container with Tracker
		model->OpenNode();
		BNodeInfo nodeInfo(model->Node());
		char preferredApp[B_MIME_TYPE_LENGTH];
		if (nodeInfo.GetPreferredApp(preferredApp) == B_OK
			&& strcasecmp(preferredApp, kTrackerSignature) != 0)
			openAsContainer = false;
		model->CloseNode();
	}

	if (openAsContainer || selector == kRunOpenWithWindow) {
		// special case opening plain folders, queries or using open with
		OpenContainerWindow(model, 0, selector);	// window adopts model
		if (nodeToClose)
			CloseParentWaitingForChildSoon(ref, nodeToClose);
	} else if (model->IsQueryTemplate()) {
		// query template - open new find window
		(new FindWindow(model->EntryRef()))->Show();
		if (nodeToClose)
			CloseParentWaitingForChildSoon(ref, nodeToClose);
	} else {
		delete model;
		// run Launch in a separate thread
		// and close parent if successfull
		if (nodeToClose)
			Thread::Launch(new EntryAndNodeDoSoonWithMessageFunctor<TTracker,
				bool (TTracker::*)(const entry_ref *, const node_ref *,
				const BMessage *)>(&TTracker::LaunchAndCloseParentIfOK, this,
				ref, nodeToClose, messageToBundle));
		else {
			BMessage refsReceived(B_REFS_RECEIVED);
			if (messageToBundle) {
				refsReceived = *messageToBundle;
				refsReceived.what = B_REFS_RECEIVED;
			}
			refsReceived.AddRef("refs", ref);
			if (brokenLinkWithSpecificHandler)
				// This cruft is to support a hacky workaround for double-clicking
				// broken refs for cifs; should get fixed in R5
				LaunchBrokenLink(brokenLikPreferredApp.String(), &refsReceived);
			else
				TrackerLaunch(&refsReceived, true);
		}
	}
	if (nodeToSelect)
		SelectChildInParentSoon(ref, nodeToSelect);

	return B_OK;
}

void
TTracker::RefsReceived(BMessage *message)
{
	OpenSelector selector = kOpen;
	if (message->HasInt32("launchUsingSelector"))
		selector = kRunOpenWithWindow;
	
	entry_ref handlingApp;
	if (message->FindRef("handler", &handlingApp) == B_OK)
		selector = kOpenWith;

	int32 count;
	uint32 type;
	message->GetInfo("refs", &type, &count);
	
	switch (selector) {
		case kRunOpenWithWindow:
			OpenContainerWindow(0, message, selector);
				// window adopts model
			break;

		case kOpenWith:
			{
				// Open With resulted in passing refs and a handler, open the files
				// with the handling app
				message->RemoveName("handler");
				
				// have to find out if handling app is the Tracker
				// if it is, just pass it to the active Tracker, no matter which Tracker
				// was chosen to handle the refs
				char signature[B_MIME_TYPE_LENGTH];
				signature[0] = '\0';
				{
					BFile handlingNode(&handlingApp, O_RDONLY);
					BAppFileInfo appInfo(&handlingNode);
					appInfo.GetSignature(signature);
				}
	
				if (strcasecmp(signature, kTrackerSignature) != 0) {
					// handling app not Tracker, pass entries to the apps RefsReceived
					TrackerLaunch(&handlingApp, message, true);
					break;
				}
				// fall thru, opening refs by the Tracker, as if they were double clicked
			}

		case kOpen:
			{
				// copy over "Poses" messenger so that refs received recipients know
				// where the open came from
				BMessage *bundleThis = NULL;
				BMessenger messenger;
				if (message->FindMessenger("TrackerViewToken", &messenger) == B_OK) {
					bundleThis = new BMessage();
					bundleThis->AddMessenger("TrackerViewToken", messenger);
				}
	
				for (int32 index = 0; index < count; index++) {
					entry_ref ref;
					message->FindRef("refs", index, &ref);
			
					const node_ref *nodeToClose = NULL;
					const node_ref *nodeToSelect = NULL;
					ssize_t numBytes;
		
					message->FindData("nodeRefsToClose", B_RAW_TYPE, index,
						(const void **)&nodeToClose, &numBytes);
					message->FindData("nodeRefToSelect", B_RAW_TYPE, index,
						(const void **)&nodeToSelect, &numBytes);
			
					OpenRef(&ref, nodeToClose, nodeToSelect, selector, bundleThis);
				}
	
				delete bundleThis;
				break;
			}
	}
}

void
TTracker::ArgvReceived(int32 argc, char **argv)
{
	BMessage *message = CurrentMessage();
	const char *currentWorkingDirectoryPath = NULL;
	entry_ref ref;

	if (message->FindString("cwd", &currentWorkingDirectoryPath) == B_OK) {
		BDirectory workingDirectory(currentWorkingDirectoryPath);
		for (int32 index = 1; index < argc; index++) {
			BEntry entry;
			if (entry.SetTo(&workingDirectory, argv[index]) == B_OK
				&& entry.GetRef(&ref) == B_OK) 
				OpenRef(&ref);
			else if (get_ref_for_path(argv[index], &ref) == B_OK)
				OpenRef(&ref);
		}
	}
}

void
TTracker::OpenContainerWindow(Model *model, BMessage *originalRefsList,
	OpenSelector openSelector, uint32 openFlags, bool checkAlreadyOpen,
	const BMessage *stateMessage)
{
	AutoLock<WindowList> lock(&fWindowList);
	BContainerWindow *window = NULL;
	if (checkAlreadyOpen && openSelector != kRunOpenWithWindow)
		// find out if window already open
		window = FindContainerWindow(model->NodeRef());

	if (dynamic_cast<BDeskWindow*>(window) != NULL || modifiers() & B_SHIFT_KEY)
		window = NULL;
		
	bool someWindowActivated = false;
	
	uint32 workspace = (uint32)(1 << current_workspace());		
	int32 windowCount = 0;
	
	while (window) {
		// At least one window open, just pull to front
		// make sure we don't jerk workspaces around		
		uint32 windowWorkspaces = window->Workspaces();
		if (windowWorkspaces & workspace) {
			window->Activate();
			someWindowActivated = true;
		}
		window = FindContainerWindow(model->NodeRef(), ++windowCount);
	}
	
	if (someWindowActivated) {
		delete model;
		return;	
	} // If no window was actiated, (none in the current workspace
	  // we open a new one.
	
	if (openSelector == kRunOpenWithWindow) {
		BMessage *refList = NULL;
		if (!originalRefsList) {
			// when passing just a single model, stuff it's entry in a single
			// element list anyway
			ASSERT(model);
			refList = new BMessage;
			refList->AddRef("refs", model->EntryRef());
			delete model;
			model = NULL;
		} else
			// clone the message, window adopts it for it's own use
			refList = new BMessage(*originalRefsList);
		window = new OpenWithContainerWindow(refList, &fWindowList);
	/*} else if (model->IsRoot()) { // BVolumeWindow should not be used!
		// window will adopt the model
		window = new BVolumeWindow(&fWindowList, openFlags);*/
	} else if (model->IsQuery()) {
		// window will adopt the model
		window = new BQueryContainerWindow(&fWindowList, openFlags);
	} else
		// window will adopt the model
		window = new BContainerWindow(&fWindowList, openFlags);
	
	if (model)
		window->CreatePoseView(model);

	BMessage restoreStateMessage(kRestoreState);
	
	if (stateMessage)
		restoreStateMessage.AddMessage("state", stateMessage);

	window->PostMessage(&restoreStateMessage);
}

void
TTracker::EditQueries(const BMessage *message)
{
	bool editOnlyIfTemplate;
	if (message->FindBool("editQueryOnPose", &editOnlyIfTemplate) != B_OK)
		editOnlyIfTemplate = false;

	type_code type;
	int32 count;
	message->GetInfo("refs", &type, &count);
	for (int32 index = 0; index < count; index++) {
		entry_ref ref;
		message->FindRef("refs", index, &ref);
		BEntry entry(&ref, true);
		if (entry.InitCheck() == B_OK && entry.Exists()) 
			(new FindWindow(&ref, editOnlyIfTemplate))->Show();

	}
}

void
TTracker::OpenInfoWindows(BMessage *message)
{
	type_code type;
	int32 count;
	message->GetInfo("refs", &type, &count);
	if (count < 1)
		return;
	
	bool multiple = true;
	
	// show info-windows for every single file if it's about only one file
	// or if we explicitly disable the multiple-file-info
	if (count == 1 || (message->FindBool("multiple", &multiple) == B_OK
		&& multiple == false)) {
		for (int32 index = 0; index < count; index++) {
			entry_ref ref;
			message->FindRef("refs", index, &ref);
			
			BEntry entry;
			if (entry.SetTo(&ref) == B_OK) {
				Model *model = new Model(&entry);
				if (model->InitCheck() != B_OK) {
					delete model;
					continue;
				}
				
				AutoLock<WindowList> lock(&fWindowList);
				BInfoWindow *wind = FindInfoWindow(model->NodeRef());
				
				if (wind) {
					wind->Activate();
					delete model;
				} else {
					wind = new BInfoWindow(model, index, &fWindowList);
					wind->PostMessage(kRestoreState);
				}
			}
		}
	} else {
		BInfoWindow *wind = new BInfoWindow(message, 0);
		wind->PostMessage(kRestoreState);
	}
}

void
TTracker::CloseInfoWindows(BMessage *message)
{
	type_code type;
	int32 count;
	message->GetInfo("refs", &type, &count);


	for (int32 index = 0; index < count; index++) {
		entry_ref ref;
		message->FindRef("refs", index, &ref);
		BEntry entry;
		if (entry.SetTo(&ref) == B_OK) {
			node_ref nref;
			entry.GetNodeRef(&nref);
			
			BInfoWindow *wind = FindInfoWindow(&nref);
			if (wind) {
				wind->Lock();
				wind->Close();
			}
		}
	}
}

BDeskWindow *
TTracker::GetDeskWindow() const
{
	int32 count = fWindowList.CountItems();
	for (int32 index = 0; index < count; index++) {
		BDeskWindow *window = dynamic_cast<BDeskWindow *>
			(fWindowList.ItemAt(index));

		if (window)
			return window;
	}
	TRESPASS();
	return NULL;
}

bool
TTracker::LockWindowList()
{
	return fWindowList.Lock();
}

void
TTracker::UnlockWindowList()
{
	fWindowList.Unlock();
}

BContainerWindow *
TTracker::FindContainerWindow(const node_ref *node, int32 number) const
{
	ASSERT(fWindowList.IsLocked());
	
	int32 count = fWindowList.CountItems();

	int32 windowsFound = 0;

	for (int32 index = 0; index < count; index++) {
		BContainerWindow *window = dynamic_cast<BContainerWindow *>
			(fWindowList.ItemAt(index));
			
		if (window && window->IsShowing(node) && number == windowsFound++)
			return window;
	}
	return NULL;
}

BContainerWindow *
TTracker::FindContainerWindow(const entry_ref *entry, int32 number) const
{
	ASSERT(fWindowList.IsLocked());
	
	int32 count = fWindowList.CountItems();

	int32 windowsFound = 0;

	for (int32 index = 0; index < count; index++) {
		BContainerWindow *window = dynamic_cast<BContainerWindow *>
			(fWindowList.ItemAt(index));

		if (window && window->IsShowing(entry) && number == windowsFound++)
			return window;
	}
	return NULL;
}

bool 
TTracker::EntryHasWindowOpen(const entry_ref *entry)
{
	AutoLock<WindowList> lock(&fWindowList);
	return FindContainerWindow(entry) != NULL;
}


BContainerWindow *
TTracker::FindParentContainerWindow(const entry_ref *ref) const
{
	BEntry entry(ref);
	BEntry parent;
	
	if (entry.GetParent(&parent) != B_OK)
		return NULL;
	
	entry_ref parentRef;
	parent.GetRef(&parentRef);

	ASSERT(fWindowList.IsLocked());
	
	int32 count = fWindowList.CountItems();
	for (int32 index = 0; index < count; index++) {
		BContainerWindow *window = dynamic_cast<BContainerWindow *>
			(fWindowList.ItemAt(index));
		if (window && window->IsShowing(&parentRef))
			return window;
	}
	return NULL;
}

BInfoWindow *
TTracker::FindInfoWindow(const node_ref* node) const
{
	ASSERT(fWindowList.IsLocked());
	
	int32 count = fWindowList.CountItems();
	for (int32 index = 0; index < count; index++) {
		BInfoWindow *window = dynamic_cast<BInfoWindow *>
			(fWindowList.ItemAt(index));
		if (window && window->IsShowing(node))
			return window;
	}
	return NULL;
}

bool 
TTracker::QueryActiveForDevice(dev_t device)
{
	AutoLock<WindowList> lock(&fWindowList);
	int32 count = fWindowList.CountItems();
	for (int32 index = 0; index < count; index++) {
		BQueryContainerWindow *window = dynamic_cast<BQueryContainerWindow *>
			(fWindowList.ItemAt(index));
		if (window) {
			AutoLock<BWindow> lock(window);
			if (window->ActiveOnDevice(device))
				return true;
		}
	}
	return false;
}

void 
TTracker::CloseActiveQueryWindows(dev_t device)
{
	// used when trying to unmount a volume - an active query would prevent that from
	// happening
	bool closed = false;
	AutoLock<WindowList> lock(fWindowList);
	for (int32 index = fWindowList.CountItems(); index >= 0; index--) {
		BQueryContainerWindow *window = dynamic_cast<BQueryContainerWindow *>
			(fWindowList.ItemAt(index));
		if (window) {
			AutoLock<BWindow> lock(window);
			if (window->ActiveOnDevice(device)) {
				window->PostMessage(B_QUIT_REQUESTED);
				closed = true;
			}
		}
	}
	lock.Unlock();
	if (closed)
		for (int32 timeout = 30; timeout; timeout--) {
			// wait a bit for windows to fully close
			if (!QueryActiveForDevice(device))
				return;
			snooze(100000);
		}
}

void
TTracker::SaveAllPoseLocations()
{
	int32 numWindows = fWindowList.CountItems();
	for (int32 windowIndex = 0; windowIndex < numWindows; windowIndex++) {
		BContainerWindow *window = dynamic_cast<BContainerWindow *>
			(fWindowList.ItemAt(windowIndex));

		if (window) {
			AutoLock<BWindow> lock(window);
			BDeskWindow *deskWindow = dynamic_cast<BDeskWindow *>(window);
		
			if (deskWindow) 
				deskWindow->SaveDesktopPoseLocations();
			else
				window->PoseView()->SavePoseLocations();
		}
	}
}


void
TTracker::CloseWindowAndChildren(const node_ref *node)
{
	BDirectory dir(node);
	if (dir.InitCheck() != B_OK)
		return;

	AutoLock<WindowList> lock(&fWindowList);
	BObjectList<BContainerWindow> closeList;

	// make a list of all windows to be closed
	// count from end to beginning so we can remove items safely
	for (int32 index = fWindowList.CountItems() - 1; index >= 0; index--) {
		BContainerWindow *window = dynamic_cast<BContainerWindow *>
			(fWindowList.ItemAt(index));
		if (window && window->TargetModel()) {
			BEntry wind_entry;
			wind_entry.SetTo(window->TargetModel()->EntryRef());

			if ((*window->TargetModel()->NodeRef() == *node)
				|| dir.Contains(&wind_entry)) {
				
				// ToDo:
				// get rid of the Remove here, BContainerWindow::Quit does it
				fWindowList.RemoveItemAt(index);
				closeList.AddItem(window);
			}
		}
	}

	// now really close the windows
	int32 numItems = closeList.CountItems();
	for (int32 index = 0; index < numItems; index++) {
		BContainerWindow *window = closeList.ItemAt(index);
		window->PostMessage(B_QUIT_REQUESTED);
	}
}

void
TTracker::CloseAllWindows()
{
	// this is a response to the DeskBar sending us a B_QUIT, when it really
	// means to say close all your windows. It might be better to have it
	// send a kCloseAllWindows message and have windowless apps stay running,
	// which is what we will do for the Tracker
	AutoLock<WindowList> lock(&fWindowList);

	int32 count = CountWindows();
	for (int32 index = 0; index < count; index++) {
		BWindow *window = WindowAt(index);
		// avoid the desktop
		if (!dynamic_cast<BDeskWindow *>(window)
			&& !dynamic_cast<FSStatusWindow *>(window))
			window->PostMessage(B_QUIT_REQUESTED);
	}
	// count from end to beginning so we can remove items safely
	for (int32 index = fWindowList.CountItems() - 1; index >= 0; index--) {
		BWindow *window = fWindowList.ItemAt(index);
		if (!dynamic_cast<BDeskWindow *>(window)
			&& !dynamic_cast<FSStatusWindow *>(window))
				// ToDo:
				// get rid of the Remove here, BContainerWindow::Quit does it
			fWindowList.RemoveItemAt(index);
	}	
}

void
TTracker::ReadyToRun()
{
	// create fakewindow for OpenTracker compatibility
	// needs to be created at index 0
	BWindow *fakewindow = new BWindow(BRect(200, 200, 550, 233), "StatusWindow",
										B_UNTYPED_WINDOW, 0);
	fakewindow->Hide();
	
	InitMimeTypes();
	InstallDefaultTemplates();
	InstallIndices();
	
	HideVarDir();

	fTrashWatcher = new BTrashWatcher();
	fTrashWatcher->Run();

	fClipboardRefsWatcher = new BClipboardRefsWatcher();
	fClipboardRefsWatcher->Run();

	fAutoMounter = new AutoMounter();
	fAutoMounter->Run();
	
	fTaskLoop = new StandAloneTaskLoop(true);

	bool openDisksWindow = false;

	// open desktop window 
	BContainerWindow *deskWindow = NULL;
	BDirectory deskDir;
	if (TFSContext::GetBootDesktopDir(deskDir) == B_OK) {
		BEntry entry;
		deskDir.GetEntry(&entry);
		BPath deskPath;
		entry.GetPath(&deskPath);
		Model *model = new Model(&entry);
		if (model->InitCheck() == B_OK) {
			AutoLock<WindowList> lock(&fWindowList);
			deskWindow = new BDeskWindow(&fWindowList);
			AutoLock<BWindow> windowLock(deskWindow);
			deskWindow->CreatePoseView(model);
			deskWindow->Init();
		} else
			delete model;

		// open previously open windows
		attr_info attrInfo;
		if (!BootedInSafeMode()
			&& deskDir.GetAttrInfo(kAttrOpenWindows, &attrInfo) == B_OK) {
			char *buffer = (char *)malloc((size_t)attrInfo.size);
			BMessage message;
			if (deskDir.ReadAttr(kAttrOpenWindows, B_MESSAGE_TYPE, 0, buffer, (size_t)attrInfo.size)
				== attrInfo.size
				&& message.Unflatten(buffer) == B_OK) {

				node_ref nodeRef;
				deskDir.GetNodeRef(&nodeRef);
	
				const char *path;
				bool hideWindow = false;
				int32 messageCounter = 0;
				for (int32 index = 0; message.FindString("paths", index, &path) == B_OK;
					index++) {
					// Don't doubleopen the Desktop
					if (BString(path).Compare(deskPath.Path()) == 0)
						continue;

					message.FindBool(path, &hideWindow);
					BEntry entry(path, true);
					if (entry.InitCheck() == B_OK) {
						entry_ref ref;
						entry.GetRef(&ref);
						
						AutoLock<WindowList> lock(&fWindowList);
						BContainerWindow *window = FindContainerWindow(&ref);
						
						bool retrieveStateMessage = window != NULL
							&& ! window->PoseView()->IsDesktopWindow();
							
						Model *model = new Model(&entry);
						if (model->InitCheck() == B_OK && model->IsContainer()) {
							BMessage state;
							bool restoreStateFromMessage = false;
							if (retrieveStateMessage)
								if (message.FindMessage("window state", messageCounter++, &state)
									== B_OK)
									restoreStateFromMessage = true;

							if (restoreStateFromMessage)
								OpenContainerWindow(model, 0, kOpen, 
									kRestoreWorkspace | (hideWindow ? kIsHidden : 0U),
									false, &state);
							else
								OpenContainerWindow(model, 0, kOpen, 
									kRestoreWorkspace | (hideWindow ? kIsHidden : 0U));
						} else
							delete model;
					}
				}
	
				if (message.HasBool("open_disks_window"))
					openDisksWindow = true;
	
				if (message.HasBool("open_root_directory")) {
					BDirectory rootDir;
					if (BVolume(nodeRef.device).GetRootDirectory(&rootDir) == B_OK) {
						BEntry entry;
						rootDir.GetEntry(&entry);
						Model *model = new Model(&entry);
						if (model->InitCheck() == B_OK)
							OpenContainerWindow(model, 0, kOpen, kRestoreWorkspace);
						else
							delete model;
					}
				}
			}
			free(buffer);
		}
	}

	// create model for root of everything
	if (deskWindow) {
		BEntry entry("/");
		Model model(&entry);
		if (model.InitCheck() == B_OK) {

			if (gTrackerSettings.ShowDisksIcon()) {
				// add the root icon to desktop window
				BMessage message;
				message.what = B_NODE_MONITOR;
				message.AddInt32("opcode", B_ENTRY_CREATED);
				message.AddInt32("device", model.NodeRef()->device);
				message.AddInt64("node", model.NodeRef()->node);
				message.AddInt64("directory", model.EntryRef()->directory);
				message.AddString("name", model.EntryRef()->name);
				deskWindow->PostMessage(&message, deskWindow->PoseView());
			}
			
			if (openDisksWindow)
				OpenContainerWindow(new Model(model), 0, kOpen, kRestoreWorkspace);
		}
	}

	// kick off building the mime type list for find panels, etc.
	fMimeTypeList = new MimeTypeList();

	if (!BootedInSafeMode())
		// kick of transient query killer
		DeleteTransientQueriesTask::StartUpTransientQueryCleaner();
}

MimeTypeList *
TTracker::MimeTypes() const
{
	return fMimeTypeList;
}	

void 
TTracker::SelectChildInParentSoon(const entry_ref *parent,
	const node_ref *child)
{
	fTaskLoop->RunLater(NewFunctionObject(this,
		&TTracker::SelectChildInParent, *parent, *child),
		100000, 200000, 5000000);
}

void 
TTracker::CloseParentWaitingForChildSoon(const entry_ref *child,
	const node_ref *parent)
{
	fTaskLoop->RunLater(NewFunctionObject(this,
		&TTracker::CloseParentWaitingForChild, *child, *parent),
		200000, 100000, 5000000);
}

void 
TTracker::SelectPoseAtLocationSoon(node_ref parent, BPoint pointInPose)
{
	fTaskLoop->RunLater(NewFunctionObject(this,
		&TTracker::SelectPoseAtLocationInParent, parent, pointInPose),
		100000);
}

void 
TTracker::SelectPoseAtLocationInParent(node_ref parent, BPoint pointInPose)
{
	AutoLock<WindowList> lock(&fWindowList);
	BContainerWindow *parentWindow = FindContainerWindow(&parent);
	if (parentWindow) {
		AutoLock<BWindow> lock(parentWindow);
		parentWindow->PoseView()->SelectPoseAtLocation(pointInPose);
	}
}

bool 
TTracker::CloseParentWaitingForChild(const entry_ref &child,
	const node_ref &parent)
{
	AutoLock<WindowList> lock(&fWindowList);
	
	BContainerWindow *parentWindow = FindContainerWindow(&parent);
	if (!parentWindow)
		// parent window already closed, give up
		return true;

	// If child is a symbolic link, dereference it, so that
	// FindContainerWindow will succeed.
	BEntry entry(&child, true);
	entry_ref resolvedChild;
	if (entry.GetRef(&resolvedChild) != B_OK)
		resolvedChild = child;

	BContainerWindow *window = FindContainerWindow(&resolvedChild);
	if (window) {
		AutoLock<BWindow> lock(window);
		if (!window->IsHidden())
			return CloseParentWindowCommon(parentWindow);
	}
	return false;	
}

void 
TTracker::CloseParent(node_ref &parent)
{
	AutoLock<WindowList> lock(&fWindowList);
	if (!lock)
		return;

	CloseParentWindowCommon(FindContainerWindow(&parent));
}

void
TTracker::ShowSettingsWindow()
{
	if (!fSettingsWindow) {
		fSettingsWindow = new TrackerSettingsWindow();
		fSettingsWindow->Show();
	} else {
		if (fSettingsWindow->Lock()) {
			if (fSettingsWindow->IsHidden())
				fSettingsWindow->Show();
			else
				fSettingsWindow->Activate();
			fSettingsWindow->Unlock();
		}
	}
}

bool 
TTracker::CloseParentWindowCommon(BContainerWindow *window)
{
	ASSERT(fWindowList.IsLocked());
	
	if (dynamic_cast<BDeskWindow *>(window))
		// don't close the destop
		return false;

	window->PostMessage(B_QUIT_REQUESTED);
	return true;
}

bool 
TTracker::SelectChildInParent(const entry_ref &parent, const node_ref &child)
{
	AutoLock<WindowList> lock(&fWindowList);
	
	BContainerWindow *window = FindContainerWindow(&parent);
	if (!window) 
		// parent window already closed, give up
		return false;

	AutoLock<BWindow> windowLock(window);
	
	if (windowLock.IsLocked()) {
		BPoseView *view = window->PoseView();
		int32 index;
		BPose *pose = view->FindPose(&child, &index);
		if (pose) {
			view->SelectPose(pose, index);
			return true;
		}
	}
	return false;	
}

const int32 kNodeMonitorBumpValue = 512;

status_t 
TTracker::NeedMoreNodeMonitors()
{
	fNodeMonitorCount += kNodeMonitorBumpValue;
	PRINT(("bumping nodeMonitorCount to %d\n", fNodeMonitorCount));

	return _kset_mon_limit_(fNodeMonitorCount);
}

status_t
TTracker::WatchNode(const node_ref *node, uint32 flags,
	BMessenger target)
{
	status_t result = watch_node(node, flags, target);
	
	if (result == B_OK || result != ENOMEM)
		// need to make sure this uses the same error value as
		// the node monitor code
		return result;
	
	PRINT(("failed to start monitoring, trying to allocate more "
		"node monitors\n"));

	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		// we are the file panel only, just fail
		return result;

	result = tracker->NeedMoreNodeMonitors();
	
	if (result != B_OK) {
		PRINT(("failed to allocate more node monitors, %s\n",
			strerror(result)));
		return result;
	}

	// try again, this time with more node monitors
	return watch_node(node, flags, target);
}

AutoMounter *
TTracker::AutoMounterLoop()
{
	return fAutoMounter;
}

bool 
TTracker::InTrashNode(const entry_ref *node) const
{
	return TFSContext::IsInTrash(*node);
}

bool
TTracker::TrashFull() const
{
	return fTrashWatcher->CheckTrashDirs();
}
	
bool
TTracker::IsTrashNode(const node_ref *node) const
{
	return fTrashWatcher->IsTrashNode(node);
}	
