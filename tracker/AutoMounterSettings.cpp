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

#include <Button.h>
#include <Debug.h>
#include <Message.h>
#include <RadioButton.h>
#include <Window.h>

#include "AutoMounter.h"
#include "AutoMounterSettings.h"
#include "Defines.h"
#include "LanguageTheme.h"

const uint32 kDone = 'done';
const uint32 kMountAllNow = 'done';
const uint32 kAutomountSettingsChanged = 'achg';
const uint32 kBootMountSettingsChanged = 'bchg';
const uint32 kAutoAll = 'aall';
const uint32 kAutoBFS = 'abfs';
const uint32 kAutoHFS = 'ahfs';
const uint32 kInitAll = 'iall';
const uint32 kInitBFS = 'ibfs';
const uint32 kInitHFS = 'ihfs';

const int32 kCheckBoxSpacing = 20;
const int32 kBorderSpacing = 15;

AutomountSettingsDialog *AutomountSettingsDialog::fOneCopyOnly = NULL;

void 
AutomountSettingsDialog::RunAutomountSettings(AutoMounter *target)
{
	// either activate an existing mount settings dialog or create a new one
	if (fOneCopyOnly) {
		fOneCopyOnly->Activate();
		return;
	}
	
	BMessage message;
	target->GetSettings(&message);
	(new AutomountSettingsDialog(&message, target))->Show();
}

AutomountSettingsDialog::AutomountSettingsDialog(BMessage *settings,  
	AutoMounter *target)
	:	BWindow(BRect(100, 100, 320, 370), LOCALE("Disk Mount Settings"), 
			B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE)
{
	AutomountSettingsPanel *panel = new AutomountSettingsPanel(Bounds(), settings, target);
	panel->ResizeToPreferred();
	AddChild(panel);
	
	ASSERT(!fOneCopyOnly);
	fOneCopyOnly = this;
	
	Run();
	Lock();
	ResizeTo(panel->Frame().Width(), panel->Frame().bottom + 6);
	Unlock();
}

AutomountSettingsDialog::~AutomountSettingsDialog()
{
	ASSERT(fOneCopyOnly);
	fOneCopyOnly = NULL;
}


AutomountSettingsPanel::AutomountSettingsPanel(BRect frame, 
	BMessage *settings, AutoMounter *target)
	:	BBox(frame, "", B_FOLLOW_ALL, B_WILL_DRAW | B_FRAME_EVENTS
			| B_NAVIGABLE_JUMP, B_PLAIN_BORDER),
		fTarget(target)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	
	BRect checkBoxRect(Bounds());
	
	BRect boxRect(Bounds());
	boxRect.InsetBy(kBorderSpacing, kBorderSpacing);
	boxRect.bottom = boxRect.top + 85;
	fAutoBox = new BBox(boxRect, "fAutoBox", B_FOLLOW_LEFT_RIGHT,
		B_WILL_DRAW | B_FRAME_EVENTS | B_PULSE_NEEDED | B_NAVIGABLE_JUMP);
	fAutoBox->SetLabel(LOCALE("Automatic Disk Mounting:"));
	AddChild(fAutoBox);
	
	checkBoxRect = fAutoBox->Bounds();
	checkBoxRect.InsetBy(10, 18);
	
	checkBoxRect.bottom = checkBoxRect.top + 20;
	
	fScanningDisabledCheck = new BRadioButton(checkBoxRect, "scanningOff",
		LOCALE("Don't Automount"), new BMessage(kAutomountSettingsChanged));
	fScanningDisabledCheck->ResizeToPreferred();
	fAutoBox->AddChild(fScanningDisabledCheck);
	
	checkBoxRect.OffsetBy(0, kCheckBoxSpacing);
	fAutoMountAllBFSCheck = new BRadioButton(checkBoxRect, "autoBFS",
		LOCALE("All BeOS Disks"), new BMessage(kAutomountSettingsChanged));
	fAutoMountAllBFSCheck->ResizeToPreferred();
	fAutoBox->AddChild(fAutoMountAllBFSCheck);

	checkBoxRect.OffsetBy(0, kCheckBoxSpacing);
	fAutoMountAllCheck = new BRadioButton(checkBoxRect, "autoAll",
		LOCALE("All Disks"), new BMessage(kAutomountSettingsChanged));
	fAutoMountAllCheck->ResizeToPreferred();
	fAutoBox->AddChild(fAutoMountAllCheck);
	
	// lower box
	boxRect.OffsetTo(boxRect.left, boxRect.bottom + kBorderSpacing);
	boxRect.bottom = boxRect.top + 105;
	fInitialBox = new BBox(boxRect, "fInitialBox", B_FOLLOW_LEFT_RIGHT,
			B_WILL_DRAW | B_FRAME_EVENTS | B_PULSE_NEEDED | B_NAVIGABLE_JUMP);
	fInitialBox->SetLabel(LOCALE("Disk Mounting During Boot:"));
	AddChild(fInitialBox);
	
	checkBoxRect = fInitialBox->Bounds();
	checkBoxRect.InsetBy(10, 18);
	
	checkBoxRect.bottom = checkBoxRect.top + 20;
	fInitialDontMountCheck = new BRadioButton(checkBoxRect, "initialNone",
		LOCALE("Only The Boot Disk"), new BMessage(kBootMountSettingsChanged));
	fInitialDontMountCheck->ResizeToPreferred();
	fInitialBox->AddChild(fInitialDontMountCheck);

	checkBoxRect.OffsetBy(0, kCheckBoxSpacing);
	fInitialMountRestoreCheck = new BRadioButton(checkBoxRect, "initialRestore",
		LOCALE("Previously Mounted Disks"), new BMessage(kBootMountSettingsChanged));
	fInitialMountRestoreCheck->ResizeToPreferred();
	fInitialBox->AddChild(fInitialMountRestoreCheck);
	
	checkBoxRect.OffsetBy(0, kCheckBoxSpacing);
	fInitialMountAllBFSCheck = new BRadioButton(checkBoxRect, "initialBFS",
		LOCALE("All BeOS Disks"), new BMessage(kBootMountSettingsChanged));
	fInitialMountAllBFSCheck->ResizeToPreferred();
	fInitialBox->AddChild(fInitialMountAllBFSCheck);
	
	checkBoxRect.OffsetBy(0, kCheckBoxSpacing);
	fInitialMountAllCheck = new BRadioButton(checkBoxRect, "initialAll",
		LOCALE("All Disks"), new BMessage(kBootMountSettingsChanged));
	fInitialMountAllCheck->ResizeToPreferred();
	fInitialBox->AddChild(fInitialMountAllCheck);
	
	// buttons
	fDone = new BButton(Bounds(), "done", LOCALE("Done"), new BMessage(kDone),
		B_FOLLOW_RIGHT);
	fDone->ResizeToPreferred();
	fDone->MoveTo(Bounds().Width() - fDone->Frame().Width() - kBorderSpacing,
		Bounds().bottom - fDone->Frame().Height() - kBorderSpacing);
	AddChild(fDone);
	fDone->MakeDefault(true);
	
	fMountAllNow = new BButton(Bounds(), "mountAll", LOCALE("Mount all disks now"),
		new BMessage(kMountAllNow), B_FOLLOW_LEFT);
	fMountAllNow->ResizeToPreferred();
	fMountAllNow->MoveTo(kBorderSpacing,
		Bounds().bottom - fMountAllNow->Frame().Height() - kBorderSpacing);
	AddChild(fMountAllNow);
	
	bool result;
	if (settings->FindBool("autoMountAll", &result) == B_OK && result)
		fAutoMountAllCheck->SetValue(1);
	else if (settings->FindBool("autoMountAllBFS", &result) == B_OK && result)
		fAutoMountAllBFSCheck->SetValue(1);
	else
		fScanningDisabledCheck->SetValue(1);
	
	if (settings->FindBool("suspended", &result) == B_OK && result)
		fScanningDisabledCheck->SetValue(1);
	
	if (settings->FindBool("initialMountAll", &result) == B_OK && result)
		fInitialMountAllCheck->SetValue(1);
	else if (settings->FindBool("initialMountRestore", &result) == B_OK && result)
		fInitialMountRestoreCheck->SetValue(1);
	else if (settings->FindBool("initialMountAllBFS", &result) == B_OK && result)
		fInitialMountAllBFSCheck->SetValue(1);
	else
		fInitialDontMountCheck->SetValue(1);
}

AutomountSettingsPanel::~AutomountSettingsPanel()
{
}

void
AutomountSettingsPanel::SendSettings(bool rescan)
{
	BMessage message(kSetAutomounterParams);
	
	message.AddBool("autoMountAll", (bool)fAutoMountAllCheck->Value());
	message.AddBool("autoMountAllBFS", (bool)fAutoMountAllBFSCheck->Value());
	if (fAutoMountAllBFSCheck->Value())
		message.AddBool("autoMountAllHFS", false);

	message.AddBool("suspended", (bool)fScanningDisabledCheck->Value());
	message.AddBool("rescanNow", rescan);

	message.AddBool("initialMountAll", (bool)fInitialMountAllCheck->Value());
	message.AddBool("initialMountAllBFS", (bool)fInitialMountAllBFSCheck->Value());
	message.AddBool("initialMountRestore", (bool)fInitialMountRestoreCheck->Value());
	if (fInitialDontMountCheck->Value()) 
		message.AddBool("initialMountAllHFS", false);
	
	fTarget->PostMessage(&message, NULL);
}

void
AutomountSettingsPanel::AttachedToWindow()
{
	fInitialMountAllCheck->SetTarget(this);	
	fInitialMountAllBFSCheck->SetTarget(this);	
	fInitialMountRestoreCheck->SetTarget(this);	
	fInitialDontMountCheck->SetTarget(this);	
	fAutoMountAllCheck->SetTarget(this);	
	fAutoMountAllBFSCheck->SetTarget(this);	
	fScanningDisabledCheck->SetTarget(this);	
	fDone->SetTarget(this);	
	fMountAllNow->SetTarget(fTarget);
}

void
AutomountSettingsPanel::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case kDone:
		case B_QUIT_REQUESTED:
			Window()->Quit();
			break;

		case kAutomountSettingsChanged:
			SendSettings(true);
			break;

		case kBootMountSettingsChanged:
			SendSettings(false);
			break;

		default:
			_inherited::MessageReceived(message);
			break;
	}
}

void
AutomountSettingsPanel::ResizeToPreferred()
{
	float buttonWidth = 0;
	buttonWidth = kBorderSpacing + fMountAllNow->Frame().Width() + kBorderSpacing;
	buttonWidth += fDone->Frame().Width() + kBorderSpacing;
	
	float autoWidth = 0;
	autoWidth = fAutoBox->StringWidth(fAutoBox->Label());
	autoWidth += kBorderSpacing * 3;
	
	float initialWidth = 0;
	fInitialBox->ResizeToPreferred();
	initialWidth = fInitialBox->StringWidth(fInitialBox->Label());
	initialWidth += kBorderSpacing * 3;
	
	float boxWidth = MAX(autoWidth, initialWidth);
	ResizeTo(MAX(buttonWidth, boxWidth), Bounds().Height());
}
