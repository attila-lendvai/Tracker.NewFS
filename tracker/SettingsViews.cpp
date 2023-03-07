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

#include "Commands.h"
#include "DeskWindow.h"
#include "IconTheme.h"
#include "LanguageTheme.h"
#include "LinkStringView.h"
#include "Model.h"
#include "SelectionWindow.h"
#include "SettingsViews.h"
#include "SequenceWindow.h"
#include "ThemePreView.h"
#include "Tracker.h"
#include "TrackerFilters.h"
#include "TrackerString.h"
#include "WidgetAttributeText.h"

#include <Box.h>
#include <Button.h>
#include <ListView.h>
#include <MenuField.h>
#include <MessageFilter.h>
#include <ColorControl.h>
#include <NodeMonitor.h>
#include <OptionPopUp.h>
#include <ScrollView.h>
#include <StringView.h>

const uint32 kSpaceBarSwitchColor = 'SBsc';

SettingsView::SettingsView(BRect rect, const char *name)
	:	BView(rect, name, B_FOLLOW_ALL_SIDES, 0)
{
}

SettingsView::~SettingsView()
{
}

// The inherited functions should set the default values
// and update the UI gadgets. The latter can by done by
// calling ShowCurrentSettings().
void SettingsView::SetDefaults() {}
	
// The inherited functions should set the values that was
// active when the settings window opened. It should also
// update the UI widgets accordingly, preferrable by calling
// ShowCurrentSettings().
void SettingsView::Revert() {}

// This function is called when the window is shown to let
// the settings views record the state to revert to.
void SettingsView::RecordRevertSettings() {}

// This function is used by the window to tell the view
// to display the current settings in the tracker.
void SettingsView::ShowCurrentSettings(bool) {}

// This function is used by the window to tell whether
// it can ghost the revert button or not. It it shows the
// reverted settings, this function should return true.
bool SettingsView::ShowsRevertSettings() const { return true; }

namespace BPrivate {
	const float kBorderSpacing = 5.0f;
	const float kItemHeight = 18.0f;
	const float kItemExtraSpacing = 2.0f;
	const float kIndentSpacing = 12.0f;
}

//------------------------------------------------------------------------
// #pragma mark -

DesktopSettingsView::DesktopSettingsView(BRect rect)
	:	SettingsView(rect, "DesktopSettingsView")
{
	BRect frame = BRect(kBorderSpacing, kBorderSpacing, rect.Width()
		- 2 * kBorderSpacing, kBorderSpacing + kItemHeight);
	
	fShowDisksIconRadioButton = new BRadioButton(frame, "", LOCALE("Show Disks Icon"),
		new BMessage(kShowDisksIconChanged));
	AddChild(fShowDisksIconRadioButton);
	fShowDisksIconRadioButton->ResizeToPreferred();
	
	const float itemSpacing = fShowDisksIconRadioButton->Bounds().Height() + kItemExtraSpacing;
	
	frame.OffsetBy(0, itemSpacing);
	
	fMountVolumesOntoDesktopRadioButton =
		new BRadioButton(frame, "", LOCALE("Show Volumes On Desktop"),
			new BMessage(kVolumesOnDesktopChanged));
	AddChild(fMountVolumesOntoDesktopRadioButton);
	fMountVolumesOntoDesktopRadioButton->ResizeToPreferred();
	
	frame.OffsetBy(20, itemSpacing);
	
	fMountSharedVolumesOntoDesktopCheckBox =
		new BCheckBox(frame, "", LOCALE("Show Shared Volumes On Desktop"),
			new BMessage(kVolumesOnDesktopChanged));
	AddChild(fMountSharedVolumesOntoDesktopCheckBox);
	fMountSharedVolumesOntoDesktopCheckBox->ResizeToPreferred();
	
	frame.OffsetBy(-20, 2 * itemSpacing);
	
	fIntegrateNonBootBeOSDesktopsCheckBox =
		new BCheckBox(frame, "", LOCALE("Integrate Non-Boot BeOS Desktops"),
			new BMessage(kDesktopIntegrationChanged));
	AddChild(fIntegrateNonBootBeOSDesktopsCheckBox);
	fIntegrateNonBootBeOSDesktopsCheckBox->ResizeToPreferred();
	
	frame.OffsetBy(0, itemSpacing);
	
	fEjectWhenUnmountingCheckBox =
		new BCheckBox(frame, "", LOCALE("Eject When Unmounting"),
			new BMessage(kEjectWhenUnmountingChanged));
	AddChild(fEjectWhenUnmountingCheckBox);
	fEjectWhenUnmountingCheckBox->ResizeToPreferred();
	
	frame.OffsetBy(0, itemSpacing);
	
	BButton *button =
		new BButton(BRect(kBorderSpacing, rect.Height() - kBorderSpacing - 20,
			kBorderSpacing + 100, rect.Height() - kBorderSpacing),
			"", LOCALE("Mount Settings"B_UTF8_ELLIPSIS), new BMessage(kRunAutomounterSettings));
	AddChild(button);		
	
	button->ResizeToPreferred();
	button->MoveBy(0, rect.Height() - kBorderSpacing - button->Frame().bottom);
	button->SetTarget(be_app);
}

void
DesktopSettingsView::AttachedToWindow()
{
	fShowDisksIconRadioButton->SetTarget(this);
	fMountVolumesOntoDesktopRadioButton->SetTarget(this);
	fMountSharedVolumesOntoDesktopCheckBox->SetTarget(this);
	fIntegrateNonBootBeOSDesktopsCheckBox->SetTarget(this);
	fEjectWhenUnmountingCheckBox->SetTarget(this);
}

void
DesktopSettingsView::MessageReceived(BMessage *message)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	switch (message->what) {		
		case kShowDisksIconChanged:
		{
			// Turn on and off related settings:
			fMountVolumesOntoDesktopRadioButton->SetValue(
				!fShowDisksIconRadioButton->Value() == 1);
			fMountSharedVolumesOntoDesktopCheckBox->SetEnabled(
				fMountVolumesOntoDesktopRadioButton->Value() == 1);
			
			// Set the new settings in the tracker:
			gTrackerSettings.SetShowDisksIcon(fShowDisksIconRadioButton->Value() == 1);
			gTrackerSettings.SetMountVolumesOntoDesktop(
				fMountVolumesOntoDesktopRadioButton->Value() == 1);
			gTrackerSettings.SetMountSharedVolumesOntoDesktop(
				fMountSharedVolumesOntoDesktopCheckBox->Value() == 1);
			
			// Construct the notification message:				
			BMessage notificationMessage;
			notificationMessage.AddBool("ShowDisksIcon",
				fShowDisksIconRadioButton->Value() == 1);
			notificationMessage.AddBool("MountVolumesOntoDesktop",
				fMountVolumesOntoDesktopRadioButton->Value() == 1);
			notificationMessage.AddBool("MountSharedVolumesOntoDesktop",
				fMountSharedVolumesOntoDesktopCheckBox->Value() == 1);
			
			// Send the notification message:
			tracker->SendNotices(kVolumesOnDesktopChanged, &notificationMessage);
			
			// Tell the settings window the contents have changed:
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		case kVolumesOnDesktopChanged:
		{
			// Turn on and off related settings:
			fShowDisksIconRadioButton->SetValue(
				!fMountVolumesOntoDesktopRadioButton->Value() == 1);
			fMountSharedVolumesOntoDesktopCheckBox->SetEnabled(
				fMountVolumesOntoDesktopRadioButton->Value() == 1);
			
			// Set the new settings in the tracker:
			gTrackerSettings.SetShowDisksIcon(fShowDisksIconRadioButton->Value() == 1);
			gTrackerSettings.SetMountVolumesOntoDesktop(
				fMountVolumesOntoDesktopRadioButton->Value() == 1);
			gTrackerSettings.SetMountSharedVolumesOntoDesktop(
				fMountSharedVolumesOntoDesktopCheckBox->Value() == 1);
			
			// Construct the notification message:				
			BMessage notificationMessage;
			notificationMessage.AddBool("ShowDisksIcon",
				fShowDisksIconRadioButton->Value() == 1);
			notificationMessage.AddBool("MountVolumesOntoDesktop",
				fMountVolumesOntoDesktopRadioButton->Value() == 1);
			notificationMessage.AddBool("MountSharedVolumesOntoDesktop",
				fMountSharedVolumesOntoDesktopCheckBox->Value() == 1);
			
			// Send the notification message:
			tracker->SendNotices(kVolumesOnDesktopChanged, &notificationMessage);
			
			// Tell the settings window the contents have changed:
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		case kDesktopIntegrationChanged:
		{
			// Set the new settings in the tracker:
			gTrackerSettings.SetIntegrateNonBootBeOSDesktops(
				fIntegrateNonBootBeOSDesktopsCheckBox->Value() == 1);
			
			// Construct the notification message:				
			BMessage notificationMessage;
			notificationMessage.AddBool("MountVolumesOntoDesktop",
				fMountVolumesOntoDesktopRadioButton->Value() == 1);
			notificationMessage.AddBool("MountSharedVolumesOntoDesktop",
				fMountSharedVolumesOntoDesktopCheckBox->Value() == 1);
			notificationMessage.AddBool("IntegrateNonBootBeOSDesktops",
				fIntegrateNonBootBeOSDesktopsCheckBox->Value() == 1);
			
			// Send the notification message:
			tracker->SendNotices(kDesktopIntegrationChanged, &notificationMessage);
			
			// Tell the settings window the contents have changed:
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		case kEjectWhenUnmountingChanged:
		{
			gTrackerSettings.SetEjectWhenUnmounting(
				fEjectWhenUnmountingCheckBox->Value() == 1);
			
			// Tell the settings window the contents have changed:
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		default:
			_inherited::MessageReceived(message);
	}
}
	
void
DesktopSettingsView::SetDefaults()
{
	// ToDo: Avoid the duplication of the default values.
	gTrackerSettings.SetShowDisksIcon(false);
	gTrackerSettings.SetMountVolumesOntoDesktop(true);
	gTrackerSettings.SetMountSharedVolumesOntoDesktop(false);
	gTrackerSettings.SetIntegrateNonBootBeOSDesktops(true);
	gTrackerSettings.SetEjectWhenUnmounting(true);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
DesktopSettingsView::Revert()
{
	gTrackerSettings.SetShowDisksIcon(fShowDisksIcon);
	gTrackerSettings.SetMountVolumesOntoDesktop(fMountVolumesOntoDesktop);
	gTrackerSettings.SetMountSharedVolumesOntoDesktop(fMountSharedVolumesOntoDesktop);
	gTrackerSettings.SetIntegrateNonBootBeOSDesktops(fIntegrateNonBootBeOSDesktops);
	gTrackerSettings.SetEjectWhenUnmounting(fEjectWhenUnmounting);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
DesktopSettingsView::ShowCurrentSettings(bool sendNotices)
{
	fShowDisksIconRadioButton->SetValue(gTrackerSettings.ShowDisksIcon());
	fMountVolumesOntoDesktopRadioButton->SetValue(gTrackerSettings.MountVolumesOntoDesktop());
	
	fMountSharedVolumesOntoDesktopCheckBox->SetValue(gTrackerSettings.MountSharedVolumesOntoDesktop());
	fMountSharedVolumesOntoDesktopCheckBox->SetEnabled(gTrackerSettings.MountVolumesOntoDesktop());
	
	fIntegrateNonBootBeOSDesktopsCheckBox->SetValue(gTrackerSettings.IntegrateNonBootBeOSDesktops());
	
	fEjectWhenUnmountingCheckBox->SetValue(gTrackerSettings.EjectWhenUnmounting());
	
	if (sendNotices) {
		TTracker *tracker = dynamic_cast<TTracker *>(be_app);
		if (!tracker)
			return;
		
		// Construct the notification message:				
		BMessage notificationMessage;
		notificationMessage.AddBool("ShowDisksIcon",
			fShowDisksIconRadioButton->Value() == 1);
		notificationMessage.AddBool("MountVolumesOntoDesktop",
			fMountVolumesOntoDesktopRadioButton->Value() == 1);
		notificationMessage.AddBool("MountSharedVolumesOntoDesktop",
			fMountSharedVolumesOntoDesktopCheckBox->Value() == 1);
		notificationMessage.AddBool("IntegrateNonBootBeOSDesktops",
			fIntegrateNonBootBeOSDesktopsCheckBox->Value() == 1);
		
		// Send notices to the tracker about the change:
		tracker->SendNotices(kVolumesOnDesktopChanged, &notificationMessage);
		tracker->SendNotices(kDesktopIntegrationChanged, &notificationMessage);
	}
}

void
DesktopSettingsView::RecordRevertSettings()
{
	fShowDisksIcon = gTrackerSettings.ShowDisksIcon();
	fMountVolumesOntoDesktop = gTrackerSettings.MountVolumesOntoDesktop();
	fMountSharedVolumesOntoDesktop = gTrackerSettings.MountSharedVolumesOntoDesktop();
	fIntegrateNonBootBeOSDesktops = gTrackerSettings.IntegrateNonBootBeOSDesktops();
	fEjectWhenUnmounting = gTrackerSettings.EjectWhenUnmounting();
}

bool
DesktopSettingsView::ShowsRevertSettings() const
{
	return
		(fShowDisksIcon ==
			(fShowDisksIconRadioButton->Value() > 0))
		&& (fMountVolumesOntoDesktop ==
			(fMountVolumesOntoDesktopRadioButton->Value() > 0))
		&& (fMountSharedVolumesOntoDesktop ==
			(fMountSharedVolumesOntoDesktopCheckBox->Value() > 0))
		&& (fIntegrateNonBootBeOSDesktops == 
			(fIntegrateNonBootBeOSDesktopsCheckBox->Value() > 0))
		&& (fEjectWhenUnmounting ==
			(fEjectWhenUnmountingCheckBox->Value() > 0));
}

//------------------------------------------------------------------------
// #pragma mark -


WindowsSettingsView::WindowsSettingsView(BRect rect)
	:	SettingsView(rect, "WindowsSettingsView")
{
	BRect frame = BRect(kBorderSpacing, kBorderSpacing, rect.Width()
		- 2 * kBorderSpacing, kBorderSpacing + kItemHeight);
	
	fShowFullPathInTitleBarCheckBox = new BCheckBox(frame, "", LOCALE("Show Full Path In Title Bar"),
		new BMessage(kWindowsShowFullPathChanged));
	AddChild(fShowFullPathInTitleBarCheckBox);
	fShowFullPathInTitleBarCheckBox->ResizeToPreferred();
	
	const float itemSpacing = fShowFullPathInTitleBarCheckBox->Bounds().Height() + kItemExtraSpacing;
	
	frame.OffsetBy(0, itemSpacing);
	
	fSingleWindowBrowseCheckBox = new BCheckBox(frame, "", LOCALE("Single Window Browse"),
		new BMessage(kSingleWindowBrowseChanged));
	AddChild(fSingleWindowBrowseCheckBox);
	fSingleWindowBrowseCheckBox->ResizeToPreferred();
	
	frame.OffsetBy(20, itemSpacing);
	
	fShowNavigatorCheckBox = new BCheckBox(frame, "", LOCALE("Show Navigator"),
		new BMessage(kShowNavigatorChanged));
	AddChild(fShowNavigatorCheckBox);
	fShowNavigatorCheckBox->ResizeToPreferred();
	
	frame.OffsetBy(20, itemSpacing);
	
	fShowHomeButtonCheckBox = new BCheckBox(frame, "", LOCALE("Show Home Button"),
		new BMessage(kShowHomeButtonChanged));
	AddChild(fShowHomeButtonCheckBox);
	fShowHomeButtonCheckBox->ResizeToPreferred();
	
	frame.OffsetBy(0, itemSpacing);
	
	frame.right = rect.Width() - 5;
	float divider = StringWidth(LOCALE("Directory:")) + 5;
	fHomeButtonDirectoryTextControl = new BTextControl(frame, "", LOCALE("Directory:"), "/boot/home",
		new BMessage(kHomeButtonDirectoryChanged));
	
	fHomeButtonDirectoryTextControl->SetDivider(divider);
	AddChild(fHomeButtonDirectoryTextControl);
	//fHomeButtonDirectoryTextControl->Width() = rect.Width() - 2;
	
	frame.OffsetBy(-40, itemSpacing);
	
	fShowSelectionWhenInactiveCheckBox = new BCheckBox(frame, "", LOCALE("Show Selection When Inactive"),
		new BMessage(kShowSelectionWhenInactiveChanged));
	AddChild(fShowSelectionWhenInactiveCheckBox);
	fShowSelectionWhenInactiveCheckBox->ResizeToPreferred();
	
	frame.OffsetBy(0, itemSpacing);
	
	fSortFolderNamesFirstCheckBox = new BCheckBox(frame, "", LOCALE("Sort Folder Names First"),
		new BMessage(kSortFolderNamesFirstChanged));
	AddChild(fSortFolderNamesFirstCheckBox);
	fSortFolderNamesFirstCheckBox->ResizeToPreferred();
}

void
WindowsSettingsView::AttachedToWindow()
{
	fSingleWindowBrowseCheckBox->SetTarget(this);
	fShowNavigatorCheckBox->SetTarget(this);
	fShowHomeButtonCheckBox->SetTarget(this);
	fHomeButtonDirectoryTextControl->SetTarget(this);
	fShowFullPathInTitleBarCheckBox->SetTarget(this);
	fShowSelectionWhenInactiveCheckBox->SetTarget(this);
	fSortFolderNamesFirstCheckBox->SetTarget(this);
}


void
WindowsSettingsView::MessageReceived(BMessage *message)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	switch (message->what) {
		case kWindowsShowFullPathChanged:
			gTrackerSettings.SetShowFullPathInTitleBar(fShowFullPathInTitleBarCheckBox->Value() == 1);
			tracker->SendNotices(kWindowsShowFullPathChanged);
			Window()->PostMessage(kSettingsContentsModified);
			break;
		
		case kSingleWindowBrowseChanged:
			gTrackerSettings.SetSingleWindowBrowse(fSingleWindowBrowseCheckBox->Value() == 1);
			if (fSingleWindowBrowseCheckBox->Value() == 0) {
				fShowNavigatorCheckBox->SetEnabled(false);
				gTrackerSettings.SetShowNavigator(0);
			} else {
				fShowNavigatorCheckBox->SetEnabled(true);
				gTrackerSettings.SetShowNavigator(fShowNavigatorCheckBox->Value() != 0);
			}
			
			tracker->SendNotices(kShowNavigatorChanged);
			tracker->SendNotices(kSingleWindowBrowseChanged);
			Window()->PostMessage(kSettingsContentsModified);
			break;
		
		case kShowNavigatorChanged:
			if (fShowNavigatorCheckBox->Value() == 0) {
				fHomeButtonDirectoryTextControl->SetEnabled(false);
				fShowHomeButtonCheckBox->SetEnabled(false);
			} else {
				fShowHomeButtonCheckBox->SetEnabled(true);
				fHomeButtonDirectoryTextControl->SetEnabled(true);
			}

			gTrackerSettings.SetShowNavigator(fShowNavigatorCheckBox->Value() == 1);
			tracker->SendNotices(kShowNavigatorChanged);
			Window()->PostMessage(kSettingsContentsModified);
			break;
		
		case kShowHomeButtonChanged:
			fHomeButtonDirectoryTextControl->SetEnabled(fShowHomeButtonCheckBox->Value() == 1);

			gTrackerSettings.SetShowHomeButton(fShowHomeButtonCheckBox->Value() == 1);
			tracker->SendNotices(kShowNavigatorChanged);
			Window()->PostMessage(kSettingsContentsModified);
			break;
		
		case kHomeButtonDirectoryChanged:
			gTrackerSettings.SetHomeButtonDirectory((char*)fHomeButtonDirectoryTextControl->Text());
			Window()->PostMessage(kSettingsContentsModified);
			break;
		
		case kShowSelectionWhenInactiveChanged:
		{
			gTrackerSettings.SetShowSelectionWhenInactive(
					fShowSelectionWhenInactiveCheckBox->Value() == 1);
			
			// Make the notification message and send it to the tracker:
			BMessage notificationMessage;
			notificationMessage.AddBool("ShowSelectionWhenInactive",
					fShowSelectionWhenInactiveCheckBox->Value() == 1);
			tracker->SendNotices(kShowSelectionWhenInactiveChanged, &notificationMessage);
			
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		case kSortFolderNamesFirstChanged:
		{
			gTrackerSettings.SetSortFolderNamesFirst(fSortFolderNamesFirstCheckBox->Value() == 1);
			
			// Make the notification message and send it to the tracker:
			BMessage notificationMessage;
			notificationMessage.AddBool("SortFolderNamesFirst",
					fSortFolderNamesFirstCheckBox->Value() == 1);
			tracker->SendNotices(kSortFolderNamesFirstChanged, &notificationMessage);
			
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		default:
			_inherited::MessageReceived(message);
			break;
	}
}

void
WindowsSettingsView::SetDefaults()
{
	gTrackerSettings.SetShowFullPathInTitleBar(false);
	gTrackerSettings.SetSingleWindowBrowse(true);
	gTrackerSettings.SetShowNavigator(true);
	gTrackerSettings.SetShowHomeButton(false);
	gTrackerSettings.SetHomeButtonDirectory("/boot/home");
	gTrackerSettings.SetShowSelectionWhenInactive(true);
	gTrackerSettings.SetSortFolderNamesFirst(false);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
WindowsSettingsView::Revert()
{
	gTrackerSettings.SetShowFullPathInTitleBar(fShowFullPathInTitleBar);
	gTrackerSettings.SetSingleWindowBrowse(fSingleWindowBrowse);
	gTrackerSettings.SetShowNavigator(fShowNavigator);
	gTrackerSettings.SetShowHomeButton(fShowHomeButton);
	gTrackerSettings.SetHomeButtonDirectory((char*)fHomeButtonDirectory);
	gTrackerSettings.SetShowSelectionWhenInactive(fShowSelectionWhenInactive);
	gTrackerSettings.SetSortFolderNamesFirst(fSortFolderNamesFirst);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
WindowsSettingsView::ShowCurrentSettings(bool sendNotices)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	fShowFullPathInTitleBarCheckBox->SetValue(gTrackerSettings.ShowFullPathInTitleBar());
	fSingleWindowBrowseCheckBox->SetValue(gTrackerSettings.SingleWindowBrowse());
	fShowNavigatorCheckBox->SetEnabled(gTrackerSettings.SingleWindowBrowse());
	fShowNavigatorCheckBox->SetValue(gTrackerSettings.ShowNavigator());
	fShowHomeButtonCheckBox->SetEnabled(gTrackerSettings.ShowNavigator());
	fShowHomeButtonCheckBox->SetValue(gTrackerSettings.ShowHomeButton());
	fHomeButtonDirectoryTextControl->SetEnabled(fShowHomeButtonCheckBox->Value() != 0);
	fHomeButtonDirectoryTextControl->SetText(gTrackerSettings.HomeButtonDirectory());
	fShowSelectionWhenInactiveCheckBox->SetValue(gTrackerSettings.ShowSelectionWhenInactive());
	fSortFolderNamesFirstCheckBox->SetValue(gTrackerSettings.SortFolderNamesFirst());
	
	if (sendNotices) {
		tracker->SendNotices(kSingleWindowBrowseChanged);
		tracker->SendNotices(kShowNavigatorChanged);
		tracker->SendNotices(kWindowsShowFullPathChanged);
		tracker->SendNotices(kShowSelectionWhenInactiveChanged);
		tracker->SendNotices(kSortFolderNamesFirstChanged);
	}
}

void
WindowsSettingsView::RecordRevertSettings()
{
	fShowFullPathInTitleBar = gTrackerSettings.ShowFullPathInTitleBar();
	fSingleWindowBrowse = gTrackerSettings.SingleWindowBrowse();
	fShowNavigator = gTrackerSettings.ShowNavigator();
	fShowHomeButton = gTrackerSettings.ShowHomeButton();
	fHomeButtonDirectory = (char *)gTrackerSettings.HomeButtonDirectory();
	fShowSelectionWhenInactive = gTrackerSettings.ShowSelectionWhenInactive();
	fSortFolderNamesFirst = gTrackerSettings.SortFolderNamesFirst();
}

bool
WindowsSettingsView::ShowsRevertSettings() const
{
	return
		(fShowFullPathInTitleBar ==
			(fShowFullPathInTitleBarCheckBox->Value() > 0))
		&& (fSingleWindowBrowse ==
			(fSingleWindowBrowseCheckBox->Value() > 0))
		&& (fShowNavigator ==
			(fShowNavigatorCheckBox->Value() > 0))
		&& (fShowHomeButton ==
			(fShowHomeButtonCheckBox->Value() > 0))
		&& (fShowSelectionWhenInactive ==
			(fShowSelectionWhenInactiveCheckBox->Value() > 0))
		&& (fSortFolderNamesFirst ==
			(fSortFolderNamesFirstCheckBox->Value() > 0)
		&& (BString(fHomeButtonDirectory).Compare(fHomeButtonDirectoryTextControl->Text()) == 0));
}

//------------------------------------------------------------------------
// #pragma mark -

FilePanelSettingsView::FilePanelSettingsView(BRect rect)
	:	SettingsView(rect, "FilePanelSettingsView")
{
	BRect frame = BRect(kBorderSpacing, kBorderSpacing, rect.Width()
		- 2 * kBorderSpacing, kBorderSpacing + kItemHeight);
	
	fDesktopFilePanelRootCheckBox = new BCheckBox(frame, "", LOCALE("File Panel Root is Desktop"),
		new BMessage(kDesktopFilePanelRootChanged));
	AddChild(fDesktopFilePanelRootCheckBox);
	fDesktopFilePanelRootCheckBox->ResizeToPreferred();
	
	const float itemSpacing = fDesktopFilePanelRootCheckBox->Bounds().Height() + kItemExtraSpacing;
	
	frame.OffsetBy(0, itemSpacing);
	
	BRect recentBoxFrame(kBorderSpacing, frame.bottom, rect.Width() - kBorderSpacing, frame.top);
	
	BBox *recentBox = new BBox(recentBoxFrame, "recentBox");
	recentBox->SetLabel(LOCALE("Recent"B_UTF8_ELLIPSIS));
	
	AddChild(recentBox);
	
	frame = recentBoxFrame.OffsetToCopy(0,0);
	frame.OffsetTo(kBorderSpacing, 3 * kBorderSpacing);
	
	
	float maxwidth = StringWidth(LOCALE("Applications"));
	maxwidth = MAX(maxwidth, StringWidth(LOCALE("Documents")));
	maxwidth = MAX(maxwidth, StringWidth(LOCALE("Folders")));
	frame.right = recentBoxFrame.right - 10;
	float divider = maxwidth + 10;
	
	fRecentDocumentsTextControl = new BTextControl(frame, "", LOCALE("Documents"), "10",
		new BMessage(kFavoriteCountChanged));
	
	fRecentDocumentsTextControl->SetDivider(divider);
	
	frame.OffsetBy(0, itemSpacing);
	
	fRecentFoldersTextControl = new BTextControl(frame, "", LOCALE("Folders"), "10",
		new BMessage(kFavoriteCountChanged));
	
	fRecentFoldersTextControl->SetDivider(divider);
	
	recentBox->AddChild(fRecentDocumentsTextControl);
	recentBox->AddChild(fRecentFoldersTextControl);
	
	recentBox->ResizeTo(recentBox->Frame().Width(), fRecentFoldersTextControl->Frame().bottom + kBorderSpacing);
	
	be_app->LockLooper();
	be_app->StartWatching(this, kFavoriteCountChangedExternally);
	be_app->UnlockLooper();
}

FilePanelSettingsView::~FilePanelSettingsView()
{
	be_app->LockLooper();
	be_app->StopWatching(this, kFavoriteCountChangedExternally);
	be_app->UnlockLooper();
}

void
FilePanelSettingsView::AttachedToWindow()
{
	fDesktopFilePanelRootCheckBox->SetTarget(this);
	fRecentDocumentsTextControl->SetTarget(this);
	fRecentFoldersTextControl->SetTarget(this);
}

void
FilePanelSettingsView::MessageReceived(BMessage *message)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	switch (message->what) {
		case kDesktopFilePanelRootChanged:
			{
				gTrackerSettings.SetDesktopFilePanelRoot(fDesktopFilePanelRootCheckBox->Value() == 1);
				
				// Make the notification message and send it to the tracker:
				BMessage message;
				message.AddBool("DesktopFilePanelRoot", fDesktopFilePanelRootCheckBox->Value() == 1);
				tracker->SendNotices(kDesktopFilePanelRootChanged, &message);
				
				Window()->PostMessage(kSettingsContentsModified);
			}
			break;
		
		case kFavoriteCountChanged:
			{
				GetAndRefreshDisplayedFigures();
				gTrackerSettings.SetRecentDocumentsCount(fDisplayedDocCount);
				gTrackerSettings.SetRecentFoldersCount(fDisplayedFolderCount);
				
				// Make the notification message and send it to the tracker:
				BMessage message;
				message.AddInt32("RecentDocuments", fDisplayedDocCount);
				message.AddInt32("RecentFolders", fDisplayedFolderCount);
				tracker->SendNotices(kFavoriteCountChanged, &message);
				
				Window()->PostMessage(kSettingsContentsModified);
			}
			break;
		
		case B_OBSERVER_NOTICE_CHANGE:
			{
				int32 observerWhat;
				if (message->FindInt32("be:observe_change_what", &observerWhat) == B_OK) {
					switch (observerWhat) {
						case kFavoriteCountChangedExternally:
							{
								int32 count;
								if (message->FindInt32("RecentApplications", &count) == B_OK) {
									gTrackerSettings.SetRecentApplicationsCount(count);
									ShowCurrentSettings();
								}
								
								if (message->FindInt32("RecentDocuments", &count) == B_OK) {
									gTrackerSettings.SetRecentDocumentsCount(count);
									ShowCurrentSettings();
								}
								
								if (message->FindInt32("RecentFolders", &count) == B_OK) {
									gTrackerSettings.SetRecentFoldersCount(count);
									ShowCurrentSettings();
								}
							}
							break;
					}
				}
			}
			break;
		
		default:
			_inherited::MessageReceived(message);
			break;
	}
}

void
FilePanelSettingsView::SetDefaults()
{
	gTrackerSettings.SetDesktopFilePanelRoot(true);
	gTrackerSettings.SetRecentDocumentsCount(10);
	gTrackerSettings.SetRecentFoldersCount(10);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
FilePanelSettingsView::Revert()
{
	gTrackerSettings.SetDesktopFilePanelRoot(fDesktopFilePanelRoot);
	gTrackerSettings.SetRecentDocumentsCount(fRecentDocuments);
	gTrackerSettings.SetRecentFoldersCount(fRecentFolders);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
FilePanelSettingsView::ShowCurrentSettings(bool sendNotices)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	fDesktopFilePanelRootCheckBox->SetValue(gTrackerSettings.DesktopFilePanelRoot());
	
	int32 recentApplications, recentDocuments, recentFolders;
	gTrackerSettings.RecentCounts(&recentApplications, &recentDocuments, &recentFolders);
	
	BString docCountText;
	docCountText << recentDocuments;
	fRecentDocumentsTextControl->SetText(docCountText.String());
	
	BString folderCountText;
	folderCountText << recentFolders;
	fRecentFoldersTextControl->SetText(folderCountText.String());
	
	if (sendNotices) {
		// Make the notification message and send it to the tracker:
		
		BMessage message;
		message.AddBool("DesktopFilePanelRoot", fDesktopFilePanelRootCheckBox->Value() == 1);
		tracker->SendNotices(kDesktopFilePanelRootChanged, &message);
		
		message.AddInt32("RecentDocuments", recentDocuments);
		message.AddInt32("RecentFolders", recentFolders);
		tracker->SendNotices(kFavoriteCountChanged, &message);
	}
}

void
FilePanelSettingsView::RecordRevertSettings()
{
	fDesktopFilePanelRoot = gTrackerSettings.DesktopFilePanelRoot();
	gTrackerSettings.RecentCounts(&fRecentApplications, &fRecentDocuments, &fRecentFolders);
}

bool
FilePanelSettingsView::ShowsRevertSettings() const
{
	GetAndRefreshDisplayedFigures();
	
	return
		(fDesktopFilePanelRoot == (fDesktopFilePanelRootCheckBox->Value() > 0))
		&& (fDisplayedDocCount == fRecentDocuments)
		&& (fDisplayedFolderCount == fRecentFolders);
}

void
FilePanelSettingsView::GetAndRefreshDisplayedFigures() const
{
	sscanf(fRecentDocumentsTextControl->Text(), "%ld", &fDisplayedDocCount);
	sscanf(fRecentFoldersTextControl->Text(), "%ld", &fDisplayedFolderCount);
	
	BString docCountText;
	docCountText << fDisplayedDocCount;
	fRecentDocumentsTextControl->SetText(docCountText.String());
	
	BString folderCountText;
	folderCountText << fDisplayedFolderCount;
	fRecentFoldersTextControl->SetText(folderCountText.String());
}

//------------------------------------------------------------------------
// #pragma mark -

TimeFormatSettingsView::TimeFormatSettingsView(BRect rect)
	:	SettingsView(rect, "WindowsSettingsView")
{
	BRect clockBoxFrame = BRect(kBorderSpacing, kBorderSpacing,
		rect.Width() / 2 - 4 * kBorderSpacing, kBorderSpacing + 5 * kItemHeight);
	
	BBox *clockBox = new BBox(clockBoxFrame, "Clock");
	clockBox->SetLabel(LOCALE("Clock"));
	
	AddChild(clockBox);
	
	BRect frame = BRect(kBorderSpacing, 2.5f*kBorderSpacing,
		clockBoxFrame.Width() - 2 * kBorderSpacing, kBorderSpacing + kItemHeight);
	
	f24HrRadioButton = new BRadioButton(frame, "", LOCALE("24 Hour"),
		new BMessage(kSettingsContentsModified));
	clockBox->AddChild(f24HrRadioButton);
	f24HrRadioButton->ResizeToPreferred();
	
	const float itemSpacing = f24HrRadioButton->Bounds().Height() + kItemExtraSpacing;
	
	frame.OffsetBy(0, itemSpacing);
	
	f12HrRadioButton = new BRadioButton(frame, "", LOCALE("12 Hour"),
		new BMessage(kSettingsContentsModified));
	clockBox->AddChild(f12HrRadioButton);
	f12HrRadioButton->ResizeToPreferred();
	
	clockBox->ResizeTo(clockBox->Bounds().Width(), f12HrRadioButton->Frame().bottom + kBorderSpacing);
	
	BRect dateFormatBoxFrame = BRect(clockBoxFrame.right + kBorderSpacing, kBorderSpacing,
		rect.right - kBorderSpacing, kBorderSpacing + 5 * itemSpacing);
	
	BBox *dateFormatBox = new BBox(dateFormatBoxFrame, "Date Order");
	dateFormatBox->SetLabel(LOCALE("Date Order"));
	
	AddChild(dateFormatBox);
	
	frame = BRect(kBorderSpacing, 2.5f*kBorderSpacing,
		dateFormatBoxFrame.Width() - 2 * kBorderSpacing, kBorderSpacing + kItemHeight);
	
	fYMDRadioButton = new BRadioButton(frame, "", LOCALE("Year-Month-Day"),
		new BMessage(kSettingsContentsModified));
	dateFormatBox->AddChild(fYMDRadioButton);
	fYMDRadioButton->ResizeToPreferred();
	
	frame.OffsetBy(0, itemSpacing);
	
	fDMYRadioButton = new BRadioButton(frame, "", LOCALE("Day-Month-Year"),
		new BMessage(kSettingsContentsModified));
	dateFormatBox->AddChild(fDMYRadioButton);
	fDMYRadioButton->ResizeToPreferred();
	
	frame.OffsetBy(0, itemSpacing);
	
	fMDYRadioButton = new BRadioButton(frame, "", LOCALE("Month-Day-Year"),
		new BMessage(kSettingsContentsModified));
	dateFormatBox->AddChild(fMDYRadioButton);
	fMDYRadioButton->ResizeToPreferred();
	
	dateFormatBox->ResizeTo(dateFormatBox->Bounds().Width(), fMDYRadioButton->Frame().bottom + kBorderSpacing);
	
	BPopUpMenu *menu = new BPopUpMenu("Separator");
	
	menu->AddItem(new BMenuItem(LOCALE("None"), new BMessage(kSettingsContentsModified)));
	menu->AddItem(new BMenuItem(LOCALE("Space"), new BMessage(kSettingsContentsModified)));
	menu->AddItem(new BMenuItem(LOCALE("-"), new BMessage(kSettingsContentsModified)));
	menu->AddItem(new BMenuItem(LOCALE("/"), new BMessage(kSettingsContentsModified)));
	menu->AddItem(new BMenuItem(LOCALE("\\"), new BMessage(kSettingsContentsModified)));
	menu->AddItem(new BMenuItem(LOCALE("."), new BMessage(kSettingsContentsModified)));
	
	frame = BRect(clockBox->Frame().left, dateFormatBox->Frame().bottom + kBorderSpacing,
		rect.right - kBorderSpacing, dateFormatBox->Frame().bottom + kBorderSpacing + itemSpacing);
	
	fSeparatorMenuField = new BMenuField(frame, "Separator", LOCALE("Separator"), menu);
	fSeparatorMenuField->ResizeToPreferred();
	AddChild(fSeparatorMenuField);
	
	frame.OffsetBy(0, 30.0f);
	
	BStringView *exampleView = new BStringView(frame, "", LOCALE("Examples:"));
	AddChild(exampleView);
	exampleView->ResizeToPreferred();
	
	frame.OffsetBy(0, itemSpacing);
	
	fLongDateExampleView = new BStringView(frame, "", "");
	AddChild(fLongDateExampleView);
	fLongDateExampleView->ResizeToPreferred();
	
	frame.OffsetBy(0, itemSpacing);
	
	fShortDateExampleView = new BStringView(frame, "", "");
	AddChild(fShortDateExampleView);
	fShortDateExampleView->ResizeToPreferred();
	
	UpdateExamples();
}

void
TimeFormatSettingsView::AttachedToWindow()
{
	f24HrRadioButton->SetTarget(this);
	f12HrRadioButton->SetTarget(this);
	fYMDRadioButton->SetTarget(this);
	fDMYRadioButton->SetTarget(this);
	fMDYRadioButton->SetTarget(this);
	
	fSeparatorMenuField->Menu()->SetTargetForItems(this);
}

void
TimeFormatSettingsView::MessageReceived(BMessage *message)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	switch (message->what) {
		case kSettingsContentsModified:
			{
				int32 separator = 0;
				BMenuItem *item = fSeparatorMenuField->Menu()->FindMarked();
				if (item) {
					separator = fSeparatorMenuField->Menu()->IndexOf(item);
					if (separator >= 0)
						gTrackerSettings.SetTimeFormatSeparator((FormatSeparator)separator);
				}
				
				DateOrder format =
					fYMDRadioButton->Value() ? kYMDFormat :
					fDMYRadioButton->Value() ? kDMYFormat : kMDYFormat;
				
				gTrackerSettings.SetDateOrderFormat(format);
				gTrackerSettings.SetClockTo24Hr(f24HrRadioButton->Value() == 1);
				
				// Make the notification message and send it to the tracker:
				BMessage notificationMessage;
				notificationMessage.AddInt32("TimeFormatSeparator", separator);
				notificationMessage.AddInt32("DateOrderFormat", format);
				notificationMessage.AddBool("24HrClock", f24HrRadioButton->Value() == 1);
				tracker->SendNotices(kDateFormatChanged, &notificationMessage);
				
				UpdateExamples();
				
				Window()->PostMessage(kSettingsContentsModified);
				break;
			}
		
		default:
			_inherited::MessageReceived(message);
	}
}

void
TimeFormatSettingsView::SetDefaults()
{
	gTrackerSettings.SetTimeFormatSeparator(kSlashSeparator);
	gTrackerSettings.SetDateOrderFormat(kMDYFormat);
	gTrackerSettings.SetClockTo24Hr(false);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
TimeFormatSettingsView::Revert()
{
	gTrackerSettings.SetTimeFormatSeparator(fSeparator);
	gTrackerSettings.SetDateOrderFormat(fFormat);
	gTrackerSettings.SetClockTo24Hr(f24HrClock);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
TimeFormatSettingsView::ShowCurrentSettings(bool sendNotices)
{
	f24HrRadioButton->SetValue(gTrackerSettings.ClockIs24Hr());
	f12HrRadioButton->SetValue(!gTrackerSettings.ClockIs24Hr());
	
	switch (gTrackerSettings.DateOrderFormat()) {
		case kYMDFormat:
			fYMDRadioButton->SetValue(1);
			break;
		
		case kMDYFormat:
			fMDYRadioButton->SetValue(1);
			break;
		
		default:
		case kDMYFormat:
			fDMYRadioButton->SetValue(1);
			break;
	}
	
	FormatSeparator separator = gTrackerSettings.TimeFormatSeparator();
	
	if (separator >= kNoSeparator && separator < kSeparatorsEnd)
		fSeparatorMenuField->Menu()->ItemAt((int32)separator)->SetMarked(true);
	
	UpdateExamples();
	
	if (sendNotices) {
		TTracker *tracker = dynamic_cast<TTracker *>(be_app);
		if (!tracker)
			return;
		
		// Make the notification message and send it to the tracker:
		BMessage notificationMessage;
		notificationMessage.AddInt32("TimeFormatSeparator", (int32)gTrackerSettings.TimeFormatSeparator());
		notificationMessage.AddInt32("DateOrderFormat", (int32)gTrackerSettings.DateOrderFormat());
		notificationMessage.AddBool("24HrClock", gTrackerSettings.ClockIs24Hr());
		tracker->SendNotices(kDateFormatChanged, &notificationMessage);
	}
}

void
TimeFormatSettingsView::RecordRevertSettings()
{
	f24HrClock = gTrackerSettings.ClockIs24Hr();
	fSeparator = gTrackerSettings.TimeFormatSeparator();
	fFormat = gTrackerSettings.DateOrderFormat();
}

bool
TimeFormatSettingsView::ShowsRevertSettings() const
{
	FormatSeparator separator;
	
	BMenuItem *item = fSeparatorMenuField->Menu()->FindMarked();
	if (item) {
		int32 index = fSeparatorMenuField->Menu()->IndexOf(item);
		if (index >= 0)
			separator = (FormatSeparator)index;
		else
			return false;
	} else
		return false;
	
	DateOrder format =
		fYMDRadioButton->Value() ? kYMDFormat :
		(fDMYRadioButton->Value() ? kDMYFormat : kMDYFormat);
	
	return
		f24HrClock == (f24HrRadioButton->Value() > 0)
		&& separator == fSeparator
		&& format == fFormat;
}

void
TimeFormatSettingsView::UpdateExamples()
{
	time_t timeValue = (time_t)time(NULL);
	tm timeData;
	localtime_r(&timeValue, &timeData);
	BString timeFormat = "Internal Error!";
	char buffer[256];
	
	FormatSeparator separator;
	
	BMenuItem *item = fSeparatorMenuField->Menu()->FindMarked();
	if (item) {
		int32 index = fSeparatorMenuField->Menu()->IndexOf(item);
		if (index >= 0)
			separator = (FormatSeparator)index;
		else
			separator = kSlashSeparator;
	} else
		separator = kSlashSeparator;
	
	DateOrder order =
		fYMDRadioButton->Value() ? kYMDFormat :
		(fDMYRadioButton->Value() ? kDMYFormat : kMDYFormat);
	
	bool clockIs24hr = (f24HrRadioButton->Value() > 0);
	
	TimeFormat(timeFormat, 0, separator, order, clockIs24hr);
	strftime(buffer, 256, timeFormat.String(), &timeData);
	
	fLongDateExampleView->SetText(buffer);
	fLongDateExampleView->ResizeToPreferred();
	
	TimeFormat(timeFormat, 4, separator, order, clockIs24hr);
	strftime(buffer, 256, timeFormat.String(), &timeData);
	
	fShortDateExampleView->SetText(buffer);
	fShortDateExampleView->ResizeToPreferred();
}

//------------------------------------------------------------------------
// #pragma mark -

SpaceBarSettingsView::SpaceBarSettingsView(BRect rect)
	:	SettingsView(rect, "SpaceBarSettingsView")
{
	BRect frame = BRect(kBorderSpacing, kBorderSpacing, rect.Width()
		- 2 * kBorderSpacing, kBorderSpacing + kItemHeight);
	
	fSpaceBarShowCheckBox = new BCheckBox(frame, "", LOCALE("Show Space Bars On Volumes"),
		new BMessage(kUpdateVolumeSpaceBar));
	AddChild(fSpaceBarShowCheckBox);
	fSpaceBarShowCheckBox->ResizeToPreferred();
	float itemSpacing = fSpaceBarShowCheckBox->Bounds().Height() + kItemExtraSpacing;
	frame.OffsetBy(0, itemSpacing);
	
	BPopUpMenu *menu = new BPopUpMenu(B_EMPTY_STRING);
	menu->SetFont(be_plain_font);
	
	BMenuItem *item;
	menu->AddItem(item = new BMenuItem(LOCALE("Used Space Color"), new BMessage(kSpaceBarSwitchColor)));
	item->SetMarked(true);
	fCurrentColor = 0;
	menu->AddItem(new BMenuItem(LOCALE("Free Space Color"), new BMessage(kSpaceBarSwitchColor)));
	menu->AddItem(new BMenuItem(LOCALE("Warning Space Color"), new BMessage(kSpaceBarSwitchColor)));
	
	BBox *box = new BBox(frame);
	box->SetLabel(fColorPicker = new BMenuField(frame, NULL, NULL, menu));
	AddChild(box);
	
	fColorControl = new BColorControl(
			BPoint(8, fColorPicker->Bounds().Height() + 8 + kItemExtraSpacing),
			B_CELLS_16x16,1,"SpaceColorControl",new BMessage(kSpaceBarColorChanged));
	fColorControl->SetValue(gTrackerSettings.UsedSpaceColor());
	fColorControl->ResizeToPreferred();
	box->AddChild(fColorControl);
	box->ResizeTo(fColorControl->Bounds().Width() + 16, fColorControl->Frame().bottom + 8);
}

SpaceBarSettingsView::~SpaceBarSettingsView()
{
}

void
SpaceBarSettingsView::AttachedToWindow()
{
	fSpaceBarShowCheckBox->SetTarget(this);
	fColorControl->SetTarget(this);
	fColorPicker->Menu()->SetTargetForItems(this);
}

void
SpaceBarSettingsView::MessageReceived(BMessage *message)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	switch (message->what) {
		case kUpdateVolumeSpaceBar:
		{
			gTrackerSettings.SetShowVolumeSpaceBar(fSpaceBarShowCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			tracker->PostMessage(kShowVolumeSpaceBar);
			break;
		}
		case kSpaceBarSwitchColor:
		{
			fCurrentColor = message->FindInt32("index");
			switch (fCurrentColor) {
				case 0:
					fColorControl->SetValue(gTrackerSettings.UsedSpaceColor());
					break;
				case 1:
					fColorControl->SetValue(gTrackerSettings.FreeSpaceColor());
					break;
				case 2:
					fColorControl->SetValue(gTrackerSettings.WarningSpaceColor());
					break;
			}
			break;
		}

		case kSpaceBarColorChanged:
		{
			switch (fCurrentColor) {
				case 0:
					gTrackerSettings.SetUsedSpaceColor(fColorControl->ValueAsColor());
					break;
				case 1:
					gTrackerSettings.SetFreeSpaceColor(fColorControl->ValueAsColor());
					break;
				case 2:
					gTrackerSettings.SetWarningSpaceColor(fColorControl->ValueAsColor());
					break;
			}

			Window()->PostMessage(kSettingsContentsModified);
			tracker->PostMessage(kSpaceBarColorChanged);
			break;
		}
		
		default:
			_inherited::MessageReceived(message);
			break;
	}
}

void
SpaceBarSettingsView::SetDefaults()
{
	gTrackerSettings.SetShowVolumeSpaceBar(false);
	gTrackerSettings.SetUsedSpaceColor(Color(0,0xcb,0,192));
	gTrackerSettings.SetFreeSpaceColor(Color(0xff,0xff,0xff,192));
	gTrackerSettings.SetWarningSpaceColor(Color(0xcb,0,0,192));
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
SpaceBarSettingsView::Revert()
{
	gTrackerSettings.SetShowVolumeSpaceBar(fSpaceBarShow);
	gTrackerSettings.SetUsedSpaceColor(fUsedSpaceColor);
	gTrackerSettings.SetFreeSpaceColor(fFreeSpaceColor);
	gTrackerSettings.SetWarningSpaceColor(fWarningSpaceColor);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
SpaceBarSettingsView::ShowCurrentSettings(bool sendNotices)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	fSpaceBarShowCheckBox->SetValue(gTrackerSettings.ShowVolumeSpaceBar());
	
	switch (fCurrentColor) {
		case 0:
			fColorControl->SetValue(gTrackerSettings.UsedSpaceColor());
			break;
		case 1:
			fColorControl->SetValue(gTrackerSettings.FreeSpaceColor());
			break;
		case 2:
			fColorControl->SetValue(gTrackerSettings.WarningSpaceColor());
			break;
	}
	
	if (sendNotices) {
		BMessage notificationMessage;
		notificationMessage.AddBool("ShowVolumeSpaceBar", gTrackerSettings.ShowVolumeSpaceBar());
		tracker->SendNotices(kShowVolumeSpaceBar, &notificationMessage);
		
		Window()->PostMessage(kSettingsContentsModified);
		BMessage notificationMessage2;
		tracker->SendNotices(kSpaceBarColorChanged, &notificationMessage2);
	}
}

void
SpaceBarSettingsView::RecordRevertSettings()
{
	fSpaceBarShow = gTrackerSettings.ShowVolumeSpaceBar();
	fUsedSpaceColor = gTrackerSettings.UsedSpaceColor();
	fFreeSpaceColor = gTrackerSettings.FreeSpaceColor();
	fWarningSpaceColor = gTrackerSettings.WarningSpaceColor();
}

bool
SpaceBarSettingsView::ShowsRevertSettings() const
{
	return ((fSpaceBarShow == (fSpaceBarShowCheckBox->Value() == 1)) &&
			(fUsedSpaceColor == gTrackerSettings.UsedSpaceColor()) &&
			(fFreeSpaceColor == gTrackerSettings.FreeSpaceColor()) &&
			(fWarningSpaceColor == gTrackerSettings.WarningSpaceColor()));
}

//------------------------------------------------------------------------
// #pragma mark -

TrashSettingsView::TrashSettingsView(BRect rect)
	:	SettingsView(rect, "TrashSettingsView")
{
	BRect frame = BRect(kBorderSpacing, kBorderSpacing, rect.Width()
			- 2 * kBorderSpacing, kBorderSpacing + kItemHeight);
	
	fDontMoveFilesToTrashCheckBox = new BCheckBox(frame, "", LOCALE("Don't Move Files To Trash"),
			new BMessage(kDontMoveFilesToTrashChanged));
	AddChild(fDontMoveFilesToTrashCheckBox);
	fDontMoveFilesToTrashCheckBox->ResizeToPreferred();
	
	frame.OffsetBy(0, fDontMoveFilesToTrashCheckBox->Bounds().Height() + kItemExtraSpacing);
	
	fAskBeforeDeleteFileCheckBox = new BCheckBox(frame, "", LOCALE("Ask Before Delete"),
			new BMessage(kAskBeforeDeleteFileChanged));
	AddChild(fAskBeforeDeleteFileCheckBox);
	fAskBeforeDeleteFileCheckBox->ResizeToPreferred();
}

void
TrashSettingsView::AttachedToWindow()
{
	fDontMoveFilesToTrashCheckBox->SetTarget(this);
	fAskBeforeDeleteFileCheckBox->SetTarget(this);
}

void
TrashSettingsView::MessageReceived(BMessage *message)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	switch (message->what) {
		case kDontMoveFilesToTrashChanged:
			gTrackerSettings.SetDontMoveFilesToTrash(fDontMoveFilesToTrashCheckBox->Value() == 1);
			
			tracker->SendNotices(kDontMoveFilesToTrashChanged);
			Window()->PostMessage(kSettingsContentsModified);
			break;
		
		case kAskBeforeDeleteFileChanged:
			gTrackerSettings.SetAskBeforeDeleteFile(fAskBeforeDeleteFileCheckBox->Value() == 1);
			
			tracker->SendNotices(kAskBeforeDeleteFileChanged);
			Window()->PostMessage(kSettingsContentsModified);
			break;
		
		default:
			_inherited::MessageReceived(message);
			break;
	}
}
	
void
TrashSettingsView::SetDefaults()
{
	gTrackerSettings.SetDontMoveFilesToTrash(false);
	gTrackerSettings.SetAskBeforeDeleteFile(true);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
TrashSettingsView::Revert()
{
	gTrackerSettings.SetDontMoveFilesToTrash(fDontMoveFilesToTrash);
	gTrackerSettings.SetAskBeforeDeleteFile(fAskBeforeDeleteFile);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
TrashSettingsView::ShowCurrentSettings(bool sendNotices)
{
	fDontMoveFilesToTrashCheckBox->SetValue(gTrackerSettings.DontMoveFilesToTrash());
	fAskBeforeDeleteFileCheckBox->SetValue(gTrackerSettings.AskBeforeDeleteFile());
	
	if (sendNotices)
		Window()->PostMessage(kSettingsContentsModified);
}

void
TrashSettingsView::RecordRevertSettings()
{
	fDontMoveFilesToTrash = gTrackerSettings.DontMoveFilesToTrash();
	fAskBeforeDeleteFile = gTrackerSettings.AskBeforeDeleteFile();
}

bool
TrashSettingsView::ShowsRevertSettings() const
{
	return (fDontMoveFilesToTrash == (fDontMoveFilesToTrashCheckBox->Value() > 0))
			&& (fAskBeforeDeleteFile == (fAskBeforeDeleteFileCheckBox->Value() > 0));
}

//------------------------------------------------------------------------
// #pragma mark -

TransparentSelectionSettingsView::TransparentSelectionSettingsView(BRect rect)
	:	SettingsView(rect, "TransparentSelectionSettingsView")
{
	BRect frame = BRect(kBorderSpacing, kBorderSpacing, rect.Width()
		- 2 * kBorderSpacing, kBorderSpacing + kItemHeight);
	
	fTransparentSelectionCheckBox = new BCheckBox(frame, "", LOCALE("Transparent Selection Box"),
		new BMessage(kTransparentSelectionChanged));
	fTransparentSelectionCheckBox->ResizeToPreferred();
	
	BBox *box = new BBox(frame);
	box->SetFont(be_plain_font);
	box->SetLabel(fTransparentSelectionCheckBox);
	AddChild(box);
	
	fColorControl = new BColorControl(
			BPoint(8, fTransparentSelectionCheckBox->Bounds().Height() + kItemExtraSpacing),
			B_CELLS_16x16,1,"TransparentSelectionColorControl",new BMessage(kTransparentSelectionColorChanged));
	fColorControl->SetValue(gTrackerSettings.TransparentSelectionColor());
	fColorControl->ResizeToPreferred();
	box->AddChild(fColorControl);
	
	float itemSpacing = fColorControl->Bounds().Height() + fTransparentSelectionCheckBox->Bounds().Height() + kItemExtraSpacing * 2;
	frame.OffsetBy(0, itemSpacing);
	
	frame.right = StringWidth("##Applications###255##");
	float divider = StringWidth("Applications") + 10;
	
	fTransparentSelectionAlphaTextControl = new BTextControl(frame, "", "Alpha", "90",
		new BMessage(kTransparentSelectionColorChanged));
	
	fTransparentSelectionAlphaTextControl->SetDivider(divider);
	box->AddChild(fTransparentSelectionAlphaTextControl);
	
	box->ResizeTo(fColorControl->Bounds().Width() + 16,fColorControl->Frame().bottom + fTransparentSelectionAlphaTextControl->Bounds().Height() + 16);
}

TransparentSelectionSettingsView::~TransparentSelectionSettingsView()
{
}

void
TransparentSelectionSettingsView::AttachedToWindow()
{
	fTransparentSelectionCheckBox->SetTarget(this);
	fColorControl->SetTarget(this);
	fTransparentSelectionAlphaTextControl->SetTarget(this);
}

void
TransparentSelectionSettingsView::MessageReceived(BMessage *message)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	switch (message->what) {
		case kTransparentSelectionChanged:
		{
			gTrackerSettings.SetTransparentSelection(
					fTransparentSelectionCheckBox->Value() == 1);
			
			// Make the notification message and send it to the tracker:
			BMessage notificationMessage;
			notificationMessage.AddBool("TransparentSelection",
					fTransparentSelectionCheckBox->Value() == 1);
			tracker->SendNotices(kTransparentSelectionChanged, &notificationMessage);
			
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		case kTransparentSelectionColorChanged:
		{
			rgb_color currentcolor;
			currentcolor = fColorControl->ValueAsColor();
			int32 alphaValue;
			sscanf(fTransparentSelectionAlphaTextControl->Text(), "%ld", &alphaValue);
			
			if (alphaValue < 1 || alphaValue > 255)
				alphaValue = 90;
			
			currentcolor.alpha = alphaValue;
			
			BString alphaString;
			alphaString << alphaValue;
			fTransparentSelectionAlphaTextControl->SetText(alphaString.String());
			
			gTrackerSettings.SetTransparentSelectionColor(currentcolor);
			
			Window()->PostMessage(kSettingsContentsModified);
			tracker->SendNotices(kTransparentSelectionColorChanged, NULL);
			break;
		}
		
		default:
			_inherited::MessageReceived(message);
			break;
	}	
}

void
TransparentSelectionSettingsView::SetDefaults()
{
	gTrackerSettings.SetTransparentSelection(false);
	gTrackerSettings.SetTransparentSelectionColor(Color(80, 80, 80, 90));
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
TransparentSelectionSettingsView::Revert()
{
	gTrackerSettings.SetTransparentSelection(fTransparentSelection);
	gTrackerSettings.SetTransparentSelectionColor(fTransparentSelectionColor);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
TransparentSelectionSettingsView::ShowCurrentSettings(bool sendNotices)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	fTransparentSelectionCheckBox->SetValue(gTrackerSettings.TransparentSelection());
	fColorControl->SetValue(gTrackerSettings.TransparentSelectionColor());
	
	int32 alphaValue;
	alphaValue = gTrackerSettings.TransparentSelectionColor().alpha;
	BString alphaString;
	alphaString << alphaValue;
	fTransparentSelectionAlphaTextControl->SetText(alphaString.String());
	
	if (sendNotices) {
		BMessage notificationMessage;
		notificationMessage.AddBool("TransparentSelection", gTrackerSettings.TransparentSelection());
		
		tracker->SendNotices(kTransparentSelectionChanged, &notificationMessage);
		tracker->SendNotices(kTransparentSelectionColorChanged, NULL);
		
		Window()->PostMessage(kSettingsContentsModified);
	}
}

void
TransparentSelectionSettingsView::RecordRevertSettings()
{
	fTransparentSelection = gTrackerSettings.TransparentSelection();
	fTransparentSelectionColor = gTrackerSettings.TransparentSelectionColor();
}

bool
TransparentSelectionSettingsView::ShowsRevertSettings() const
{
	return ((fTransparentSelection == (fTransparentSelectionCheckBox->Value() > 0)) &&
			(fTransparentSelectionColor == gTrackerSettings.TransparentSelectionColor()));
}

//------------------------------------------------------------------------
// #pragma mark -

FilteringSettingsView::FilteringSettingsView(BRect rect)
	:	SettingsView(rect, "FilteringSettingsView")
{
	BRect frame = BRect(kBorderSpacing, kBorderSpacing, rect.Width()
		- 2 * kBorderSpacing, kBorderSpacing + kItemHeight);
	
	fDynamicFilteringCheckBox = new BCheckBox(frame, "", LOCALE("Enable Dynamic Filtering"),
		new BMessage(kDynamicFilteringChanged));
	fDynamicFilteringCheckBox->ResizeToPreferred();
	
	BBox *box = new BBox(frame);
	box->SetFont(be_plain_font);
	box->SetLabel(fDynamicFilteringCheckBox);
	AddChild(box);
	frame.OffsetBy(0, kItemHeight);
	
	BMenu *menu = new BPopUpMenu("");
	menu->AddItem(new BMenuItem(LOCALE("Name starts with"), new BMessage(kDynamicFilteringExpressionTypeChanged)));
	menu->AddItem(new BMenuItem(LOCALE("Name ends with"), new BMessage(kDynamicFilteringExpressionTypeChanged)));
	menu->AddItem(new BMenuItem(LOCALE("Name contains"), new BMessage(kDynamicFilteringExpressionTypeChanged)));
	menu->AddItem(new BMenuItem(LOCALE("Name matches wildcard expression"), new BMessage(kDynamicFilteringExpressionTypeChanged)));
	//menu->AddItem(new BMenuItem("Name matches regular expression", new BMessage(kDynamicFilteringExpressionTypeChanged)));
	menu->SetLabelFromMarked(true);
	
	fDynamicFilteringExpressionTypeMenuField = new BMenuField(frame, "", "", menu);
	box->AddChild(fDynamicFilteringExpressionTypeMenuField);
	fDynamicFilteringExpressionTypeMenuField->SetDivider(fDynamicFilteringExpressionTypeMenuField->StringWidth(LOCALE("Name")) + 8);
	fDynamicFilteringExpressionTypeMenuField->SetDivider(0);
	fDynamicFilteringExpressionTypeMenuField->Menu()->ItemAt(3)->SetMarked(true);
	fDynamicFilteringExpressionTypeMenuField->ResizeToPreferred();
	float itemSpacing = fDynamicFilteringExpressionTypeMenuField->Bounds().Height() + 8 + kItemExtraSpacing;
	frame.OffsetBy(0, itemSpacing);
	
	fDynamicFilteringInvertCheckBox = new BCheckBox(frame, "", LOCALE("Invert Filter"),
		new BMessage(kDynamicFilteringInvertChanged));
	box->AddChild(fDynamicFilteringInvertCheckBox);
	fDynamicFilteringInvertCheckBox->ResizeToPreferred();
	itemSpacing = fDynamicFilteringInvertCheckBox->Bounds().Width() + kItemHeight;
	frame.OffsetBy(itemSpacing, 0);
	
	fDynamicFilteringIgnoreCaseCheckBox = new BCheckBox(frame, "", LOCALE("Ignore case"),
		new BMessage(kDynamicFilteringIgnoreCaseChanged));
	box->AddChild(fDynamicFilteringIgnoreCaseCheckBox);
	fDynamicFilteringIgnoreCaseCheckBox->ResizeToPreferred();
	frame.OffsetBy(-itemSpacing, 0);
	itemSpacing = fDynamicFilteringIgnoreCaseCheckBox->Bounds().Height() + kItemExtraSpacing;
	frame.OffsetBy(0, itemSpacing);
	
	box->ResizeTo(Bounds().Width() - 8, fDynamicFilteringIgnoreCaseCheckBox->Frame().bottom + kItemExtraSpacing);
	frame.OffsetBy(0, 10);
	
	fStaticFilteringCheckBox = new BCheckBox(frame, "", LOCALE("Enable Static Filtering"),
		new BMessage(kStaticFilteringChanged));
	fStaticFilteringCheckBox->ResizeToPreferred();
	
	box = new BBox(frame);
	box->ResizeTo(Bounds().Width() - 8, Bounds().Height() - frame.top - 4);
	box->SetFont(be_plain_font);
	box->SetLabel(fStaticFilteringCheckBox);
	AddChild(box);
	frame = box->Bounds();
	frame.OffsetBy(8, kItemHeight + kItemExtraSpacing);
	
	BButton testButton(frame, "", "", NULL);
	float buttonWidth = 20, buttonHeight = kItemHeight;
	testButton.GetPreferredSize(&buttonWidth, &buttonHeight);
	fStaticFilteringListView = new BListView(frame, "StaticFilters");
	fStaticFilteringListView->ResizeTo(box->Bounds().Width() - 32, frame.Height() - frame.top - buttonHeight - kItemHeight + 4);
	fStaticFilteringScrollView = new BScrollView("StaticFilteringScrollView", fStaticFilteringListView, B_FOLLOW_ALL_SIDES, B_WILL_DRAW, false, true);
	box->AddChild(fStaticFilteringScrollView);
	itemSpacing = fStaticFilteringScrollView->Bounds().Height() + kItemExtraSpacing;
	frame.OffsetBy(0, itemSpacing);
	
	// add all existing filters
	TrackerFilters filters;
	const char *array[4] = { LOCALE("Name starts with:"),
		LOCALE("Name ends with:"), LOCALE("Name contains:"),
		LOCALE("Name matchs wildcard:") };
	
	for (int32 i = 0; i < filters.CountFilters(); i++) {
		BString buffer;
		buffer << array[filters.ExpressionTypeAt(i)] << " \"" << filters.ExpressionAt(i) << "\"";
		BStringItem *item = new BStringItem(buffer.String());
		fStaticFilteringListView->AddItem(item);
	}
	
	fStaticFilteringAddFilterButton = new BButton(frame, "StaticFilteringAddFilter", LOCALE("Add Filter"), new BMessage(kStaticFilterAdded));
	fStaticFilteringAddFilterButton->ResizeToPreferred();
	box->AddChild(fStaticFilteringAddFilterButton);
	itemSpacing = fStaticFilteringAddFilterButton->Bounds().Width() + kItemHeight;
	frame.OffsetBy(itemSpacing, 0);
	
	fStaticFilteringRemoveFilterButton = new BButton(frame, "StaticFilteringAddFilter", LOCALE("Remove Filter"), new BMessage(kStaticFilterRemoved));
	fStaticFilteringRemoveFilterButton->ResizeToPreferred();
	box->AddChild(fStaticFilteringRemoveFilterButton);
}

FilteringSettingsView::~FilteringSettingsView()
{
}

void
FilteringSettingsView::AttachedToWindow()
{
	fDynamicFilteringCheckBox->SetTarget(this);
	fDynamicFilteringInvertCheckBox->SetTarget(this);
	fDynamicFilteringIgnoreCaseCheckBox->SetTarget(this);
	fDynamicFilteringExpressionTypeMenuField->Menu()->SetTargetForItems(this);
	
	fStaticFilteringCheckBox->SetTarget(this);
	fStaticFilteringListView->SetTarget(this);
	fStaticFilteringAddFilterButton->SetTarget(this);
	fStaticFilteringRemoveFilterButton->SetTarget(this);
}

void
FilteringSettingsView::MessageReceived(BMessage *message)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	switch (message->what) {
		case kDynamicFilteringChanged:
		{
			gTrackerSettings.SetDynamicFiltering(fDynamicFilteringCheckBox->Value() == 1);
			
			// Make the notification message and send it to the tracker:
			BMessage notificationMessage;
			notificationMessage.AddBool("DynamicFiltering", fDynamicFilteringCheckBox->Value() == 1);
			tracker->SendNotices(kDynamicFilteringChanged, &notificationMessage);
			
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		case kDynamicFilteringExpressionTypeChanged:
		{
			if (!fDynamicFilteringExpressionTypeMenuField->LockLooper())
				break;
			
			BMenuItem *item = fDynamicFilteringExpressionTypeMenuField->Menu()->FindMarked();
			if (!item) {
				fDynamicFilteringExpressionTypeMenuField->UnlockLooper();
				break;
			}
			
			int32 index = fDynamicFilteringExpressionTypeMenuField->Menu()->IndexOf(item);
			
			fDynamicFilteringExpressionTypeMenuField->UnlockLooper();
			
			TrackerStringExpressionType typeArray[] = {	kStartsWith, kEndsWith,
				kContains, kGlobMatch, kRegexpMatch};
			
			gTrackerSettings.SetDynamicFilteringExpressionType(typeArray[index]);
			
			Window()->PostMessage(kSettingsContentsModified);
			BMessage notificationMessage;
			notificationMessage.AddInt32("DynamicFilteringExpressionType", typeArray[index]);
			tracker->SendNotices(kDynamicFilteringChanged, &notificationMessage);
			break;
		}
		
		case kDynamicFilteringInvertChanged:
		{
			gTrackerSettings.SetDynamicFilteringInvert(fDynamicFilteringInvertCheckBox->Value() == 1);
			
			BMessage notificationMessage;
			notificationMessage.AddBool("DynamicFilteringInvert", fDynamicFilteringInvertCheckBox->Value() == 1);
			tracker->SendNotices(kDynamicFilteringChanged, &notificationMessage);
			
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		case kDynamicFilteringIgnoreCaseChanged:
		{
			gTrackerSettings.SetDynamicFilteringIgnoreCase(fDynamicFilteringIgnoreCaseCheckBox->Value() == 1);
			
			BMessage notificationMessage;
			notificationMessage.AddBool("DynamicFilteringIgnoreCase", fDynamicFilteringIgnoreCaseCheckBox->Value() == 1);
			tracker->SendNotices(kDynamicFilteringChanged, &notificationMessage);
			
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		case kStaticFilteringChanged:
		{
			gTrackerSettings.SetStaticFiltering(fStaticFilteringCheckBox->Value() == 1);
			
			BMessage notificationMessage;
			notificationMessage.AddBool("StaticFiltering", fStaticFilteringCheckBox->Value() == 1);
			tracker->SendNotices(kStaticFilteringChanged, &notificationMessage);
			
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		case kStaticFilterAdded:
		{
			SelectionWindow *addwindow = new SelectionWindow(NULL, this, LOCALE("Add Filter"));
			addwindow->Show();
			break;
		}			
		
		case kStaticFilterRemoved:
		{
			int32 selection = fStaticFilteringListView->CurrentSelection();
			TrackerFilters().RemoveFilterAt(selection);
			delete fStaticFilteringListView->RemoveItem(selection);
			BMessage notificationMessage;
			notificationMessage.AddBool("StaticFilterRemoved", true);
			tracker->SendNotices(kStaticFilteringChanged, &notificationMessage);
			break;
		}
		
		case kSelectMatchingEntries:
		{
			int32 expressionType;
			message->FindInt32("ExpressionType", &expressionType);
			const char* expression;
			message->FindString("Expression", &expression);
			bool invert, ignoreCase;
			message->FindBool("InvertSelection", &invert);
			message->FindBool("IgnoreCase", &ignoreCase);
			char* array[4] = {"Name starts with: ", "Name ends with: ", "Name contains: ", "Name matchs wildcard: "};
			BString buffer;
			buffer << array[expressionType] << "\"" << expression << "\"";
			BStringItem *item = new BStringItem(buffer.String());
			fStaticFilteringListView->LockLooper();
			fStaticFilteringListView->AddItem(item);
			fStaticFilteringListView->UnlockLooper();
			TrackerStringExpressionType typeArray[] = {	kStartsWith, kEndsWith, kContains, kGlobMatch, kRegexpMatch};
			TrackerFilters().AddFilter(typeArray[expressionType], (char*)expression, invert, ignoreCase);
			BMessage notificationMessage;
			notificationMessage.AddBool("StaticFilterAdded", true);
			tracker->SendNotices(kStaticFilteringChanged, &notificationMessage);
		}
		
		default:
			_inherited::MessageReceived(message);
			break;
	}
}

void
FilteringSettingsView::SetDefaults()
{
	gTrackerSettings.SetDynamicFiltering(true);
	gTrackerSettings.SetDynamicFilteringExpressionType(kGlobMatch);
	gTrackerSettings.SetDynamicFilteringInvert(false);
	gTrackerSettings.SetDynamicFilteringIgnoreCase(true);
	gTrackerSettings.SetStaticFiltering(false);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
FilteringSettingsView::Revert()
{
	gTrackerSettings.SetDynamicFiltering(fDynamicFiltering);
	gTrackerSettings.SetDynamicFilteringExpressionType(fDynamicFilteringExpressionType);
	gTrackerSettings.SetDynamicFilteringInvert(fDynamicFilteringInvert);
	gTrackerSettings.SetDynamicFilteringIgnoreCase(fDynamicFilteringIgnoreCase);
	gTrackerSettings.SetStaticFiltering(fStaticFiltering);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
FilteringSettingsView::ShowCurrentSettings(bool sendNotices)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	fDynamicFilteringCheckBox->SetValue(gTrackerSettings.DynamicFiltering());
	fDynamicFilteringExpressionTypeMenuField->Menu()->ItemAt(gTrackerSettings.DynamicFilteringExpressionType())->SetMarked(true);
	fDynamicFilteringInvertCheckBox->SetValue(gTrackerSettings.DynamicFilteringInvert());
	fDynamicFilteringIgnoreCaseCheckBox->SetValue(gTrackerSettings.DynamicFilteringIgnoreCase());
	fStaticFilteringCheckBox->SetValue(gTrackerSettings.StaticFiltering());
	
	if (sendNotices) {
		BMessage notificationMessage;
		notificationMessage.AddBool("DynamicFiltering", gTrackerSettings.DynamicFiltering());
		notificationMessage.AddInt32("DynamicFilteringExpressionType", gTrackerSettings.DynamicFilteringExpressionType());
		notificationMessage.AddBool("DynamicFilteringInvert", gTrackerSettings.DynamicFilteringInvert());
		notificationMessage.AddBool("DynamicFilteringIgnoreCase", gTrackerSettings.DynamicFilteringIgnoreCase());
		
		tracker->SendNotices(kDynamicFilteringChanged, &notificationMessage);
		
		BMessage notificationMessage2;
		notificationMessage2.AddBool("StaticFiltering", gTrackerSettings.StaticFiltering());
		
		tracker->SendNotices(kStaticFilteringChanged, &notificationMessage2);
		
		Window()->PostMessage(kSettingsContentsModified);
	}
}

void
FilteringSettingsView::RecordRevertSettings()
{
	fDynamicFiltering = gTrackerSettings.DynamicFiltering();
	fDynamicFilteringExpressionType = gTrackerSettings.DynamicFilteringExpressionType();
	fDynamicFilteringInvert = gTrackerSettings.DynamicFilteringInvert();
	fDynamicFilteringIgnoreCase = gTrackerSettings.DynamicFilteringIgnoreCase();
	fStaticFiltering = gTrackerSettings.StaticFiltering();
}

bool
FilteringSettingsView::ShowsRevertSettings() const
{
	return ((fDynamicFiltering == (fDynamicFilteringCheckBox->Value() > 0)) &&
			(fDynamicFilteringExpressionType == (fDynamicFilteringExpressionTypeMenuField->Menu()->IndexOf(fDynamicFilteringExpressionTypeMenuField->Menu()->FindMarked()))) &&
			(fDynamicFilteringInvert == (fDynamicFilteringInvertCheckBox->Value() > 0)) &&
			(fDynamicFilteringIgnoreCase == (fDynamicFilteringIgnoreCaseCheckBox->Value() > 0)) &&
			(fStaticFiltering == (fStaticFilteringCheckBox->Value() > 0)));
}

//------------------------------------------------------------------------
// #pragma mark -

UndoSettingsView::UndoSettingsView(BRect rect)
	:	SettingsView(rect, "UndoSettingsView")
{
	BRect frame = BRect(kBorderSpacing, kBorderSpacing, rect.Width()
		- 2 * kBorderSpacing, kBorderSpacing + kItemHeight);
	
	fUndoEnabledCheckBox = new BCheckBox(frame, "", LOCALE("Enable Undo"),
		new BMessage(kUndoEnabledChanged));
	AddChild(fUndoEnabledCheckBox);
	fUndoEnabledCheckBox->ResizeToPreferred();
	
	float itemSpacing = fUndoEnabledCheckBox->Bounds().Height() + kItemExtraSpacing;
	frame.OffsetBy(0, itemSpacing);
	
	float divider = StringWidth(LOCALE("History Depth:")) + 5;
	fUndoDepthTextControl = new BTextControl(frame, "", LOCALE("History Depth:"), "10",
		new BMessage(kUndoDepthChanged));
	fUndoDepthTextControl->SetDivider(divider);
	AddChild(fUndoDepthTextControl);
}

UndoSettingsView::~UndoSettingsView()
{
}

void
UndoSettingsView::AttachedToWindow()
{
	fUndoEnabledCheckBox->SetTarget(this);
	fUndoDepthTextControl->SetTarget(this);
}

void
UndoSettingsView::MessageReceived(BMessage *message)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	switch (message->what) {
		case kUndoEnabledChanged: {
			fUndoDepthTextControl->SetEnabled(fUndoEnabledCheckBox->Value() == 1);
			gTrackerSettings.SetUndoEnabled(fUndoEnabledCheckBox->Value() == 1);
			
			// Make the notification message and send it to the tracker:
			BMessage notificationMessage;
			notificationMessage.AddBool("UndoEnabled", fUndoEnabledCheckBox->Value() == 1);
			tracker->SendNotices(kUndoEnabledChanged, &notificationMessage);
			
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		case kUndoDepthChanged:	{
			GetAndRefreshDisplayFigures();
			gTrackerSettings.SetUndoDepth(fDisplayedUndoDepth);
			
			// Make the notification message and send it to the tracker:
			BMessage message;
			message.AddInt32("UndoDepth", fDisplayedUndoDepth);
			tracker->SendNotices(kUndoDepthChanged, &message);
			
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}
		
		default:
			_inherited::MessageReceived(message);
			break;
	}
}

void
UndoSettingsView::SetDefaults()
{
	gTrackerSettings.SetUndoEnabled(true);
	gTrackerSettings.SetUndoDepth(10);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
UndoSettingsView::Revert()
{
	gTrackerSettings.SetUndoEnabled(fUndoEnabled);
	gTrackerSettings.SetUndoDepth(fUndoDepth);
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
UndoSettingsView::ShowCurrentSettings(bool sendNotices)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	fUndoEnabledCheckBox->SetValue(gTrackerSettings.UndoEnabled());
	
	int32 undoDepth = gTrackerSettings.UndoDepth();
	BString undoDepthText;
	undoDepthText << undoDepth;
	fUndoDepthTextControl->SetText(undoDepthText.String());
	
	if (sendNotices) {
		BMessage notificationMessage;
		notificationMessage.AddBool("UndoEnabled", gTrackerSettings.UndoEnabled());
		notificationMessage.AddInt32("UndoDepth", gTrackerSettings.UndoDepth());
		
		tracker->SendNotices(kUndoEnabledChanged, &notificationMessage);
	}
}

void
UndoSettingsView::RecordRevertSettings()
{
	fUndoEnabled = gTrackerSettings.UndoEnabled();
	fUndoDepth = gTrackerSettings.UndoDepth();
}

bool
UndoSettingsView::ShowsRevertSettings() const
{
	GetAndRefreshDisplayFigures();
	
	return ((fUndoEnabled == (fUndoEnabledCheckBox->Value() > 0)) &&
			(fUndoDepth == fDisplayedUndoDepth));
}

void
UndoSettingsView::GetAndRefreshDisplayFigures() const
{
	sscanf(fUndoDepthTextControl->Text(), "%ld", &fDisplayedUndoDepth);
	
	BString undoDepthText;
	undoDepthText << fDisplayedUndoDepth;
	fUndoDepthTextControl->SetText(undoDepthText.String());
}

//------------------------------------------------------------------------
// #pragma mark -

IconThemeSettingsView::IconThemeSettingsView(BRect rect)
	:	SettingsView(rect, "IconThemeSettingsView")
{
	BRect frame = BRect(kBorderSpacing, kBorderSpacing, rect.Width()
		- 2 * kBorderSpacing, rect.Height() - kBorderSpacing - kItemHeight);
	
	BButton testButton(frame, "", "", NULL);
	float buttonWidth = 20, buttonHeight = kItemHeight;
	testButton.GetPreferredSize(&buttonWidth, &buttonHeight);
	
	fThemesPopUp = new BOptionPopUp(frame, "ThemesPopUp", LOCALE("Select Theme:"), new BMessage(kCurrentIconThemeChanged));
	fThemesPopUp->ResizeToPreferred();
	fThemesPopUp->ResizeTo(Bounds().Width() - 10, fThemesPopUp->Bounds().Height());
	AddChild(fThemesPopUp);
	float itemSpacing = fThemesPopUp->Bounds().Height() + kItemExtraSpacing * 2;
	frame.OffsetBy(0, itemSpacing);
	
	// use largest string first to get proper width
	fAuthorLabel = new BStringView(frame, "AuthorLabel", LOCALE("Author:"));
	float max_width = 0;
	max_width = MAX(max_width, fAuthorLabel->StringWidth(LOCALE("Comment:")));
	max_width = MAX(max_width, fAuthorLabel->StringWidth(LOCALE("Author:")));
	max_width = MAX(max_width, fAuthorLabel->StringWidth(LOCALE("Link:")));
	fAuthorLabel->ResizeToPreferred();
	fAuthorLabel->ResizeTo(max_width, fAuthorLabel->Bounds().Height());
	AddChild(fAuthorLabel);
	float labelwidth = fAuthorLabel->Bounds().Width() + kItemExtraSpacing;
	float labelheight = fAuthorLabel->Bounds().Height() + kItemExtraSpacing * 2;
	frame.OffsetBy(labelwidth, 0);
	
	fAuthorString = new BStringView(frame, "AuthorString", "");
	fAuthorString->ResizeTo(Bounds().Width() - frame.left - kItemExtraSpacing * 2, fAuthorLabel->Bounds().Height());
	AddChild(fAuthorString);
	frame.OffsetBy(-labelwidth, labelheight);
	
	fCommentLabel = new BStringView(frame, "CommentLabel", LOCALE("Comment:"));
	fCommentLabel->ResizeToPreferred();
	AddChild(fCommentLabel);
	frame.OffsetBy(labelwidth, 0);
	
	fCommentString = new BStringView(frame, "CommentString", "");
	fCommentString->ResizeTo(Bounds().Width() - frame.left - kItemExtraSpacing * 2, fAuthorLabel->Bounds().Height());
	AddChild(fCommentString);
	frame.OffsetBy(-labelwidth, labelheight);
	
	fLinkLabel = new BStringView(frame, "LinkLabel", LOCALE("Link:"));
	fLinkLabel->ResizeToPreferred();
	AddChild(fLinkLabel);
	frame.OffsetBy(labelwidth, 0);
	
	fLinkString = new BLinkStringView(frame, "LinkString", "", "");
	fLinkString->ResizeTo(Bounds().Width() - frame.left - kItemExtraSpacing * 2, fAuthorLabel->Bounds().Height());
	AddChild(fLinkString);
	frame.OffsetBy(-labelwidth, labelheight);
	
	fSequenceOptions = new BButton(frame, "SequenceOptions", LOCALE("Lookup Sequence"B_UTF8_ELLIPSIS), new BMessage(kShowSequenceOptions));
	fSequenceOptions->ResizeToPreferred();
	// align left-bottom
	fSequenceOptions->MoveTo(frame.left, rect.Height() - fSequenceOptions->Bounds().Height() - kBorderSpacing);
	AddChild(fSequenceOptions);
	
	fApplyTheme = new BButton(frame, "ApplyTheme", LOCALE("Apply Theme"), new BMessage(kApplyIconTheme));
	fApplyTheme->ResizeToPreferred();
	// align right-bottom
	fApplyTheme->MoveTo(rect.Width() - fApplyTheme->Bounds().Width() - kBorderSpacing, rect.Height() - fApplyTheme->Bounds().Height() - kBorderSpacing);
	AddChild(fApplyTheme);
	
	fPreview = new ThemePreView(frame.OffsetByCopy(3, 0), "ThemePreView");
	fPreview->ResizeTo(rect.Width() - 2 * kBorderSpacing - 6, fApplyTheme->Frame().top - frame.top - B_H_SCROLL_BAR_HEIGHT - kItemExtraSpacing * 4);
	BScrollView *scrollview = new BScrollView("ThemePreViewScrollView", fPreview, B_FOLLOW_ALL_SIDES, B_WILL_DRAW, true, false);
	AddChild(scrollview);
}

IconThemeSettingsView::~IconThemeSettingsView()
{
	stop_watching(this);
}

void
IconThemeSettingsView::AttachedToWindow()
{
	fThemesPopUp->SetTarget(this);
	fApplyTheme->SetTarget(this);
	fSequenceOptions->SetTarget(this);
	
	fAuthorLabel->Invalidate();
	fAuthorString->Invalidate();
	fCommentLabel->Invalidate();
	fCommentString->Invalidate();
	fLinkLabel->Invalidate();
	fLinkString->Invalidate();
	
	// can watch directory now as we have a window
	UpdateThemeList(true);
	GetAndRefreshDisplayFigures();
	UpdateInfo();
}

void
IconThemeSettingsView::MessageReceived(BMessage *message)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	int32 temp;
	fThemesPopUp->SelectedOption(NULL, &temp);
	bool enabled = temp > 0 && temp < fThemesPopUp->CountOptions();
	
	switch (message->what) {
		case B_NODE_MONITOR:
			UpdateThemeList();
			break;
		
		case kCurrentIconThemeChanged: {
			GetAndRefreshDisplayFigures();
			UpdateInfo();
			fPreview->SetTo(fDisplayedTheme.String());
			UpdatePreview();
		} break;
		
		case kApplyIconTheme: {
			GetAndRefreshDisplayFigures();
			gTrackerSettings.SetIconThemeEnabled(enabled);
			gTrackerSettings.SetCurrentIconTheme(fDisplayedTheme.LockBuffer(0));
			fDisplayedTheme.UnlockBuffer();
			GetIconTheme()->RefreshTheme();
				// RefreshTheme will notify for us
			
			Window()->PostMessage(kSettingsContentsModified);
		} break;
		
		case kShowSequenceOptions: {
			SequenceWindow *window = new SequenceWindow(BRect(0, 0, 250, 120));
			window->MoveCloseToMouse();
			window->Show();
		} break;
		
		default: {
			_inherited::MessageReceived(message);
		} break;
	}
}

void
IconThemeSettingsView::SetDefaults()
{
	gTrackerSettings.SetIconThemeEnabled(false);
	gTrackerSettings.SetCurrentIconTheme("");
	gTrackerSettings.SetIconThemeLookupSequence("badecf");
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
IconThemeSettingsView::Revert()
{
	gTrackerSettings.SetIconThemeEnabled(fThemesEnabled);
	gTrackerSettings.SetCurrentIconTheme(fCurrentTheme.LockBuffer(-1));
	fCurrentTheme.UnlockBuffer();
	gTrackerSettings.SetIconThemeLookupSequence(fThemeLookupSequence.LockBuffer(-1));
	fThemeLookupSequence.UnlockBuffer();
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
IconThemeSettingsView::ShowCurrentSettings(bool sendNotices)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	if (fThemesPopUp->SelectOptionFor(gTrackerSettings.CurrentIconTheme()) != B_OK)
		fThemesPopUp->SelectOptionFor((int32)0);
	
	fPreview->SetTo(gTrackerSettings.CurrentIconTheme());
	fDisplayedTheme = gTrackerSettings.CurrentIconTheme();
	UpdatePreview();
	UpdateInfo();
	
	if (sendNotices) {
		BMessage notificationMessage;
		notificationMessage.AddBool("ThemesEnabled", gTrackerSettings.IconThemeEnabled());
		notificationMessage.AddString("CurrentTheme", gTrackerSettings.CurrentIconTheme());
		notificationMessage.AddString("ThemeLookupSequence", gTrackerSettings.IconThemeLookupSequence());
		
		tracker->SendNotices(kIconThemeChanged, &notificationMessage);
		Window()->PostMessage(kSettingsContentsModified);
	}
}

void
IconThemeSettingsView::RecordRevertSettings()
{
	fThemesEnabled = gTrackerSettings.IconThemeEnabled();
	fCurrentTheme = gTrackerSettings.CurrentIconTheme();
	fThemeLookupSequence = gTrackerSettings.IconThemeLookupSequence();
}

bool
IconThemeSettingsView::ShowsRevertSettings()
{
	GetAndRefreshDisplayFigures();
	return (fDisplayedTheme.Compare(fCurrentTheme) == 0);
}

void
IconThemeSettingsView::GetAndRefreshDisplayFigures()
{
	const char *name = NULL;
	int32 value = 0;
	
	if (!fThemesPopUp->SelectedOption(&name, &value) || value == 0)
		fDisplayedTheme = "";
	else
		fDisplayedTheme = name;
}

void
IconThemeSettingsView::UpdatePreview()
{
	fPreview->ResizeTo(fPreview->GetSize(), fPreview->Bounds().Height());
	BScrollBar *scrollbar = fPreview->ScrollBar(B_HORIZONTAL);
	scrollbar->SetRange(0, MAX(0, fPreview->Bounds().Width() - scrollbar->Bounds().Width()));
}

void
IconThemeSettingsView::UpdateInfo()
{
	BString path = "Tracker/Themes/";
	path << fDisplayedTheme.String();
	Settings info("info", path.String());
	
	StringValueSetting *author;
	info.Add(author = new StringValueSetting("Author", "", "", ""));
	StringValueSetting *comment;
	info.Add(comment = new StringValueSetting("Comment", "", "", ""));
	StringValueSetting *link;
	info.Add(link = new StringValueSetting("Link", "", "", ""));
	StringValueSetting *label;
	info.Add(label = new StringValueSetting("LinkLabel", "", "", ""));
	
	info.TryReadingSettings();
	
	fAuthorString->SetText(author->Value());
	fCommentString->SetText(comment->Value());
	
	BString string = label->Value();
	fLinkString->SetText((string.Length() > 0 ? string.String() : link->Value()));
	fLinkString->SetLink(link->Value());
}

void
IconThemeSettingsView::UpdateThemeList(bool dowatch)
{
	while (fThemesPopUp->CountOptions() > 0)
		fThemesPopUp->RemoveOptionAt(0);
	
	// add option to disable themes
	int32 index = 0;
	fThemesPopUp->AddOption(LOCALE("Disable Themes"), index++);
	
	BDirectory directory(GetIconTheme()->GetThemesPath().Path());
	if (directory.InitCheck() == B_OK) {
		directory.Rewind();
		BEntry entry;
		char name[B_FILE_NAME_LENGTH];
		
		while (directory.GetNextEntry(&entry) == B_OK) {
			if (!entry.IsDirectory())
				continue;
			
			entry.GetName(name);
			fThemesPopUp->AddOption(name, index++);
		}
		
		if (dowatch) {
			// watch the themes folder as themes may come and go
			node_ref ref;
			directory.GetNodeRef(&ref);
			TTracker::WatchNode(&ref, B_WATCH_DIRECTORY, this);
		}
	}
}

//------------------------------------------------------------------------
// #pragma mark -

LanguageThemeSettingsView::LanguageThemeSettingsView(BRect rect)
	:	SettingsView(rect, "LanguageThemeSettingsView")
{
	BRect frame = BRect(kBorderSpacing, kBorderSpacing, rect.Width()
		- 2 * kBorderSpacing, rect.Height() - kBorderSpacing - kItemHeight);
	
	BButton testButton(frame, "", "", NULL);
	float buttonWidth = 20, buttonHeight = kItemHeight;
	testButton.GetPreferredSize(&buttonWidth, &buttonHeight);
	
	fThemesPopUp = new BOptionPopUp(frame, "ThemesPopUp", LOCALE("Select Theme:"), new BMessage(kLanguageThemeChanged));
	fThemesPopUp->ResizeToPreferred();
	fThemesPopUp->ResizeTo(Bounds().Width() - 32, fThemesPopUp->Bounds().Height());
	AddChild(fThemesPopUp);
	float itemSpacing = fThemesPopUp->Bounds().Height() + kItemExtraSpacing * 2;
	frame.OffsetBy(0, itemSpacing);
	
	// use largest string first to get proper width
	fAuthorLabel = new BStringView(frame, "AuthorLabel", LOCALE("Author:"));
	float max_width = 0;
	max_width = MAX(max_width, fAuthorLabel->StringWidth(LOCALE("E-Mail:")));
	max_width = MAX(max_width, fAuthorLabel->StringWidth(LOCALE("Author:")));
	max_width = MAX(max_width, fAuthorLabel->StringWidth(LOCALE("Link:")));
	fAuthorLabel->ResizeToPreferred();
	fAuthorLabel->ResizeTo(max_width, fAuthorLabel->Bounds().Height());
	AddChild(fAuthorLabel);
	float labelwidth = fAuthorLabel->Bounds().Width() + kItemExtraSpacing;
	float labelheight = fAuthorLabel->Bounds().Height() + kItemExtraSpacing * 2;
	frame.OffsetBy(labelwidth, 0);
	
	fAuthorString = new BStringView(frame, "AuthorString", LOCALE("Translation:Author"));
	fAuthorString->ResizeTo(Bounds().Width() - frame.left - kItemExtraSpacing * 2, fAuthorLabel->Bounds().Height());
	AddChild(fAuthorString);
	frame.OffsetBy(-labelwidth, labelheight);
	
	fLinkLabel = new BStringView(frame, "LinkLabel", LOCALE("Link:"));
	fLinkLabel->ResizeToPreferred();
	AddChild(fLinkLabel);
	frame.OffsetBy(labelwidth, 0);
	
	fLinkString = new BLinkStringView(frame, "LinkString", "", LOCALE("Translation:LinkLabel"));
	fLinkString->ResizeTo(Bounds().Width() - frame.left - kItemExtraSpacing * 2, fAuthorLabel->Bounds().Height());
	AddChild(fLinkString);
	frame.OffsetBy(-labelwidth, labelheight);
	
	fEMailLabel = new BStringView(frame, "EMailLabel", LOCALE("E-Mail:"));
	fEMailLabel->ResizeToPreferred();
	AddChild(fEMailLabel);
	frame.OffsetBy(labelwidth, 0);
	
	fEMailString = new BStringView(frame, "EMailString", LOCALE("Translation:eMail"));
	fEMailString->ResizeTo(Bounds().Width() - frame.left - kItemExtraSpacing * 2, fAuthorLabel->Bounds().Height());
	AddChild(fEMailString);
	frame.OffsetBy(-labelwidth, labelheight);
	
	fApplyTheme = new BButton(frame, "ApplyTheme", LOCALE("Apply Theme"), new BMessage(kApplyLanguageTheme));
	fApplyTheme->ResizeToPreferred();
	// align right-bottom
	fApplyTheme->MoveTo(rect.Width() - fApplyTheme->Bounds().Width() - kBorderSpacing, rect.Height() - fApplyTheme->Bounds().Height() - kBorderSpacing);
	AddChild(fApplyTheme);
}

LanguageThemeSettingsView::~LanguageThemeSettingsView()
{
	stop_watching(this);
}

void
LanguageThemeSettingsView::AttachedToWindow()
{
	fThemesPopUp->SetTarget(this);
	fApplyTheme->SetTarget(this);
	
	fAuthorLabel->Invalidate();
	fAuthorString->Invalidate();
	fLinkLabel->Invalidate();
	fLinkString->Invalidate();
	fEMailLabel->Invalidate();
	fEMailString->Invalidate();
	
	// can watch directory now as we have a window
	UpdateThemeList(true);
	GetAndRefreshDisplayFigures();
	UpdateInfo();
}

void
LanguageThemeSettingsView::MessageReceived(BMessage *message)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	switch (message->what) {
		case B_NODE_MONITOR:
			UpdateThemeList();
			break;
		
		case kLanguageThemeChanged: {
			GetAndRefreshDisplayFigures();
			UpdateInfo();
		} break;
		
		case kApplyLanguageTheme: {
			GetAndRefreshDisplayFigures();
			gTrackerSettings.SetLanguageTheme(fDisplayedTheme.LockBuffer(0));
			fDisplayedTheme.UnlockBuffer();
			gLanguageTheme->SetTo(fDisplayedTheme.String());
			Window()->PostMessage(kSettingsContentsModified);
		} break;
		
		default: {
			_inherited::MessageReceived(message);
		} break;
	}
}

void
LanguageThemeSettingsView::SetDefaults()
{
	gTrackerSettings.SetLanguageTheme("English");
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
LanguageThemeSettingsView::Revert()
{
	gTrackerSettings.SetLanguageTheme(fCurrentTheme.LockBuffer(-1));
	fCurrentTheme.UnlockBuffer();
	
	ShowCurrentSettings(true);
		// true -> send notices about the change
}

void
LanguageThemeSettingsView::ShowCurrentSettings(bool sendNotices)
{
	TTracker *tracker = dynamic_cast<TTracker *>(be_app);
	if (!tracker)
		return;
	
	if (fThemesPopUp->SelectOptionFor(gTrackerSettings.LanguageTheme()) != B_OK)
		fThemesPopUp->SelectOptionFor((int32)0);
	
	fDisplayedTheme = gTrackerSettings.LanguageTheme();
	UpdateInfo();
	
	if (sendNotices) {
		BMessage notificationMessage;
		notificationMessage.AddString("LanguageTheme", gTrackerSettings.LanguageTheme());
		
		tracker->SendNotices(kLanguageThemeChanged, &notificationMessage);
		Window()->PostMessage(kSettingsContentsModified);
	}
}

void
LanguageThemeSettingsView::RecordRevertSettings()
{
	fCurrentTheme = gTrackerSettings.LanguageTheme();
}

bool
LanguageThemeSettingsView::ShowsRevertSettings()
{
	GetAndRefreshDisplayFigures();
	return (fDisplayedTheme.Compare(fCurrentTheme) == 0);
}

void
LanguageThemeSettingsView::GetAndRefreshDisplayFigures()
{
	const char *name = NULL;
	int32 value = 0;
	
	fThemesPopUp->SelectedOption(&name, &value);
	if (!name)
		fDisplayedTheme = "English";
	else
		fDisplayedTheme = name;
}

void
LanguageThemeSettingsView::UpdateInfo()
{
	LanguageTheme theme;
	theme.SetTo(fDisplayedTheme.String());
	
	BString author = theme.Translate("Translation:Author");
	BString link = theme.Translate("Translation:Link");
	BString linklabel = theme.Translate("Translation:LinkLabel");
	BString email = theme.Translate("Translation:eMail");
	
	if (author.Compare("Translation:Author") != 0)
		fAuthorString->SetText(author.String());
	else
		fAuthorString->SetText("");
	
	if (email.Compare("Translation:eMail") != 0)
		fEMailString->SetText(email.String());
	else
		fEMailString->SetText("");
	
	if (link.Compare("Translation:Link") != 0) {
		if (linklabel.Compare("Translation:LinkLabel") != 0
			&& linklabel.Length() > 0)
			fLinkString->SetText(linklabel.String());
		else
			fLinkString->SetText(link.String());
		
		fLinkString->SetLink(link.String());
	} else {
		fLinkString->SetText("");
		fLinkString->SetLink("");
	}
}

void
LanguageThemeSettingsView::UpdateThemeList(bool dowatch)
{
	while (fThemesPopUp->CountOptions() > 0)
		fThemesPopUp->RemoveOptionAt(0);
	
	int32 index = 0;
	BDirectory directory(gLanguageTheme->GetThemesPath().Path());
	if (directory.InitCheck() == B_OK) {
		directory.Rewind();
		BEntry entry;
		char name[B_FILE_NAME_LENGTH];
		
		while (directory.GetNextEntry(&entry) == B_OK) {
			if (!entry.IsFile() && !entry.IsSymLink())
				continue;
			
			entry.GetName(name);
			fThemesPopUp->AddOption(name, index++);
		}
		
		if (dowatch) {
			// watch the themes folder as themes may come and go
			node_ref ref;
			directory.GetNodeRef(&ref);
			TTracker::WatchNode(&ref, B_WATCH_DIRECTORY, this);
		}
	}
	
	if (fThemesPopUp->CountOptions() <= 0)
		fThemesPopUp->AddOption("English", index++);
}
