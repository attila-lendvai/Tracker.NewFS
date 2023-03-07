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

#include <Alert.h>
#include <Debug.h>
#include <Directory.h>
#include <File.h>
#include <Font.h>
#include <MenuField.h>
#include <Mime.h>
#include <NodeInfo.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <Roster.h>
#include <Screen.h>
#include <ScrollView.h>
#include <SymLink.h>
#include <TextView.h>
#include <Volume.h>
#include <VolumeRoster.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "Attributes.h"
#include "AutoLock.h"
#include "Commands.h"
#include "FSUtils.h"
#include "IconCache.h"
#include "IconMenuItem.h"
#include "InfoWindow.h"
#include "LanguageTheme.h"
#include "Model.h"
#include "NavMenu.h"
#include "PoseView.h"
#include "TFSContext.h"
#include "Tracker.h"
#include "WidgetAttributeText.h"

const float kDrawMargin = 3.0f;
const float kBorderMargin = 15.0f;
const float kBorderWidth = 32.0f;

// Offsets taken from TAlertView::Draw in BAlert.cpp
const float kIconHorizOffset = 18.0f;
const float kIconVertOffset = 6.0f;

// The font height's for the two types of information we display
const float kTitleFontHeight = 14.0f;
const float kAttribFontHeight = 10.0f;

// Amount you have to move the mouse before a drag starts
const float kDragSlop = 3.0f;

const rgb_color kAttrTitleColor = {0, 0, 0, 255};
const rgb_color kAttrValueColor = {0, 0, 0, 255};
const rgb_color kLinkColor = {0, 0, 220, 255};
const rgb_color kDarkBorderColor = {184, 184, 184, 255};

const uint32 kSetPreferredApp = 'setp';
const uint32 kSelectNewSymTarget = 'snew';
const uint32 kNewTargetSelected = 'selc';
const uint32 kRecalculateSize = 'resz';
const uint32 kSetLinkTarget = 'link';
const uint32 kPermissionsSelected = 'sepe';
const uint32 kOpenLinkSource = 'opls';
const uint32 kOpenLinkTarget = 'oplt';

const uint32 kPaneSwitchClosed = 0;
const uint32 kPaneSwitchOpen = 2;

BInfoWindow::BInfoWindow(Model *model, int32 group_index, LockingList<BWindow> *list)
	:	BWindow(BInfoWindow::InfoWindowRect(false),
			"InfoWindow", B_TITLED_WINDOW,
			B_NOT_RESIZABLE | B_NOT_ZOOMABLE, B_CURRENT_WORKSPACE),
		fModel(model),
		fStopCalc(false),
		fIndex(group_index),
		fCalcThreadID(-1),
		fWindowList(list),
		fPermissionsView(NULL),
		fFilePanel(NULL),
		fFilePanelOpen(false),
		fMultiple(false),
		fMessage(NULL),
		fRefs(NULL)
{
	SetPulseRate(1000000);		// we use pulse to check freebytes on volume
		
	TTracker::WatchNode(model->NodeRef(), B_WATCH_ALL | B_WATCH_MOUNT, this);

	// window list is Locked by Tracker around this constructor
	if (list)
		list->AddItem(this);

	AddShortcut('E', 0, new BMessage(kEditItem));
	AddShortcut('O', 0, new BMessage(kOpenSelection));
	AddShortcut('T', 0, new BMessage(kMoveToTrash));
	AddShortcut('U', 0, new BMessage(kUnmountVolume));
	AddShortcut('P', 0, new BMessage(kPermissionsSelected));
	
	Run();
}

BInfoWindow::BInfoWindow(BMessage *message, int32 groupIndex, LockingList<BWindow> *list)
	:	BWindow(BInfoWindow::InfoWindowRect(false),
			"InfoWindow", B_TITLED_WINDOW,
			B_NOT_RESIZABLE | B_NOT_ZOOMABLE, B_CURRENT_WORKSPACE),
		fModel(NULL),
		fStopCalc(false),
		fIndex(groupIndex),
		fCalcThreadID(-1),
		fWindowList(list),
		fPermissionsView(NULL),
		fFilePanel(NULL),
		fFilePanelOpen(false),
		fMultiple(true),
		fMessage(message),
		fRefs(NULL)
{
	SetPulseRate(1000000);		// we use pulse to check freebytes on volume
	
	type_code type;
	int32 count;
	message->GetInfo("refs", &type, &count);
	
	fRefs = new BObjectList<entry_ref>(count);
	
	for (int32 index = 0; index < count; index++) {
		entry_ref *ref = new entry_ref;
		message->FindRef("refs", index, ref);
		fRefs->AddItem(ref);
		
		BEntry entry;
		if (entry.SetTo(ref) == B_OK) {
			node_ref nref;
			entry.GetNodeRef(&nref);
			TTracker::WatchNode(&nref, B_WATCH_ALL | B_WATCH_MOUNT, this);
		}
	}
	
	// window list is Locked by Tracker around this constructor
	if (list)
		list->AddItem(this);

	AddShortcut('O', 0, new BMessage(kOpenSelection));
	AddShortcut('T', 0, new BMessage(kMoveToTrash));
	AddShortcut('U', 0, new BMessage(kUnmountVolume));
	AddShortcut('P', 0, new BMessage(kPermissionsSelected));
	
	Run();
}

BInfoWindow::~BInfoWindow()
{
	// Check to make sure the file panel is destroyed...
	delete fFilePanel;
	delete fModel;
}

BRect
BInfoWindow::InfoWindowRect(bool)
{
	return BRect(70, 50, 385, 220);
}

void
BInfoWindow::Quit()
{
	stop_watching(this);

	if (fWindowList) {
		AutoLock<LockingList<BWindow> > lock(fWindowList);
		fWindowList->RemoveItem(this);
	}

	fStopCalc = true;

	// wait until CalcSize thread has terminated before closing window
	status_t result;
	wait_for_thread(fCalcThreadID, &result);

	_inherited::Quit();
}

bool
BInfoWindow::IsShowing(const node_ref *node) const
{
	if (fMultiple)
		return false;
	else
		return *TargetModel()->NodeRef() == *node;
}

void
BInfoWindow::Show()
{
	if (!fMultiple) {
		BModelOpener modelOpener(fModel);
		if (TargetModel()->InitCheck() != B_OK) {
			Close();
			return;
		}
	}

	AutoLock<BWindow> lock(this);

	BRect attrRect(Bounds());
	fAttributeView = new AttributeView(attrRect, fModel, fRefs);
	ResizeTo(fAttributeView->Bounds().Width(), Bounds().Height());
	AddChild(fAttributeView);

	// position window appropriately based on index
	BRect windRect(InfoWindowRect(fMultiple || TargetModel()->IsSymLink() || TargetModel()->IsFile()));
	if ((fIndex + 2) % 2 == 1) {
		windRect.OffsetBy(320, 0);
		fIndex--;
	}

	windRect.OffsetBy(fIndex * 8, fIndex * 8);

	// make sure window is visible on screen
	BScreen screen(this);
	if (!windRect.Intersects(screen.Frame()))
		windRect.OffsetTo(50, 50);

	MoveTo(windRect.LeftTop());
	ResizeTo(windRect.Width(), windRect.Height());

	// volume case is handled by view
	if (fMultiple || !TargetModel()->IsVolume()) {
		if (fMultiple || TargetModel()->IsDirectory()) {
			// if this is a folder or a multiple-file-info then spawn thread to calculate size
			SetSizeStr(LOCALE("calculating"B_UTF8_ELLIPSIS));
			fCalcThreadID = spawn_thread((fMultiple ? BInfoWindow::CalcMultipleSize : BInfoWindow::CalcSize), "CalcSize", B_NORMAL_PRIORITY, this);
			resume_thread(fCalcThreadID);
		} else {
			fAttributeView->SetLastSize(TargetModel()->StatBuf()->st_size);

			BString sizeStr;
			GetSizeString(sizeStr, fAttributeView->LastSize(), 0);
			SetSizeStr(sizeStr.String());
		}
	}

	BString buffer;
	
	if (fMultiple)
		buffer << LOCALE("Multiple File Info");
	else
		buffer << TargetModel()->Name() << LOCALE(" info");
	
	SetTitle(buffer.String());

	lock.Unlock();
	_inherited::Show();
}

void
BInfoWindow::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case kRestoreState: 
			Show();
			break;
		
		case kOpenSelection:
			{
				BMessage refsMessage(B_REFS_RECEIVED);
				if (!fMultiple)
					refsMessage.AddRef("refs", fModel->EntryRef());
				else {
					for (int index = 0; index < fRefs->CountItems(); index++)
						refsMessage.AddRef("refs", fRefs->ItemAt(index));
				}
			
				// add a messenger to the launch message that will be used to
				// dispatch scripting calls from apps to the PoseView
				refsMessage.AddMessenger("TrackerViewToken", BMessenger(this));
				be_app->PostMessage(&refsMessage);
				break;
			}
		
		case kEditItem:
			{
				if (fMultiple)
					break;
				
				BEntry entry(fModel->EntryRef());
				if (ConfirmChangeIfWellKnownDirectory(&entry, "rename"))
					fAttributeView->BeginEditingTitle();
				break;
			}
		
		case kIdentifyEntry:
			{
				bool force = (modifiers() & B_OPTION_KEY) != 0;
				BEntry entry;
				
				if (!fMultiple) {
					if (entry.SetTo(fModel->EntryRef(), true) == B_OK) {
						BPath path;
						if (entry.GetPath(&path) == B_OK) 
							update_mime_info(path.Path(), true, false, force ? 2 : 1);
					}
				} else {
					for (int index = 0; index < fRefs->CountItems(); index++) {
						if (entry.SetTo(fRefs->ItemAt(index), true) == B_OK) {
							BPath path;
							if (entry.GetPath(&path) == B_OK)
								update_mime_info(path.Path(), true, false, force ? 2 : 1);
						}
					}
				}
				break;
			}
		
		case kRecalculateSize:
			{
				fStopCalc = true;
			
				// Wait until any current CalcSize thread has terminated before
				// starting a new one
				status_t result;
				wait_for_thread(fCalcThreadID, &result);
				
				// Start recalculating..			
				fStopCalc = false;
				SetSizeStr(LOCALE("calculating"B_UTF8_ELLIPSIS));
				fCalcThreadID = spawn_thread((fMultiple ? BInfoWindow::CalcMultipleSize : BInfoWindow::CalcSize), "CalcSize", B_NORMAL_PRIORITY, this);
				resume_thread(fCalcThreadID);
				
				break;
			}
		
		case kSetLinkTarget: 
			if (fMultiple)
				break;
			
			OpenFilePanel(fModel->EntryRef());
			break;

		// An item was dropped into the window
		case B_SIMPLE_DATA:
			// If we are not a SymLink, just ignore the request
			if (fMultiple || !fModel->IsSymLink()) 
				break;
			// supposed to fall through

		// An item was selected from the file panel
		case kNewTargetSelected:
			{
				if (fMultiple)
					break;
				
				// Extract the BEntry, and set its full path to the string value
				BEntry targetEntry;
				entry_ref ref;
				BPath path;
				
				if (message->FindRef("refs", &ref) == B_OK
					&& targetEntry.SetTo(&ref, true) == B_OK
					&& targetEntry.Exists()) {
					// We now have to re-target the broken symlink. Unfortunately,
					// there's no way to change the target of an existing symlink.
					// So we have to delete the old one and create a new one.
					// First, stop watching the broken node (we don't want this window
					// to quit when the node is removed.)
					stop_watching(this);
					
					// Get the parent...
					BDirectory parent;
					BEntry tmpEntry(TargetModel()->EntryRef());
					if (tmpEntry.GetParent(&parent) != B_OK)
						break;

					// Preserve the name
					BString name(TargetModel()->Name());

					// Extract path for new target...
					BEntry target(&ref);
					BPath targetPath;
					if (target.GetPath(&targetPath) != B_OK)
						break;

					// Preserve the original attributes
					AttributeStreamMemoryNode memoryNode;
					{
						BModelOpener opener(TargetModel());
						AttributeStreamFileNode original(TargetModel()->Node());
						memoryNode << original;
					}
					
					// Delete the broken node.
					BEntry oldEntry(TargetModel()->EntryRef());
					oldEntry.Remove();
					
					// Create new node
					BSymLink link;
					parent.CreateSymLink(name.String(), targetPath.Path(), &link);
					
					// Update our Model()
					BEntry symEntry(&parent, name.String());
					fModel->SetTo(&symEntry);

					BModelWriteOpener opener(TargetModel());
					
					// Copy the attributes back...
					AttributeStreamFileNode newNode(TargetModel()->Node());
					newNode << memoryNode;
					
					// Start watching this again...					
					TTracker::WatchNode(TargetModel()->NodeRef(), B_WATCH_ALL | B_WATCH_MOUNT, this);

					// Tell the attribute view about this new model...					
					fAttributeView->ReLinkTargetModel(TargetModel());
				}
				break;
			}

		case B_CANCEL: 
			// File panel window has closed
			delete fFilePanel;
			fFilePanel = NULL;
			// It's no longer open
			fFilePanelOpen = false;
			break;

		case kUnmountVolume:
			// Sanity check that this isn't the boot volume...
			// (The unmount menu item should have been disabled anyways)
			if (!fMultiple && fModel->IsVolume()) {
				BVolume boot;
				BVolumeRoster().GetBootVolume(&boot);
				BVolume volume(fModel->NodeRef()->device);
				if (volume != boot) {
					dynamic_cast<TTracker*>(be_app)->SaveAllPoseLocations();
	
					BMessage unmountMessage(kUnmountVolume);
					unmountMessage.AddInt32("device_id", volume.Device());
					be_app->PostMessage(&unmountMessage);
				}
			}
			break;
		
		case kDelete:
			{
				// ask user and async
				if (fMultiple)
					(new TFSContext(fRefs))->Remove(true, true);
				else
					(new TFSContext(*fModel->EntryRef()))->Remove(true, true);
				
				break;
			}
			
		case kMoveToTrash:
			{
				TFSContext *tfscontext;
				if (fMultiple)
					tfscontext = new TFSContext(fRefs);
				else
					tfscontext = new TFSContext(*fModel->EntryRef());
				
				// both async, ask user if delete
				if (modifiers() & B_SHIFT_KEY)
					tfscontext->Remove(true, true);
				else
					tfscontext->MoveToTrash(true);

				break;
			}

		case kEmptyTrash: 
			(new TFSContext())->EmptyTrash();
			break;

		case B_NODE_MONITOR:
			switch (message->FindInt32("opcode")) {
				case B_ENTRY_REMOVED:
					{
						node_ref itemNode;
						message->FindInt32("device", &itemNode.device);
						message->FindInt64("node", &itemNode.node);
						if (!fMultiple) {
							// our window itself may be deleted
							if (*TargetModel()->NodeRef() == itemNode) 
								Close();
						} else {
							// we may need to recalculate size
						}
						break;
					}
	
				case B_ENTRY_MOVED:
				case B_STAT_CHANGED:
				case B_ATTR_CHANGED:
					if (fMultiple)
						break;
					
					fAttributeView->ModelChanged(TargetModel(), message);
						// must be called before the FilePermissionView::ModelChanged()
						// call, because it changes the model... (bad style!)

					if (fPermissionsView != NULL)
						fPermissionsView->ModelChanged(TargetModel());
					break;
				
				case B_DEVICE_UNMOUNTED:
					{
						// We were watching a volume that is no longer mounted,
						// we might as well quit...
						node_ref itemNode;
						// Only the device information is available...
						message->FindInt32("device", &itemNode.device);
						if (!fMultiple) {
							if (TargetModel()->NodeRef()->device == itemNode.device)
								Close();
						} else {
							// we may need to recalculate or close
						}

						break;
					}
					
				default: 
					break;
			}
			break;
	
		case kPermissionsSelected:
			if (fPermissionsView == NULL) {
				// Only true on first call.
				BRect rect(kBorderWidth + 1, fAttributeView->Bounds().bottom,
					fAttributeView->Bounds().right, fAttributeView->Bounds().bottom + 80);
				
				if (!fMultiple)
					fPermissionsView = new FilePermissionsView(rect, fModel);	
				else
					fPermissionsView = new FilePermissionsView(rect, fRefs);	
				
				fPermissionsView->ResizeToPreferred();
				if (fPermissionsView->Bounds().Width() > Bounds().Width()) {
					ResizeTo(fPermissionsView->Bounds().Width(), Bounds().Height());
					fAttributeView->Invalidate();
				}
				
				ResizeBy(0, fPermissionsView->Bounds().Height());
				fAttributeView->AddChild(fPermissionsView);
				fAttributeView->SetPermissionsSwitchState(kPaneSwitchOpen);
			} else if (fPermissionsView->IsHidden()) {
				//fPermissionsView->ModelChanged(fModel);
				fPermissionsView->Show();
				fPermissionsView->ResizeToPreferred();
				ResizeBy(0, fPermissionsView->Bounds().Height());
				fAttributeView->SetPermissionsSwitchState(kPaneSwitchOpen);
			} else {
				fPermissionsView->Hide();
				ResizeBy(0, -fPermissionsView->Bounds().Height());
				fPermissionsView->ResizeToPreferred();
				fAttributeView->SetPermissionsSwitchState(kPaneSwitchClosed);
			}
			break;

		default:
			_inherited::MessageReceived(message);
			break;
	}
}

static BString &
PrintFloat(BString &result, float number)
{
	char buffer[128];
	sprintf(buffer, "%.1f", number);
	result += buffer;
	return result;
}

void
BInfoWindow::GetSizeString(BString &result, off_t size, int32 fileCount, int32 dirCount, int32 linkCount, bool multiple)
{
	char numStr[256];
	sprintf(numStr, "%Ld", size);
	BString bytes;

	uint32 length = strlen(numStr);
	if (length >= 4) {
		uint32 charsTillComma = length % 3;
		if (charsTillComma == 0)
			charsTillComma = 3;

		uint32 numberIndex = 0;
	
		while (numStr[numberIndex]) {
			bytes += numStr[numberIndex++];
			if (--charsTillComma == 0 && numStr[numberIndex]) {
				bytes += ',';
				charsTillComma = 3;
			}
		}
	} else
		bytes = numStr;
	
	if (size >= kTBSize)
		PrintFloat(result, (float)size / kTBSize) << " " << LOCALE("TB");
	else if (size >= kGBSize) 
		PrintFloat(result, (float)size / kGBSize) << " " << LOCALE("GB");
	else if (size >= kMBSize)
		PrintFloat(result, (float)size / kMBSize) << " " << LOCALE("MB");
	else if (size >= kKBSize)
		result << (int64)(size + kHalfKBSize) / kKBSize << " " << LOCALE("KB");
	else
		result << size;

	if (!multiple) {
		if (size >= kKBSize)
			result << " (" << bytes;
		
		result << " " << LOCALE("bytes");
		
		if (size >= kKBSize)
			result  << ")";

		if (fileCount)
			result << " " << LOCALE("for") << " " << fileCount << " " << LOCALE("files");
	} else {
		result << ", " << fileCount << " file" << (fileCount == 1 ? "" : "s");
		result << ", " << linkCount << " link" << (linkCount == 1 ? "" : "s");
		result << ", " << dirCount << " dir" << (dirCount == 1 ? "" : "s");
	}
}

class InfoWindowTFSContext : public TFSContext {
	BInfoWindow		&mWindow;
	
public:
	InfoWindowTFSContext(BInfoWindow &in_window, const entry_ref &in_ref) :
		TFSContext(in_ref), mWindow(in_window) {}
	InfoWindowTFSContext(BInfoWindow &in_window) :
		TFSContext(), mWindow(in_window) {}
	
	void	CheckCancel() {
				if (mWindow.StopCalc()) {
					FS_CONTROL_THROW(kCancel);
				}
			}
};

int32
BInfoWindow::CalcSize(void *castToWindow)
{
	BInfoWindow *window = static_cast<BInfoWindow *>(castToWindow);
	BDirectory dir(window->TargetModel()->EntryRef());
	BDirectory trashDir;
	TFSContext::GetTrashDir(trashDir, window->TargetModel()->EntryRef()->device); 
	if (dir.InitCheck() != B_OK) {
		if (window->StopCalc())
			return B_ERROR;
		
		AutoLock<BWindow> lock(window);
		if (!lock)
			return B_ERROR;
		
		window->SetSizeStr(LOCALE("Error calculating size."));

		return B_ERROR;
	}
	
	TFSContext::ProgressInfo info;
	
	if (dir == trashDir) {
		
		InfoWindowTFSContext *context = new InfoWindowTFSContext(*window);
		BVolumeRoster volRoster;
		BVolume volume;
		BEntry entry;
		entry_ref ref;
		
		while (volRoster.GetNextVolume(&volume) == B_OK) {
			if (!volume.IsPersistent())
				continue;
			
			if (TFSContext::GetTrashDir(trashDir, volume.Device()) == B_OK) {
				trashDir.GetEntry(&entry);
				entry.GetRef(&ref);
				context->EntryList().push_back(ref);
			}
		}	
		context -> CalculateItemsAndSize(info);
		
	} else {
	
		InfoWindowTFSContext *context = new InfoWindowTFSContext(*window, *window->TargetModel()->EntryRef());
		context -> CalculateItemsAndSize(info);
	}
		
	BString string;
	// got the size value, update the size string
	GetSizeString(string, info.mTotalSize, info.mTotalEntryCount - info.mTotalDirCount);
	// XXX we've got a lot more additional information here, what to do?

	if (window->StopCalc())
		// window closed, bail
		return B_OK;

	AutoLock<BWindow> lock(window);
	if (lock.IsLocked()) 
		window->SetSizeStr(string.String());

	return B_OK;
}

int32
BInfoWindow::CalcMultipleSize(void *castToWindow)
{
	BInfoWindow *window = static_cast<BInfoWindow *>(castToWindow);
	int32 dirs = 0;
	int32 files = 0;
	int32 links = 0;
	off_t size = 0;
	off_t filesize;
	
	BString string;
	for (int index = 0; index < window->fRefs->CountItems(); index++) {
		BEntry entry(window->fRefs->ItemAt(index));
		
		if (entry.InitCheck() == B_OK) {
			if (entry.IsDirectory())
				dirs++;
			else if (entry.IsSymLink())
				links++;
			else if (entry.IsFile()) {
				files++;
				entry.GetSize(&filesize);
				size += filesize;
			} else
				printf("something is wrong in CalcMultipleSize!\n");
		}
	}
	
	// got the size value, update the size string
	GetSizeString(string, size, files, dirs, links, true);
	
	if (window->StopCalc())
		// window closed, bail
		return B_OK;

	AutoLock<BWindow> lock(window);
	if (lock.IsLocked()) 
		window->SetSizeStr(string.String());

	return B_OK;
}

void
BInfoWindow::SetSizeStr(const char *sizeStr)
{
	AttributeView *view = dynamic_cast<AttributeView *>(FindView("attr_view"));
	if (view)
		view->SetSizeStr(sizeStr);
}

void
BInfoWindow::OpenFilePanel(const entry_ref *ref)
{
	// Open a file dialog box to allow the user to select a new target
	// for the sym link...
	if (fFilePanel == NULL) {
		BMessenger runner(this);

		fFilePanel = new BFilePanel(B_OPEN_PANEL, &runner, ref,
			B_FILE_NODE | B_SYMLINK_NODE | B_DIRECTORY_NODE,
			false, new BMessage(kNewTargetSelected));

		if (fFilePanel != NULL) {
			fFilePanel->SetButtonLabel(B_DEFAULT_BUTTON, LOCALE("Select"));
			fFilePanel->Window()->ResizeTo(500, 300);
			char buffer[1024];
			sprintf(buffer, LOCALE("Link \"%s\" to:"), fModel->Name());
			fFilePanel->Window()->SetTitle(buffer);
			fFilePanel->Show();
			fFilePanelOpen = true;
		}
	} else if (!fFilePanelOpen) {
		fFilePanel->Show();
		fFilePanelOpen = true;
	} else {
		fFilePanelOpen = true;
		fFilePanel->Window()->Activate(true);
	}
}


AttributeView::AttributeView(BRect rect, Model *model, BObjectList<entry_ref> *refs)
	:	BView(rect, "attr_view", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_PULSE_NEEDED),
		fDivider(0),
		fPreferredAppMenu(NULL),
		fModel(model),
		fIconModel(model),
		fMouseDown(false),
		fDragging(false),
		fDoubleClick(false),
		fTrackingState(no_track),
		fIsDropTarget(false),
		fTitleEditView(NULL),
		fPathWindow(NULL),
		fLinkWindow(NULL),
		fMultiple(refs != NULL),
		fRefs(refs)
{
	// Set view color to standard background grey...
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	
	// If the model is a symlink, then we deference the model to
	// get the targets icon...
	if (!fMultiple && fModel->IsSymLink()) {
		Model *resolvedModel = new Model(model->EntryRef(), true, true);
		if (resolvedModel->InitCheck() == B_OK)
			fIconModel = resolvedModel;
		// broken link, just show the symlink
	}

	// Create the rect for displaying the icon
	fIconRect.Set(0, 0, B_LARGE_ICON - 1, B_LARGE_ICON - 1);
	// Offset taken from BAlert...
	fIconRect.OffsetBy(kIconHorizOffset, kIconVertOffset);
	
	if (fMultiple) {
		fButton = new BButton(fIconRect, "Button", LOCALE("Multi"), new BMessage(kShowMultiple));
		AddChild(fButton);
		fExpanded = false;
	}
	
	// The title rect...
	// The magic numbers are used to properly calculate the rect so that
	// when the editing text view is displayed, the position of the text
	// does not change. 
	BFont currentFont;
	font_height fontMetrics;
	GetFont(&currentFont);
	currentFont.SetSize(kTitleFontHeight);
	currentFont.GetHeight(&fontMetrics);

	fTitleRect.left = fIconRect.right + 5;
	fTitleRect.top = 0;
	fTitleRect.bottom = fontMetrics.ascent + 1;
	fTitleRect.right = min_c(fTitleRect.left + currentFont.StringWidth(fMultiple ? LOCALE("Multiple Files") : fModel->Name()), Bounds().Width() - 5);
	// Offset so that it centers with the icon
	fTitleRect.OffsetBy(0, fIconRect.top + ((fIconRect.Height() - fTitleRect.Height()) / 2));
	// Make some room for the border for when we are in edit mode...
	// (Negative numbers increase the size of the rect)
	fTitleRect.InsetBy(-1, -2);

	fFreeBytes = -1;
	fSizeStr = "";
	fSizeRect.Set(0, 0, 0, 0);
	
	// Find offset for attributes, might be overiden below if there
	// is a prefered handle menu displayed
#if ENABLE_LANGUAGE_THEMES
	// Due to localisation we need to check all strings to find the longest
	// TODO: Maybe optimize this?
	fDivider = 0;
	currentFont.SetSize(kAttribFontHeight);
	fDivider = MAX(fDivider, currentFont.StringWidth(LOCALE("Created:")));
	fDivider = MAX(fDivider, currentFont.StringWidth(LOCALE("Modified:")));
	fDivider = MAX(fDivider, currentFont.StringWidth(LOCALE("Kind:")));
	fDivider = MAX(fDivider, currentFont.StringWidth(LOCALE("Path:")));
	
	if (!fMultiple && fModel->IsSymLink())
		fDivider = MAX(fDivider, currentFont.StringWidth(LOCALE("Link To:")));
	if (!fMultiple && fModel->IsExecutable())
		fDivider = MAX(fDivider, currentFont.StringWidth(LOCALE("Version:")));
	if (!fMultiple && fModel->IsVolume())
		fDivider = MAX(fDivider, currentFont.StringWidth(LOCALE("Capacity:")));
	
	fDivider += kBorderMargin + kBorderWidth + 1;
#else
	currentFont.SetSize(kAttribFontHeight);
	fDivider = currentFont.StringWidth(LOCALE("Modified:")) + kBorderMargin + kBorderWidth + 1;
#endif
	
	// Add a preferred handler pop-up menu if this item
	// is a file...This goes in place of the Link To:
	// string...
	if (fMultiple || model->IsFile()) {
		BMimeType mime;
		BNodeInfo nodeInfo;
		bool executable = true;
		
		if (fMultiple) {
			for (int index = 0; index < fRefs->CountItems(); index++) {
				Model model(fRefs->ItemAt(index));
				if (model.InitCheck() != B_OK)
					continue;
				
				// we want to find out if the files all have the same type
				if (!model.IsExecutable()) {
					if (!index) {
						if (model.MimeType() != mime.Type()) {
							executable = true;
							break;
						}
					} else {
						executable = false;
						mime.SetTo(model.MimeType());
					}
				}
			}
		} else {
			executable = fModel->IsExecutable();
			mime.SetTo(fModel->MimeType());
			nodeInfo.SetTo(fModel->Node());
		}
		
		// But don't add the menu if the file is executable
		if (!executable) {
			SetFontSize(kAttribFontHeight);
			float lineHeight = CurrentFontHeight();
			float divider = currentFont.StringWidth(LOCALE("Opens with:")) + 5;
			fDivider = MAX(fDivider, divider + kBorderWidth + kBorderMargin - 2);
			
			BRect preferredAppRect(fDivider - divider + 2,
				fTitleRect.bottom + (lineHeight * 7),
				Bounds().Width() - 5, fTitleRect.bottom + (lineHeight * 8));
			fPreferredAppMenu = new BMenuField(preferredAppRect, "", "", new BPopUpMenu(""));
			fPreferredAppMenu->SetDivider(divider);
			fPreferredAppMenu->SetFont(&currentFont);
			fPreferredAppMenu->SetHighColor(kAttrTitleColor);
			fPreferredAppMenu->SetLabel(LOCALE("Opens with:"));
	
			char prefSignature[B_MIME_TYPE_LENGTH];
			nodeInfo.GetPreferredApp(prefSignature);
	
			BMessage supportingAppList;
			mime.GetSupportingApps(&supportingAppList);
			// Add the default menu item and set it to marked...
			BMenuItem *result;
			result = new BMenuItem(LOCALE("Default Application"), new BMessage(kSetPreferredApp));
			result->SetTarget(this);
			fPreferredAppMenu->Menu()->AddItem(result);
			result->SetMarked(true);
			
			for (int32 index = 0; ; index++) {
				const char *signature;
				if (supportingAppList.FindString("applications", index, &signature) != B_OK)
					break;
				
				// Only add separator item if there are more items
				if (index == 0)
					fPreferredAppMenu->Menu()->AddSeparatorItem();
					
				BMessage *itemMessage = new BMessage(kSetPreferredApp);
				itemMessage->AddString("signature", signature);
			
				status_t err = B_ERROR;
				entry_ref entry;
				
				if (signature && signature[0])
					err = be_roster->FindApp(signature, &entry);
				
				if (err != B_OK)
					result = new BMenuItem(signature, itemMessage);
				else 
					result = new BMenuItem(entry.name, itemMessage);
			
				result->SetTarget(this);
				fPreferredAppMenu->Menu()->AddItem(result);
				if (strcmp(signature, prefSignature) == 0)
					result->SetMarked(true);
			}
	
			AddChild(fPreferredAppMenu);
		}
	}

	fPermissionsSwitch = new PaneSwitch(BRect(), LOCALE("Permissions"));
	fPermissionsSwitch->SetMessage(new BMessage(kPermissionsSelected));
	AddChild(fPermissionsSwitch);

	BStringView *stringView = new BStringView(BRect(), "Permissions", LOCALE("Permissions"));
	
	AddChild(stringView);

	stringView->ResizeToPreferred();
	
	BRect bounds = Bounds(), stringViewBounds = stringView->Bounds();
	fPermissionsSwitch->MoveTo(kBorderWidth + 3, bounds.bottom - stringViewBounds.bottom +
		(stringViewBounds.bottom - 11) / 2);
	
	stringView->MoveTo(kBorderWidth + 11 + 3, bounds.bottom -
		stringViewBounds.bottom);

	fPermissionsSwitch->ResizeTo(10, 11);

	InitStrings(model);
}

AttributeView::~AttributeView()
{
	if (fPathWindow->Lock())
		fPathWindow->Quit();
	
	if (fLinkWindow->Lock())
		fLinkWindow->Quit();
	
/*	if (fDescWindow->Lock())
		fDescWindow->Quit(); */

	if (!fMultiple && fModel->IsSymLink() && fIconModel != fModel)
		delete fIconModel;
}

void
AttributeView::SetRefs(BObjectList<entry_ref> *refs)
{
	fRefs = refs;
}
	
void
AttributeView::InitStrings(const Model *model)
{
	if (fMultiple)
		return;
	
	BMimeType mime;
	char desc[B_MIME_TYPE_LENGTH];

	ASSERT(model->IsNodeOpen());

	BRect drawBounds(Bounds());
	drawBounds.left = fDivider;
	// We'll do our own truncation later on in Draw()...
	WidgetAttributeText::AttrAsString(model, &fCreatedStr, kAttrStatCreated,
		B_TIME_TYPE, drawBounds.Width() - kBorderMargin, this);
	WidgetAttributeText::AttrAsString(model, &fModifiedStr, kAttrStatModified,
		B_TIME_TYPE, drawBounds.Width() - kBorderMargin, this);
	WidgetAttributeText::AttrAsString(model, &fPathStr, kAttrPath,
		B_STRING_TYPE, 0, this);

	// Use the same method as used to resolve fIconModel, which handles
	// both absolute and relative symlinks. if the link is broken, try to
	// get a little more information.
	if (model->IsSymLink()) {
		bool linked = false;
		
		Model resolvedModel(model->EntryRef(), true, true);
		if (resolvedModel.InitCheck() == B_OK) {
			// Get the path of the link
			BPath traversedPath;
			resolvedModel.GetPath(&traversedPath);

			// If the BPath is initialized, then check the file for existence
			if (traversedPath.InitCheck() == B_OK) {
				const char *pathString = traversedPath.Path();
				fLinkToStr = pathString;
				
				BFile bFile(pathString, B_READ_ONLY);
				if (B_OK != bFile.InitCheck()) {
					// the file does not exist. So show the link, but annotate it
					// with the word (broken)
					fLinkToStr += LOCALE(" (broken)");
				}
				linked = true;
			}	
		}
		
		if (!linked) {
			// since the link was not resolved, it must be broken, so get its
			// target and again annotate with (broken). It may be absolute or relative.
			BSymLink symLink(model->EntryRef());
			char linkToPath[B_PATH_NAME_LENGTH];
			symLink.ReadLink(linkToPath, B_PATH_NAME_LENGTH);
			fLinkToStr = linkToPath;
			fLinkToStr += LOCALE(" (broken)");	// link points to missing file
		}
	}

	if (mime.SetType(model->MimeType()) == B_OK
		&& mime.GetShortDescription(desc) == B_OK)
		fDescStr = desc;

	if (fDescStr.Length() == 0)
		fDescStr = model->MimeType();
}

void
AttributeView::AttachedToWindow()
{
	BFont font(be_plain_font);

	font.SetSpacing(B_BITMAP_SPACING);
	SetFont(&font);

	CheckAndSetSize();
	if (fPreferredAppMenu)
		fPreferredAppMenu->Menu()->SetTargetForItems(this);
	
	if (fMultiple)
		fButton->SetTarget(this);
	
	_inherited::AttachedToWindow();
}


void
AttributeView::Pulse()
{
	CheckAndSetSize();
	_inherited::Pulse();
}

void
AttributeView::ModelChanged(Model *model, BMessage *message)
{
	BRect drawBounds(Bounds());
	drawBounds.left = fDivider;

	switch (message->FindInt32("opcode")) {
		case B_ENTRY_MOVED:
			{
				node_ref dirNode;
				node_ref itemNode;
				dirNode.device = itemNode.device = message->FindInt32("device");
				message->FindInt64("to directory", &dirNode.node);
				message->FindInt64("node", &itemNode.node);
	
				const char *name;
				if (message->FindString("name", &name) != B_OK)
					return;

				// ensure notification is for us
				if (*model->NodeRef() == itemNode
					// For volumes, the device ID is obviously not handled in a
					// consistent way; the node monitor sends us the ID of the parent
					// device, while the model is set to the device of the volume
					// directly - this hack works for volumes that are mounted in
					// the root directory
					|| model->IsVolume()
					&& itemNode.device == 1
					&& itemNode.node == model->NodeRef()->node) {
					model->UpdateEntryRef(&dirNode, name);
					BString title;
					title << name << LOCALE(" info");
					Window()->SetTitle(title.String());
					WidgetAttributeText::AttrAsString(model, &fPathStr, kAttrPath, B_STRING_TYPE, 0, this);
				}
				break;
			}
		
		case B_STAT_CHANGED: 
			if (model->OpenNode() == B_OK) {
				WidgetAttributeText::AttrAsString(model, &fCreatedStr,
					kAttrStatCreated, B_TIME_TYPE, drawBounds.Width() - kBorderMargin, this);
				WidgetAttributeText::AttrAsString(model, &fModifiedStr,
					kAttrStatModified, B_TIME_TYPE, drawBounds.Width() - kBorderMargin, this);

				// don't change the size if it's a directory
				if (!model->IsDirectory()) {
					fLastSize = model->StatBuf()->st_size;
					fSizeStr = "";
					BInfoWindow::GetSizeString(fSizeStr, fLastSize, 0);
				}
				model->CloseNode();
			}
			break;

		case B_ATTR_CHANGED:
			{
				// watch for icon updates
				const char *attrName;
				if (message->FindString("attr", &attrName) == B_OK) {
					if (strcmp(attrName, kAttrLargeIcon) == 0) 
						Invalidate(BRect(10, 10, 10 + B_LARGE_ICON, 10 + B_LARGE_ICON));
	
					if (strcmp(attrName, kAttrMIMEType) == 0) {
						if (model->OpenNode() == B_OK) {
							InitStrings(model);
							model->CloseNode();
						}
						Invalidate();
					}
				}
				break;
			}
		
		default:
			break;
	}
	
	// Update the icon stuff
	if (fIconModel != fModel) {
		delete fIconModel;
		fIconModel = NULL;
	}

	fModel = model;
	if (fModel->IsSymLink()) {
		// if we are looking at a symlink, deference the model and look at the
		// target
		Model *resolvedModel = new Model(model->EntryRef(), true, true);
		if (resolvedModel->InitCheck() == B_OK) {
			if (fIconModel != fModel)
				delete fIconModel;
			fIconModel = resolvedModel;
		} else {
			fIconModel = model;
			delete resolvedModel;
		}
		InitStrings(model);
		Invalidate();
	}

	drawBounds.left = fDivider;
	Invalidate(drawBounds);
}

void
AttributeView::ResizeToPreferred()
{
}

// This only applies to symlinks. If the target of the symlink
// was changed, then we have to update the entire model.
// (Since in order to re-target a symlink, we had to delete
// the old model and create a new one.... BSymLink::SetTarget(),
// would be nice...)
void
AttributeView::ReLinkTargetModel(Model *model)
{
	fModel = model;
	if (fModel->IsSymLink()) {
		Model *resolvedModel = new Model(model->EntryRef(), true, true);
		if (resolvedModel->InitCheck() == B_OK) {
			if (fIconModel != fModel)
				delete fIconModel;
			fIconModel = resolvedModel;
		} else {
			fIconModel = fModel;
			delete resolvedModel;
		}
	}
	InitStrings(model);
	Invalidate(Bounds());
}

void
AttributeView::MouseDown(BPoint point)
{
	// Make sure this isn't the trash directory...
	BEntry entry;
	if (!fMultiple)
		fModel->GetEntry(&entry);
	// Assume this isn't part of a double click...
	fDoubleClick = false;
	// Start tracking the mouse if we are in any of the hotspots
	if (fLinkRect.Contains(point)) {
		InvertRect(fLinkRect);
		fTrackingState = link_track;
	} else if (fPathRect.Contains(point)) {
		InvertRect(fPathRect);
		fTrackingState = path_track;
	} else if (fTitleRect.Contains(point)) {
		// You can't change the name of the trash
		if (!fMultiple
			&& !TFSContext::IsTrashDir(entry)
			&& ConfirmChangeIfWellKnownDirectory(&entry, "rename", true)
			&& fTitleEditView == 0)
			BeginEditingTitle();
	} else if (fTitleEditView) {
		FinishEditingTitle(true);
	} else if (fSizeRect.Contains(point)) {
		if (fMultiple || (fModel->IsDirectory() && !fModel->IsVolume())) {
			InvertRect(fSizeRect);
			fTrackingState = size_track;
		} else
			fTrackingState = no_track;
	} else if (fIconRect.Contains(point)) {
		uint32 buttons;
		Window()->CurrentMessage()->FindInt32("buttons", (int32 *)&buttons);
		if (((modifiers() & B_CONTROL_KEY) != 0) || (buttons & B_SECONDARY_MOUSE_BUTTON) != 0) {
			// Show contextual menu
			BPopUpMenu *contextMenu = new BPopUpMenu("FileContext", false, false);
			BuildContextMenu(contextMenu);
			if (contextMenu) {
				contextMenu->SetAsyncAutoDestruct(true);
				contextMenu->Go(ConvertToScreen(point), true, true, ConvertToScreen(fIconRect));
			}
		} else {
			// Check to see if the point is actually on part of the icon,
			// versus just in the container rect. The icons are always
			// the large version...
			BPoint offsetPoint;
			offsetPoint.x = point.x - fIconRect.left;
			offsetPoint.y = point.y - fIconRect.top;
			if (IconCache::sIconCache->IconHitTest(offsetPoint, fIconModel, kNormalIcon, B_LARGE_ICON)) {
				// Can't drag the trash anywhere..
				fTrackingState = (fMultiple || TFSContext::IsTrashDir(entry) ? open_only_track : icon_track);
				// Check for possible double click...
				if (abs((int32)(fClickPoint.x - point.x)) < kDragSlop
					&& abs((int32)(fClickPoint.y - point.y)) < kDragSlop) {
					int32 clickCount;
					Window()->CurrentMessage()->FindInt32("clicks", &clickCount);
					// This checks the *previous* click point...
					if (clickCount == 2) {
						offsetPoint.x = fClickPoint.x - fIconRect.left;
						offsetPoint.y = fClickPoint.y - fIconRect.top;
						fDoubleClick = IconCache::sIconCache->IconHitTest(offsetPoint,
							fIconModel, kNormalIcon, B_LARGE_ICON);
					}
				}
			}
		}
	}
	fClickPoint = point;
	fMouseDown = true;
	SetMouseEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
}

void
AttributeView::MouseMoved(BPoint point, uint32, const BMessage *message)
{
	// Highlight Drag target...
	if (!fMultiple && message && message->ReturnAddress() != BMessenger(this)
		&& message->what == B_SIMPLE_DATA
		&& BPoseView::CanHandleDragSelection(fModel, message, (modifiers() & B_CONTROL_KEY) != 0)) {
		bool overTarget = fIconRect.Contains(point);
		SetDrawingMode(B_OP_OVER);
		if (overTarget != fIsDropTarget) {
			IconCache::sIconCache->Draw(fIconModel, this, fIconRect.LeftTop(),
				overTarget ? kSelectedIcon : kNormalIcon, B_LARGE_ICON, true);
			fIsDropTarget = overTarget;
		}
	}
	
	switch (fTrackingState) {
		case link_track: 
			if (fLinkRect.Contains(point) != fMouseDown) {
				fMouseDown = !fMouseDown;
				InvertRect(fLinkRect);
			}
			break;

		case path_track: 
			if (fPathRect.Contains(point) != fMouseDown) {
				fMouseDown = !fMouseDown;
				InvertRect(fPathRect);
			}
			break;

		case size_track: 
			if (fSizeRect.Contains(point) != fMouseDown) {
				fMouseDown = !fMouseDown;
				InvertRect(fSizeRect);
			}
			break;

		case icon_track:
			if (!fMultiple && fMouseDown && !fDragging && (abs((int32)(point.x - fClickPoint.x)) > kDragSlop
				|| abs((int32)(point.y - fClickPoint.y)) > kDragSlop)) {
				// Find the required height
				BFont font;
				GetFont(&font);
				font.SetSize(kAttribFontHeight);
				
				float height = CurrentFontHeight(kAttribFontHeight) + fIconRect.Height() + 8;
				BRect rect(0, 0, min_c(fIconRect.Width()
					+ font.StringWidth(fModel->Name()) + 4, fIconRect.Width() * 3), height);
				BBitmap *dragBitmap = new BBitmap(rect, B_RGBA32, true);
				dragBitmap->Lock();
				BView *view = new BView(dragBitmap->Bounds(), "", B_FOLLOW_NONE, 0);
				dragBitmap->AddChild(view);
				view->SetOrigin(0, 0);
				BRect clipRect(view->Bounds());
				BRegion newClip;
				newClip.Set(clipRect);
				view->ConstrainClippingRegion(&newClip);
		
				// Transparent draw magic
				view->SetHighColor(0, 0, 0, 0);
				view->FillRect(view->Bounds());
				view->SetDrawingMode(B_OP_ALPHA);
				view->SetHighColor(0, 0, 0, 128);	// set the level of transparency by  value
				view->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_COMPOSITE);

				// Draw the icon
				float hIconOffset = (rect.Width() - fIconRect.Width()) / 2;
				IconCache::sIconCache->Draw(fIconModel, view, BPoint(hIconOffset, 0),
					kNormalIcon, B_LARGE_ICON, true);
				// See if we need to truncate the string...
				BString nameString(fMultiple ? LOCALE("Multiple Files") : fModel->Name());
				if (view->StringWidth(nameString.String()) > rect.Width()) 
					view->TruncateString(&nameString, B_TRUNCATE_END, rect.Width() - 5);

				// Draw the label
				font_height fontHeight;
				font.GetHeight(&fontHeight);
				float leftText = (view->StringWidth(nameString.String()) - fIconRect.Width()) / 2;
				view->MovePenTo(BPoint(hIconOffset - leftText + 2,
					fIconRect.Height() + (fontHeight.ascent + 2)));
				view->DrawString(nameString.String());

				view->Sync();
				dragBitmap->Unlock();
		
				BMessage message(B_REFS_RECEIVED);
				message.AddPoint("click_pt", fClickPoint);
				BPoint tmpLoc;
				uint32 button;
				GetMouse(&tmpLoc, &button);
				if (button)
					message.AddInt32("buttons", (int32)button);
	
				message.AddInt32("be:actions",
					(modifiers() & B_OPTION_KEY) != 0 ? B_COPY_TARGET : B_MOVE_TARGET);
				message.AddRef("refs", fModel->EntryRef());
				DragMessage(&message, dragBitmap, B_OP_ALPHA, BPoint((fClickPoint.x - fIconRect.left)
					+ hIconOffset, fClickPoint.y - fIconRect.top), this);
				fDragging = true;
			}
			break;

		case open_only_track :
			// Special type of entry that can't be renamed or drag and dropped...
			// It can only be opened by double clicking on the icon...
			break;

		default:
			{
				// Only consider this if the window is the active window..
				// We have to manually get the mouse here in the event that the
				// mouse is over a pop-up window...
				uint32 buttons;
				BPoint point;
				GetMouse(&point, &buttons);
				if (Window()->IsActive() && !buttons) {
					// If we are down here, then that means that we're tracking the mouse
					// but not from a mouse down. In this case, we're just interested in
					// knowing whether or not we need to display the "pop-up" version
					// of the path or link text....
					BRect screen(BScreen(B_MAIN_SCREEN_ID).Frame());
					BFont font;
					GetFont(&font);
					if (fPathRect.Contains(point)
						&& StringWidth(fPathStr.String()) > (Bounds().Width() - (fDivider + kBorderMargin))) {
						fTrackingState = no_track;
						BRect rect(fPathRect);
						rect.right = rect.left + StringWidth(fPathStr.String()) + 4;
						rect.OffsetBy(Window()->Frame().left, Window()->Frame().top);
						if (rect.left < 0)
							rect.OffsetBy(rect.left * -1, 0);
						else if (rect.right > screen.right)
							rect.OffsetBy(screen.right - rect.right, 0);
						fPathWindow = new BWindow(rect, "fPathWindow",
							B_BORDERED_WINDOW, B_AVOID_FRONT | B_AVOID_FOCUS);
						/* FIXTHIS */
						fPathWindow->AddChild(new TrackingView(fPathWindow->Bounds(),
							fModel, fPathStr.String(), &font));
						fPathWindow->Sync();
						fPathWindow->Show();
					} else if (fLinkRect.Contains(point)
						&& StringWidth(fLinkToStr.String()) > (Bounds().Width() - (fDivider + kBorderMargin))) {
						fTrackingState = no_track;
						BRect rect(fLinkRect);
						rect.right = rect.left + StringWidth(fLinkToStr.String()) + 4;
						rect.OffsetBy(Window()->Frame().left, Window()->Frame().top);
						if (rect.left < 0)
							rect.OffsetBy(rect.left * -1, 0);
						else if (rect.right > screen.right)
							rect.OffsetBy(screen.right - rect.right, 0);
						fLinkWindow = new BWindow(rect, "fLinkWindow",
							B_BORDERED_WINDOW, B_AVOID_FRONT | B_AVOID_FOCUS);
						/* FIXTHIS */
						fLinkWindow->AddChild(new TrackingView(fLinkWindow->Bounds(),
							fModel, fLinkToStr.String(), &font));
						fLinkWindow->Sync();
						fLinkWindow->Show();
					}
				}
				break;
			}
	}
}

namespace BPrivate {

void
OpenParentAndSelectOriginal(const entry_ref *ref)
{
	BEntry entry(ref);
	node_ref node;
	entry.GetNodeRef(&node);
	
	BEntry parent;
	entry.GetParent(&parent);
	entry_ref parentRef;
	parent.GetRef(&parentRef);

	BMessage message(B_REFS_RECEIVED);
	message.AddRef("refs", &parentRef);
	message.AddData("nodeRefToSelect", B_RAW_TYPE, &node, sizeof(node_ref));
	
	be_app->PostMessage(&message);
}

}

void
AttributeView::MouseUp(BPoint point)
{
	// Are we in the link rect?
	if (fTrackingState == link_track && fLinkRect.Contains(point)) {
		InvertRect(fLinkRect);

		BSymLink symLink(fModel->EntryRef());
		char linkToPath[B_PATH_NAME_LENGTH];
		symLink.ReadLink(linkToPath, B_PATH_NAME_LENGTH);
		// Check for broken links
		BEntry entry(linkToPath);
		if (entry.InitCheck() == B_OK && !entry.Exists()) {
			// Open a file dialog panel to allow the user to relink.
			// For convenience open the parent folder of the previous
			// target if possible
			BInfoWindow *window = dynamic_cast<BInfoWindow *>(Window());
			if (window) {
				BPath path = fModel->EntryRef();
				BPath parent;
				path.GetParent(&parent);
				BEntry entry = parent.Path();
				entry_ref ref;
				entry.GetRef(&ref);
				window->OpenFilePanel(&ref);
			}
			
		} else {
			entry_ref ref;
			entry.GetRef(&ref);
			OpenParentAndSelectOriginal(&ref);	
		}
	} else if (fTrackingState == path_track && fPathRect.Contains(point)) {
		InvertRect(fPathRect);
		OpenParentAndSelectOriginal(fModel->EntryRef());	
	} else if ((fTrackingState == icon_track || fTrackingState == open_only_track)
		&& fIconRect.Contains(point)) {
		// If it was a double click, then tell Tracker to open the item...
		// The CurrentMessage() here does *not* have a "clicks" field,
		// which is why we are tracking the clicks with this temp var...
		if (fDoubleClick){
			// Double click, launch.
			BMessage message(B_REFS_RECEIVED);
			message.AddRef("refs", fModel->EntryRef());
		
			// add a messenger to the launch message that will be used to
			// dispatch scripting calls from apps to the PoseView
			message.AddMessenger("TrackerViewToken", BMessenger(this));
			be_app->PostMessage(&message);
			fDoubleClick = false;
		}
	} else if (fTrackingState == size_track && fSizeRect.Contains(point)) {
		// Recalculate size
		Window()->PostMessage(kRecalculateSize);
	}
	
	// End mouse tracking...
	fMouseDown = false;
	fDragging = false;
	fTrackingState = no_track;
	
} // end of MouseUp()

void
AttributeView::CheckAndSetSize()
{
	/* FIXTHIS */
	if (fMultiple)
		return;
	
	if (fModel->IsVolume()) {
		BVolume volume(fModel->NodeRef()->device);
		off_t freeBytes = volume.FreeBytes();
		if (fFreeBytes == freeBytes)
			return;
		
		fFreeBytes = freeBytes;
		off_t capacity = volume.Capacity();
		
		char buffer[500];
		if (capacity >= kGBSize)
			sprintf(buffer, LOCALE("%.1f GB (%.1f MB used -- %.1f MB free)"),
				(float)capacity / kGBSize,
				(float)(capacity - fFreeBytes) / kMBSize,
				(float)fFreeBytes / kMBSize);
		else
			sprintf(buffer, LOCALE("%.1f MB (%.1f MB used -- %.1f MB free)"),
				(float)capacity / kMBSize,
				(float)(capacity - fFreeBytes) / kMBSize,
				(float)fFreeBytes / kMBSize);
	
		fSizeStr = buffer;
	} else if (fModel->IsFile()) {
		// poll for size changes because they do not get node monitored
		// untill a file gets closed
		StatStruct statBuf;
		BModelOpener opener(fModel);

		if (fModel->InitCheck() != B_OK || fModel->Node()->GetStat(&statBuf) != B_OK)
			return;
		
		if (fLastSize == statBuf.st_size)
			return;

		fLastSize = statBuf.st_size;
		fSizeStr = "";
		BInfoWindow::GetSizeString(fSizeStr, fLastSize, 0);

	} else
		return;

	BRect bounds(Bounds());
	float lineHeight = CurrentFontHeight() + 2;
	bounds.Set(fDivider, fIconRect.bottom, bounds.right, fIconRect.bottom + lineHeight);
	Invalidate(bounds);
}

void
AttributeView::MessageReceived(BMessage *message)
{
	if (!fMultiple && message->WasDropped()
		&& message->what == B_SIMPLE_DATA
		&& message->ReturnAddress() != BMessenger(this)
		&& fIconRect.Contains(ConvertFromScreen(message->DropPoint()))
		&& BPoseView::CanHandleDragSelection(fModel, message,  (modifiers() & B_CONTROL_KEY) != 0)) {
		BPoseView::HandleDropCommon(message, fModel, 0, this, message->DropPoint());
		Invalidate(fIconRect);
		return;
	}

	switch (message->what) {
		case kSetPreferredApp:
			{
				const char *newSignature;
				if (message->FindString("signature", &newSignature) != B_OK)
					newSignature = NULL;
				
				if (!fMultiple) {
					BNode node(fModel->EntryRef());
					BNodeInfo nodeInfo(&node);
					
					fModel->SetPreferredAppSignature(newSignature);
					nodeInfo.SetPreferredApp(newSignature);
				} else {
					for (int index = 0; index < fRefs->CountItems(); index++) {
						BNode node(fRefs->ItemAt(index));
						BNodeInfo nodeInfo(&node);
						nodeInfo.SetPreferredApp(newSignature);
					}
				}
				break;
			}
		case kShowMultiple:
			{
				uint32 what = kGetInfo;
				if (!fExpanded) {
					fExpanded = true;
					fButton->SetLabel(LOCALE("Close"));
				} else {
					fExpanded = false;
					fButton->SetLabel(LOCALE("Multi"));
					what = kCloseInfo;
				}
				
				BMessage *message = new BMessage(what);
				for (int index = 0; index < fRefs->CountItems(); index++)
					message->AddRef("refs", fRefs->ItemAt(index));
				message->AddBool("multiple", false);
				be_app->PostMessage(message);
				break;
			}
		default:
			_inherited::MessageReceived(message);
	}
}

void
AttributeView::Draw(BRect)
{
	// Set the low color for anti-aliasing
	SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	// Clear the old contents...
	SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	FillRect(Bounds());
	
	// Draw the dark grey area on the left...
	BRect drawBounds(Bounds());
	drawBounds.right = kBorderWidth;
	SetHighColor(kDarkBorderColor);
	FillRect(drawBounds);
		
	// Draw the icon, straddling the border...
	if (!fMultiple) {
		SetDrawingMode(B_OP_OVER);
		IconCache::sIconCache->Draw(fIconModel, this, fIconRect.LeftTop(),
			kNormalIcon, B_LARGE_ICON, true);
	}
		
	// Font information...
	font_height fontMetrics;
	BFont currentFont;
	float lineHeight = 0;
	float lineBase = 0;
	// Draw the main title if the user is not currently editing it...
	if (fTitleEditView == NULL) {	
		SetFont(be_bold_font);
		SetFontSize(kTitleFontHeight);
		GetFont(&currentFont);
		currentFont.GetHeight(&fontMetrics);
		lineHeight = CurrentFontHeight() + 5;
		lineBase = fTitleRect.bottom - fontMetrics.descent;
		SetHighColor(kAttrValueColor);
		MovePenTo(BPoint(fIconRect.right + 6, lineBase));
	
		BString nameString(fMultiple ? LOCALE("Multiple Files") : fModel->Name());
		// Recalculate the rect width
		fTitleRect.right = min_c(fTitleRect.left + currentFont.StringWidth(nameString.String()),
			Bounds().Width() - 5);
		// Check for possible need of truncation
		if (StringWidth(nameString.String()) > fTitleRect.Width()) {
			TruncateString(&nameString, B_TRUNCATE_END,
				fTitleRect.Width() - 2);
			DrawString(nameString.String());
		} else
			DrawString(nameString.String());
	}
			
	// Draw the attribute font stuff
	SetFont(be_plain_font);
	SetFontSize(kAttribFontHeight);
	GetFontHeight(&fontMetrics);
	lineHeight = CurrentFontHeight() + 5;

	// Starting base line for the first string...
	lineBase = fTitleRect.bottom + lineHeight;
	
	// Capacity/size
	SetHighColor(kAttrTitleColor);
	if (!fMultiple && fModel->IsVolume()) {
		MovePenTo(BPoint(fDivider - (StringWidth(LOCALE("Capacity:"))), lineBase));
		DrawString(LOCALE("Capacity:"));
	} else {
		MovePenTo(BPoint(fDivider - (StringWidth(LOCALE("Size:"))), lineBase));
		fSizeRect.left = fDivider + 2;
		fSizeRect.top = lineBase - fontMetrics.ascent;
		fSizeRect.bottom = lineBase + fontMetrics.descent;
		DrawString(LOCALE("Size:"));
	}

	MovePenTo(BPoint(fDivider + kDrawMargin, lineBase));
	SetHighColor(kAttrValueColor);
	// Check for possible need of truncation
	if (StringWidth(fSizeStr.String()) > (Bounds().Width() - (fDivider + kBorderMargin))) {
		BString tmpString(fSizeStr.String());
		TruncateString(&tmpString, B_TRUNCATE_MIDDLE,
			Bounds().Width() - (fDivider + kBorderMargin));
		DrawString(tmpString.String());
		fSizeRect.right = fSizeRect.left + StringWidth(tmpString.String()) + 3;
	} else {
		DrawString(fSizeStr.String());
		fSizeRect.right = fSizeRect.left + StringWidth(fSizeStr.String()) + 3;
	}
	lineBase += lineHeight;

	// Created
	SetHighColor(kAttrTitleColor);	
	MovePenTo(BPoint(fDivider - (StringWidth(LOCALE("Created:"))), lineBase));
	SetHighColor(kAttrTitleColor);
	DrawString(LOCALE("Created:"));
	MovePenTo(BPoint(fDivider + kDrawMargin, lineBase));
	SetHighColor(kAttrValueColor);
	DrawString(fCreatedStr.String());
	lineBase += lineHeight;

	// Modified
	MovePenTo(BPoint(fDivider - (StringWidth(LOCALE("Modified:"))), lineBase));
	SetHighColor(kAttrTitleColor);
	DrawString(LOCALE("Modified:"));
	MovePenTo(BPoint(fDivider + kDrawMargin, lineBase));
	SetHighColor(kAttrValueColor);
	DrawString(fModifiedStr.String());
	lineBase += lineHeight;

	// Description
	MovePenTo(BPoint(fDivider - (StringWidth(LOCALE("Kind:"))), lineBase));
	SetHighColor(kAttrTitleColor);
	DrawString(LOCALE("Kind:"));
	MovePenTo(BPoint(fDivider + kDrawMargin, lineBase));
	SetHighColor(kAttrValueColor);
	DrawString(fDescStr.String());
	lineBase += lineHeight;

	BFont normalFont;
	GetFont(&normalFont);

	// Path
	MovePenTo(BPoint(fDivider - (StringWidth(LOCALE("Path:"))), lineBase));
	SetHighColor(kAttrTitleColor);
	DrawString(LOCALE("Path:"));

	MovePenTo(BPoint(fDivider + kDrawMargin, lineBase));
	SetHighColor(kLinkColor);

	// Check for truncation...
	if (StringWidth(fPathStr.String()) > (Bounds().Width() - (fDivider + kBorderMargin))) {
		BString nameString(fPathStr.String());
		TruncateString(&nameString, B_TRUNCATE_MIDDLE,
			Bounds().Width() - (fDivider + kBorderMargin));
		DrawString(nameString.String());
	} else 
		DrawString(fPathStr.String());


	// Cache the position of the path
	fPathRect.top = lineBase - fontMetrics.ascent;
	fPathRect.bottom = lineBase + fontMetrics.descent;
	fPathRect.left = fDivider + 2;
	fPathRect.right = fPathRect.left + StringWidth(fPathStr.String()) + 3;

	lineBase += lineHeight;
	
	/* FIXTHIS */
	if (fMultiple)
		return;
	
	// Link to/version
	if (fModel->IsSymLink()) {
		MovePenTo(BPoint(fDivider - (StringWidth(LOCALE("Link To:"))), lineBase));
		SetHighColor(kAttrTitleColor);
		DrawString(LOCALE("Link To:"));
		MovePenTo(BPoint(fDivider + kDrawMargin, lineBase));
		SetHighColor(kLinkColor);
		// Check for truncation...
		if (StringWidth(fLinkToStr.String()) > (Bounds().Width() - (fDivider + kBorderMargin))) {
			BString nameString(fLinkToStr.String());
			TruncateString(&nameString, B_TRUNCATE_MIDDLE, 
				Bounds().Width() - (fDivider + kBorderMargin));
			DrawString(nameString.String());
		} else
			DrawString(fLinkToStr.String());

		// Cache the position of the link field
		fLinkRect.top = lineBase - fontMetrics.ascent;
		fLinkRect.bottom = lineBase + fontMetrics.descent;
		fLinkRect.left = fDivider + 2;
		fLinkRect.right = fLinkRect.left + StringWidth(fLinkToStr.String()) + 3;
	} else if (fModel->IsExecutable()) {
		MovePenTo(BPoint(fDivider - (StringWidth(LOCALE("Version:"))), lineBase));
		SetHighColor(kAttrTitleColor);
		DrawString(LOCALE("Version:"));
		BString buffer;
		MovePenTo(BPoint(fDivider + kDrawMargin, lineBase));
		SetHighColor(kAttrValueColor);
		if (fModel->GetLongVersionString(buffer, B_APP_VERSION_KIND) == B_OK) {
			if (StringWidth(buffer.String()) > (Bounds().Width() - (fDivider + kBorderMargin))) {
				BString nameString(buffer.String());
				TruncateString(&nameString, B_TRUNCATE_MIDDLE, 
					Bounds().Width() - (fDivider + kBorderMargin));
				DrawString(nameString.String());
			} else 
				DrawString(buffer.String());
		} else
			DrawString("-");

		// No link field
		fLinkRect = BRect(-1, -1, -1, -1);
	}
}

void
AttributeView::BeginEditingTitle()
{
	if (fTitleEditView != NULL)
		return;

	BFont font;
	GetFont(&font);
	font.SetSize(kTitleFontHeight);
	BRect textFrame(fTitleRect);
	textFrame.right = Bounds().Width() - 5;
	BRect textRect(textFrame);
	textRect.OffsetTo(0, 0);
	textRect.InsetBy(1, 1);
	// Just make it some really large size, since we don't do any line wrapping.
	// The text filter will make sure to scroll the cursor into position...
	textRect.right = 2000;
	fTitleEditView = new BTextView(textFrame, "text_editor",
		textRect, &font, 0, B_FOLLOW_ALL, B_WILL_DRAW);
	fTitleEditView->SetText(fModel->Name());
	// Reset the width of the text rect
	textRect = fTitleEditView->TextRect();
	textRect.right = fTitleEditView->LineWidth() + 20;
	fTitleEditView->SetTextRect(textRect);
	fTitleEditView->SetWordWrap(false);
	// Add filter for catching B_RETURN and B_ESCAPE key's
	fTitleEditView->AddFilter(new BMessageFilter(B_KEY_DOWN, AttributeView::TextViewFilter));
	
	BScrollView *scrollView = new BScrollView("BorderView",
		fTitleEditView, 0, 0, false, false, B_PLAIN_BORDER);
	AddChild(scrollView);	 
	fTitleEditView->SelectAll();
	fTitleEditView->MakeFocus();	

	Window()->UpdateIfNeeded();
}

void
AttributeView::FinishEditingTitle(bool commit)
{
	if (fTitleEditView == NULL)
		return;

	bool reopen = false;
	
	const char *text = fTitleEditView->Text();
	if (commit && strcmp(text, fModel->Name()) != 0) {
		BEntry entry(fModel->EntryRef());
		BDirectory parent;
		if (entry.InitCheck() == B_OK
			&& entry.GetParent(&parent) == B_OK) {
			if (parent.Contains(text)) {
				(new BAlert("", LOCALE("That name is already taken. "
					"Please type another one."), LOCALE("OK"), 0, 0,
					B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go();
				reopen = true;
			} else {
				if (fModel->IsVolume()) {
					BVolume	vol(fModel->NodeRef()->device);
					if (vol.InitCheck() == B_OK)
						vol.SetName(text);
				} else
					entry.Rename(text);
				// Adjust the size of the text rect...
				BFont currentFont;
				GetFont(&currentFont);
				currentFont.SetSize(kTitleFontHeight);
				fTitleRect.right = min_c(fTitleRect.left
					+ currentFont.StringWidth(fTitleEditView->Text()), Bounds().Width() - 5);
			}
		}
	}
	
	// Remove view
	BView *scrollView = fTitleEditView->Parent();
	RemoveChild(scrollView);
	delete scrollView;
	fTitleEditView = NULL;

	if (reopen) 
		BeginEditingTitle();
}

void
AttributeView::MakeFocus(bool isFocus)
{
	if (!isFocus && fTitleEditView != NULL)
		FinishEditingTitle(true);
}

void
AttributeView::WindowActivated(bool isFocus)
{
	if (!isFocus && fTitleEditView != NULL)
		FinishEditingTitle(true);
}

float
AttributeView::CurrentFontHeight(float size)
{
	BFont font;
	GetFont(&font);
	if (size > -1)
		font.SetSize(size);
		
	font_height fontHeight;
	font.GetHeight(&fontHeight);
	
	return fontHeight.ascent + fontHeight.descent + fontHeight.leading + 2;
}

status_t
AttributeView::BuildContextMenu(BMenu *parent)
{
	/* FIXTHIS */
	if (fMultiple)
		return B_ERROR;
	
	// Add navigation menu if this is not a symlink...
	// Symlink's to directories are OK however!
	BEntry entry(fModel->EntryRef());
	entry_ref ref;
	entry.GetRef(&ref);
	Model model(&entry);
	bool navigate = false;
	if (model.InitCheck() == B_OK) {
		if (model.IsSymLink()) {
			// Check if it's to a directory...
			if (entry.SetTo(model.EntryRef(), true) == B_OK) {
				navigate = entry.IsDirectory();
				entry.GetRef(&ref);
			}
		} else if (model.IsDirectory() || model.IsVolume()) 
			navigate = true;
	}
	ModelMenuItem *navigationItem = NULL;	
	if (navigate) {
		navigationItem = new ModelMenuItem(new Model(model),
			new BNavMenu(model.Name(), B_REFS_RECEIVED, be_app, Window()));
	
		// setup a navigation menu item which will dynamically load items
		// as menu items are traversed
		BNavMenu *navMenu = dynamic_cast<BNavMenu *>(navigationItem->Submenu());
		navMenu->SetNavDir(&ref);
		navigationItem->SetLabel(model.Name());
		navigationItem->SetEntry(&entry);
	
		parent->AddItem(navigationItem, 0);
		parent->AddItem(new BSeparatorItem(), 1);

		BMessage *message = new BMessage(B_REFS_RECEIVED);
		message->AddRef("refs", &ref);
		navigationItem->SetMessage(message);
		navigationItem->SetTarget(be_app);
	}

	
	parent->AddItem(new BMenuItem(LOCALE("Open"), new BMessage(kOpenSelection), 'O'));

	if (!TFSContext::IsTrashDir(entry)) {
		parent->AddItem(new BMenuItem(LOCALE("Edit Name"), new BMessage(kEditItem), 'E'));
		if (fModel->IsVolume()) {
			parent->AddSeparatorItem();
			BMenuItem *item;
			parent->AddItem(item = new BMenuItem(LOCALE("Unmount"), new BMessage(kUnmountVolume), 'U'));
			// volume model, enable/disable the Unmount item
			BVolume boot;
			BVolumeRoster().GetBootVolume(&boot);
			BVolume volume;
			volume.SetTo(fModel->NodeRef()->device);
			if (volume == boot)
				item->SetEnabled(false);
			
		} else {
			parent->AddItem(new BMenuItem(LOCALE("Move to Trash"), new BMessage(kMoveToTrash), 'T'));
			parent->AddSeparatorItem();
			parent->AddItem(new BMenuItem(LOCALE("Identify"), new BMessage(kIdentifyEntry)));
		}
	} else if (TFSContext::IsTrashDir(entry)) 
		parent->AddItem(new BMenuItem(LOCALE("Empty Trash"), new BMessage(kEmptyTrash)));
	
	BMenuItem *sizeItem = NULL;
	if (model.IsDirectory() && !model.IsVolume()) 
		parent->AddItem(sizeItem = new BMenuItem(LOCALE("Recalculate Folder Size"),
			new BMessage(kRecalculateSize)));

	if (model.IsSymLink()) 
		parent->AddItem(sizeItem = new BMenuItem(LOCALE("Set new link target"),
			new BMessage(kSetLinkTarget)));

	parent->AddItem(new BSeparatorItem());
	parent->AddItem(new BMenuItem(LOCALE("Permissions"), new BMessage(kPermissionsSelected), 'P'));

	parent->SetFont(be_plain_font);
	parent->SetTargetForItems(this);
	// Reset the nav menu to be_app...
	if (navigate)
		navigationItem->SetTarget(be_app);
	if (sizeItem)
		sizeItem->SetTarget(Window());
			
	return B_OK;
}

void
AttributeView::SetPermissionsSwitchState(int32 state)
{
	fPermissionsSwitch->SetValue(state);
	fPermissionsSwitch->Invalidate();
}


filter_result
AttributeView::TextViewFilter(BMessage *message, BHandler **, BMessageFilter *filter)
{
	uchar key;
	AttributeView *attribView = static_cast<AttributeView *>(
		static_cast<BWindow *>(filter->Looper())->FindView("attr_view"));
	
	// Adjust the size of the text rect...
	BRect nuRect(attribView->TextView()->TextRect());
	nuRect.right = attribView->TextView()->LineWidth() + 20;
	attribView->TextView()->SetTextRect(nuRect);

	// Make sure the cursor is in view...
	attribView->TextView()->ScrollToSelection();		
	if (message->FindInt8("byte", (int8 *)&key) != B_OK)
		return B_DISPATCH_MESSAGE;

	if (key == B_RETURN || key == B_ESCAPE) {
		attribView->FinishEditingTitle(key == B_RETURN);
		return B_SKIP_MESSAGE;
	}

	return B_DISPATCH_MESSAGE;
}

off_t 
AttributeView::LastSize() const
{
	return fLastSize;
}

void 
AttributeView::SetLastSize(off_t lastSize)
{
	fLastSize = lastSize;
}

void 
AttributeView::SetSizeStr(const char *sizeStr)
{
	fSizeStr = sizeStr;
	
	BRect bounds(Bounds());
	float lineHeight = CurrentFontHeight(kAttribFontHeight) + 6;
	bounds.Set(fDivider, fIconRect.bottom, bounds.right, fIconRect.bottom + lineHeight);
	Invalidate(bounds);
}

TrackingView::TrackingView(BRect frame, const Model *model, const char *str, const BFont *font)
	:	BView(frame, "trackingView", B_FOLLOW_ALL, B_WILL_DRAW),
		fMouseDown(false),
		fMouseInView(false),
		fModel(model),
		fString(str),
		fFont(*font)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	SetEventMask(B_POINTER_EVENTS, 0);
}

void
TrackingView::MouseDown(BPoint)
{
	fMouseDown = true;
	fMouseInView = true;
	InvertRect(Bounds());
}

void
TrackingView::MouseMoved(BPoint, uint32 transit, const BMessage *)
{
	if ((transit == B_ENTERED_VIEW || transit == B_EXITED_VIEW) && fMouseDown)
		InvertRect(Bounds());
		
	fMouseInView = (transit == B_ENTERED_VIEW || transit == B_INSIDE_VIEW);
		
	if (!fMouseInView && !fMouseDown)
		Window()->Close();
}

void
TrackingView::MouseUp(BPoint)
{
	if (fMouseInView) {
		if (fModel->IsSymLink()) {
			BSymLink symLink(fModel->EntryRef());
			char linkToPath[B_PATH_NAME_LENGTH];
			symLink.ReadLink(linkToPath, B_PATH_NAME_LENGTH);
			// Check for broken links
			BEntry entry(linkToPath);
			if (entry.InitCheck() == B_OK && entry.Exists()) {
				entry_ref ref;
				entry.GetRef(&ref);
				OpenParentAndSelectOriginal(&ref);
			} else {
				BPath parent;
				BPath path = linkToPath;
				path.GetParent(&parent);
				entry.SetTo(parent.Path());
				if (entry.InitCheck() == B_OK && entry.Exists()) {
					entry_ref ref;
					entry.GetRef(&ref);
					BMessage message(B_REFS_RECEIVED);
					message.AddRef("refs", &ref);
					be_app->PostMessage(&message);
				}
			}
		} else 
			OpenParentAndSelectOriginal(fModel->EntryRef());
	}
	
	Window()->Close();
	fMouseDown = false;
}

void
TrackingView::Draw(BRect)
{
	SetHighColor(kLinkColor);
	SetLowColor(ViewColor());
	
	font_height fontHeight;
	fFont.GetHeight(&fontHeight);
	
	DrawString(fString.String(), BPoint(3, Bounds().Height() - fontHeight.descent));
}

