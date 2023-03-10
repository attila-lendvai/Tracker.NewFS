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

#include <string.h>

#include <Debug.h>
#include <Directory.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <Volume.h>
#include <VolumeRoster.h>

#include "Attributes.h"
#include "Bitmaps.h"
#include "TFSContext.h"
#include "Tracker.h"
#include "TrashWatcher.h"

BTrashWatcher::BTrashWatcher()
	:	BLooper("TrashWatcher", B_LOW_PRIORITY),
		fTrashNodeList(20, true)
{
	TFSContext::CreateSpecialDirs();
	WatchTrashDirs();
	fTrashFull = CheckTrashDirs();
	UpdateTrashIcons();

	// watch volumes
	TTracker::WatchNode(0, B_WATCH_MOUNT, this);
}

BTrashWatcher::~BTrashWatcher()
{
	stop_watching(this);
}

bool
BTrashWatcher::IsTrashNode(const node_ref *testNode) const
{
	int32 count = fTrashNodeList.CountItems();
	for (int32 index = 0; index < count; index++) {
		node_ref *nref = fTrashNodeList.ItemAt(index);
		if (nref->node == testNode->node && nref->device == testNode->device)
			return true;
	}

	return false;
}

void
BTrashWatcher::MessageReceived(BMessage *message)
{
	if (message->what != B_NODE_MONITOR) {
		_inherited::MessageReceived(message);
		return;
	}
	
	switch (message->FindInt32("opcode")) {
		case B_ENTRY_CREATED:
			if (!fTrashFull) {
				fTrashFull = true;
				UpdateTrashIcons();
			}
			break;

		case B_ENTRY_MOVED:
			{
				// allow code to fall through if move is from/to trash
				// but do nothing for moves in the same directory
				ino_t toDir;
				ino_t fromDir;
				message->FindInt64("from directory", &fromDir);
				message->FindInt64("to directory", &toDir);
				if (fromDir == toDir)
					break;
			}
			// fall thru
			
		case B_DEVICE_UNMOUNTED:
			// fall thru

		case B_ENTRY_REMOVED:
			{
				bool full = CheckTrashDirs();
				if (fTrashFull != full) {
					fTrashFull = full;
					UpdateTrashIcons();
				}
				break;
			}
		// We should handle DEVICE_UNMOUNTED here too to remove trash

		case B_DEVICE_MOUNTED:
			{
				dev_t device;
				BDirectory trashDir;
				if (message->FindInt32("new device", &device) == B_OK
					&& TFSContext::GetTrashDir(trashDir, device) == B_OK) {
					node_ref trashNode;
					trashDir.GetNodeRef(&trashNode);
					TTracker::WatchNode(&trashNode, B_WATCH_DIRECTORY, this);
					fTrashNodeList.AddItem(new node_ref(trashNode));

					// Check if the new volume has anything trashed.
					if (CheckTrashDirs() && !fTrashFull) {
						fTrashFull = true;
						UpdateTrashIcons();
					}
				}
				break;
			}
	}
}

void
BTrashWatcher::UpdateTrashIcons()
{
#ifndef OLD_TRASHWATCHER
	// cause update in the cache
	IconCache::sIconCache->IconChanged("tracker/active_trash", NULL);
	
	BVolume	boot;
	if (BVolumeRoster().GetBootVolume(&boot) != B_OK)
		return;
	
	BDirectory dir;
	if (TFSContext::GetTrashDir(dir, boot.Device()) != B_OK)
		return;
	
	// trigger an attrchanged notification to any watching model
	// this will cause a redraw
	uint8 temp;
	dir.ReadAttr(kAttrMiniIcon, B_COLOR_8_BIT_TYPE, 0, &temp, 1);
	dir.WriteAttr(kAttrMiniIcon, B_COLOR_8_BIT_TYPE, 0, &temp, 1);
	
#else
	BVolume	boot;
	if (BVolumeRoster().GetBootVolume(&boot) != B_OK)
		return;

	BDirectory trashDir;
	if (TFSContext::GetTrashDir(trashDir, boot.Device()) == B_OK) {
		// pull out the icons for the current trash state from resources and
		// apply them onto the trash directory node
		size_t largeSize = 0;
		size_t smallSize = 0;
		
		const void *largeData = GetTrackerResources()->LoadResource('ICON',
			fTrashFull ? kResTrashFullIcon : kResTrashIcon, &largeSize);

		const void *smallData = GetTrackerResources()->LoadResource('MICN',
			fTrashFull ? kResTrashFullIcon : kResTrashIcon,  &smallSize);

		// apply the themed svg icon
		if (TrackerSettings().ThemesEnabled()) {
			const char *filename = GetTrackerTheme()->FileForID(fTrashFull ? kResTrashFullIcon : kResTrashIcon);
			BPath path = GetTrackerTheme()->GetIconPath(filename, USE_SCALABLE);
			if (path.Path() != NULL) {
				BFile file(path.Path(), B_READ_ONLY);
				
				if (file.InitCheck() == B_OK) {
					off_t size;
					file.GetSize(&size);
					uint8 buffer[size];
					file.Read(buffer, size);
					trashDir.RemoveAttr(kAttrScalableIcon);
					trashDir.WriteAttr(kAttrScalableIcon, 'ZICO', 0, buffer, size);
				}
			}
		}
		
		if (largeData) 
			trashDir.WriteAttr(kAttrLargeIcon, B_COLOR_8_BIT_TYPE, 0,
				largeData, largeSize);
		else
			TRESPASS();
		
		if (smallData)
			trashDir.WriteAttr(kAttrMiniIcon, B_COLOR_8_BIT_TYPE, 0,
				smallData, smallSize);
		else
			TRESPASS();
	}
#endif
}

void
BTrashWatcher::WatchTrashDirs()
{
	BVolumeRoster volRoster;
	volRoster.Rewind();
	BVolume	volume;
	while (volRoster.GetNextVolume(&volume) == B_OK) {
		if (volume.IsReadOnly() || !volume.IsPersistent())
			continue;
		
		BDirectory trashDir;
		if (TFSContext::GetTrashDir(trashDir, volume.Device()) == B_OK) {
			node_ref trash_node;
			trashDir.GetNodeRef(&trash_node);
			watch_node(&trash_node, B_WATCH_DIRECTORY, this);
			fTrashNodeList.AddItem(new node_ref(trash_node));
		}
	}
}

bool
BTrashWatcher::CheckTrashDirs()
{
	BVolumeRoster volRoster;
	volRoster.Rewind();
	BVolume	volume;
	while (volRoster.GetNextVolume(&volume) == B_OK) {
		if (volume.IsReadOnly() || !volume.IsPersistent())
			continue;
		
		BDirectory trashDir;
		TFSContext::GetTrashDir(trashDir, volume.Device());
		trashDir.Rewind();
		BEntry entry;
		if (trashDir.GetNextEntry(&entry) == B_OK)
			return true;
	}

	return false;
}
