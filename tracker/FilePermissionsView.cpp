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

#include "Defines.h"
#include <Beep.h>
#include <stdlib.h>
#include "FilePermissionsView.h"
#include "LanguageTheme.h"

const uint32 kPermissionsChanged = 'prch';
const uint32 kNewOwnerEntered = 'nwow';
const uint32 kNewGrupEntered = 'nwgr';

FilePermissionsView::FilePermissionsView(BRect rect, Model *model)
	:	BView(rect, "FilePermissionsView", B_FOLLOW_LEFT_RIGHT, B_WILL_DRAW),
		fModel(model),
		fRefs(NULL),
		fMultiple(false)
{
	InitCommon();
}

FilePermissionsView::FilePermissionsView(BRect rect, BObjectList<entry_ref> *refs)
	:	BView(rect, "MultipleFilePermissionsView", B_FOLLOW_LEFT_RIGHT, B_WILL_DRAW),
		fModel(NULL),
		fRefs(refs),
		fMultiple(true)
{
	InitCommon();
}

void
FilePermissionsView::ResizeToPreferred()
{
	ResizeTo(fGroupTextControl->Frame().right + 50, Bounds().Height());
}

void
FilePermissionsView::InitCommon()
{
#if !ENABLE_LANGUAGE_THEMES
	// Constants for the column labels: "User", "Group" and "Other".
	const float kColumnLabelMiddle = 77, kColumnLabelTop = 6, kColumnLabelSpacing = 37,
		kColumnLabelBottom = 20, kColumnLabelWidth = 35, kAttribFontHeight = 10;
#else
	const float kAttribFontHeight = 10, kColumnLabelTop = 6,
		kColumnLabelBottom = 20;
	// Variables based on the localised version of the Strings
	BFont font = be_plain_font;
	font.SetSize(kAttribFontHeight);
	
	float kColumnLabelMiddle = 0;
	kColumnLabelMiddle = MAX(kColumnLabelMiddle, font.StringWidth(LOCALE("Read")));
	kColumnLabelMiddle = MAX(kColumnLabelMiddle, font.StringWidth(LOCALE("Write")));
	kColumnLabelMiddle = MAX(kColumnLabelMiddle, font.StringWidth(LOCALE("Execute")));
	
	float kColumnLabelWidth = 0;
	kColumnLabelWidth = MAX(kColumnLabelWidth, font.StringWidth(LOCALE("Owner")));
	kColumnLabelWidth = MAX(kColumnLabelWidth, font.StringWidth(LOCALE("Group")));
	kColumnLabelWidth = MAX(kColumnLabelWidth, font.StringWidth(LOCALE("Other")));
	
	float kColumnLabelSpacing = kColumnLabelWidth + 2;
	kColumnLabelMiddle += kColumnLabelWidth / 2 + 17;
#endif
	
	BStringView *strView;
	
	strView = new BStringView(BRect(kColumnLabelMiddle - kColumnLabelWidth / 2,
		kColumnLabelTop, kColumnLabelMiddle + kColumnLabelWidth / 2, kColumnLabelBottom),
		"", LOCALE("Owner"));
	AddChild(strView);
	strView->SetAlignment(B_ALIGN_CENTER);
	strView->SetFontSize(kAttribFontHeight);
	
	strView = new BStringView(BRect(kColumnLabelMiddle - kColumnLabelWidth / 2
		+ kColumnLabelSpacing, kColumnLabelTop, kColumnLabelMiddle + kColumnLabelWidth / 2
		+ kColumnLabelSpacing, kColumnLabelBottom), "", LOCALE("Group"));
	AddChild(strView);
	strView->SetAlignment(B_ALIGN_CENTER);
	strView->SetFontSize(kAttribFontHeight);
	
	strView = new BStringView(BRect(kColumnLabelMiddle - kColumnLabelWidth / 2
		+ 2 * kColumnLabelSpacing, kColumnLabelTop, kColumnLabelMiddle + kColumnLabelWidth / 2
		+ 2 * kColumnLabelSpacing, kColumnLabelBottom), "", LOCALE("Other"));
	AddChild(strView);
	strView->SetAlignment(B_ALIGN_CENTER);
	strView->SetFontSize(kAttribFontHeight);
	
	// Constants for the row labels: "Read", "Write" and "Execute".
	const float kRowLabelLeft = 10, kRowLabelTop = kColumnLabelTop + 15,
		kRowLabelVerticalSpacing = 18, kRowLabelRight = kColumnLabelMiddle
		- kColumnLabelWidth / 2 - 5, kRowLabelHeight = 14;
	
	strView = new BStringView(BRect(kRowLabelLeft, kRowLabelTop, kRowLabelRight,
		kRowLabelTop + kRowLabelHeight), "", LOCALE("Read"));
	AddChild(strView);
	strView->SetAlignment(B_ALIGN_RIGHT);
	strView->SetFontSize(kAttribFontHeight);
	
	strView = new BStringView(BRect(kRowLabelLeft, kRowLabelTop
		+ kRowLabelVerticalSpacing, kRowLabelRight, kRowLabelTop
		+ kRowLabelVerticalSpacing + kRowLabelHeight), "", LOCALE("Write"));
	AddChild(strView);
	strView->SetAlignment(B_ALIGN_RIGHT);
	strView->SetFontSize(kAttribFontHeight);
	
	strView = new BStringView(BRect(kRowLabelLeft, kRowLabelTop
		+ 2 * kRowLabelVerticalSpacing, kRowLabelRight, kRowLabelTop
		+ 2 * kRowLabelVerticalSpacing + kRowLabelHeight), "", LOCALE("Execute"));
	AddChild(strView);
	strView->SetAlignment(B_ALIGN_RIGHT);
	strView->SetFontSize(kAttribFontHeight);
	
	// Constants for the 3x3 check box array.
	const float kLeftMargin = kRowLabelRight + 15, kTopMargin = kRowLabelTop - 2,
		kHorizontalSpacing = kColumnLabelSpacing, kVerticalSpacing = kRowLabelVerticalSpacing,
		kCheckBoxWidth = 18, kCheckBoxHeight = 18;
	
	FocusCheckBox **checkBoxArray[3][3] =
		{{ &fReadUserCheckBox, &fReadGroupCheckBox, &fReadOtherCheckBox },
		 { &fWriteUserCheckBox, &fWriteGroupCheckBox, &fWriteOtherCheckBox },
		 { &fExecuteUserCheckBox, &fExecuteGroupCheckBox, &fExecuteOtherCheckBox }};
	
	for (int32 x = 0; x < 3; x++)
		for (int32 y = 0; y < 3; y++) {
			*checkBoxArray[y][x] =
				new FocusCheckBox(BRect(kLeftMargin + kHorizontalSpacing * x,
					kTopMargin + kVerticalSpacing * y,
					kLeftMargin + kHorizontalSpacing * x + kCheckBoxWidth,
					kTopMargin + kVerticalSpacing * y + kCheckBoxHeight),
					"", "",	new BMessage(kPermissionsChanged));
			AddChild(*checkBoxArray[y][x]);
		}
	
#if !ENABLE_LANGUAGE_THEMES
	const float kTextControlLeft = 170, kTextControlRight = 270,
		kTextControlTop = kColumnLabelTop, kTextControlHeight = 14, kTextControlSpacing = 16;
#else
	const float kTextControlTop = kColumnLabelTop, kTextControlHeight = 14, kTextControlSpacing = 16;
	float kTextControlLeft = kColumnLabelMiddle + kColumnLabelWidth / 2 + 2 * kColumnLabelSpacing;
	float kTextControlRight = 100;
	kTextControlRight = MAX(kTextControlRight, font.StringWidth(LOCALE("Owner")));
	kTextControlRight = MAX(kTextControlRight, font.StringWidth(LOCALE("Group")));
	kTextControlRight += kTextControlLeft;
#endif
	strView = new BStringView(BRect(kTextControlLeft, kTextControlTop, kTextControlRight,
		kTextControlTop + kTextControlHeight), "", LOCALE("Owner"));
	
	strView->SetAlignment(B_ALIGN_CENTER);
	strView->SetFontSize(kAttribFontHeight);
	
	AddChild(strView);
	
	fOwnerTextControl = new BTextControl(BRect(kTextControlLeft, kTextControlTop - 2
		+ kTextControlSpacing, kTextControlRight, kTextControlTop + kTextControlHeight - 2
		+ kTextControlSpacing), "",	"", "", new BMessage(kNewOwnerEntered));
	
	fOwnerTextControl->SetDivider(0);
	
	AddChild(fOwnerTextControl);
	
	strView = new BStringView(BRect(kTextControlLeft, kTextControlTop + 5
		+ 2 * kTextControlSpacing, kTextControlRight, kTextControlTop + 2
		+ 2 * kTextControlSpacing + kTextControlHeight), "", LOCALE("Group"));
	
	strView->SetAlignment(B_ALIGN_CENTER);
	strView->SetFontSize(kAttribFontHeight);
	
	AddChild(strView);
	
	fGroupTextControl = new BTextControl(BRect(kTextControlLeft, kTextControlTop
		+ 3 * kTextControlSpacing, kTextControlRight, kTextControlTop
		+ 3 * kTextControlSpacing + kTextControlHeight), "", "", "",
		new BMessage(kNewGrupEntered));
	
	fGroupTextControl->SetDivider(0);
	
	AddChild(fGroupTextControl);
	
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	
	if (!fMultiple)
		ModelChanged(fModel);
}


void
FilePermissionsView::ModelChanged(Model *model)
{
	fModel = model;
	
	bool hideCheckBoxes = false;
	uid_t nodeOwner = 0;
	gid_t nodeGroup = 0;
	mode_t perms = 0;

	if (fModel != NULL) {
		BNode node(fModel->EntryRef());
	
		if (node.InitCheck() == B_OK) {
			
			if (fReadUserCheckBox->IsHidden()) {
				fReadUserCheckBox->Show();
				fReadGroupCheckBox->Show();
				fReadOtherCheckBox->Show();
				fWriteUserCheckBox->Show();
				fWriteGroupCheckBox->Show();
				fWriteOtherCheckBox->Show();
				fExecuteUserCheckBox->Show();
				fExecuteGroupCheckBox->Show();
				fExecuteOtherCheckBox->Show();
			}		

			if (node.GetPermissions(&perms) == B_OK) {
				fReadUserCheckBox->SetValue((int32)(perms & S_IRUSR));
				fReadGroupCheckBox->SetValue((int32)(perms & S_IRGRP));
				fReadOtherCheckBox->SetValue((int32)(perms & S_IROTH));
				fWriteUserCheckBox->SetValue((int32)(perms & S_IWUSR));
				fWriteGroupCheckBox->SetValue((int32)(perms & S_IWGRP));
				fWriteOtherCheckBox->SetValue((int32)(perms & S_IWOTH));
				fExecuteUserCheckBox->SetValue((int32)(perms & S_IXUSR));
				fExecuteGroupCheckBox->SetValue((int32)(perms & S_IXGRP));
				fExecuteOtherCheckBox->SetValue((int32)(perms & S_IXOTH));
			} else
				hideCheckBoxes = true;
			
			if (node.GetOwner(&nodeOwner) == B_OK) {
				BString user;
				if (nodeOwner == 0)
					if (getenv("USER") != NULL)
						user << getenv("USER");
					else
						user << "root";
				else
					user << nodeOwner;
				fOwnerTextControl->SetText(user.String());	
			} else
				fOwnerTextControl->SetText(LOCALE("Unknown"));	
			
			if (node.GetGroup(&nodeGroup) == B_OK) {
				BString group;
				if (nodeGroup == 0)
					if (getenv("GROUP") != NULL)
						group << getenv("GROUP");
					else
						group << "0";
				else
					group << nodeGroup;
				fGroupTextControl->SetText(group.String());	
			} else
				fGroupTextControl->SetText(LOCALE("Unknown"));	
				
			// Unless we're root, only allow the owner to transfer the ownership,
			// i.e. disable text controls if uid:s doesn't match:
			thread_id thisThread = find_thread(NULL);
			thread_info threadInfo;
			get_thread_info(thisThread, &threadInfo);
			team_info teamInfo;
			get_team_info(threadInfo.team, &teamInfo);
			if (teamInfo.uid != 0 && nodeOwner != teamInfo.uid) {
				fOwnerTextControl->SetEnabled(false);
				fGroupTextControl->SetEnabled(false);
			} else {
				fOwnerTextControl->SetEnabled(true);
				fGroupTextControl->SetEnabled(true);
			}

		} else
			hideCheckBoxes = true;
	} else
		hideCheckBoxes = true;
			
	if (hideCheckBoxes) {
		fReadUserCheckBox->Hide();
		fReadGroupCheckBox->Hide();
		fReadOtherCheckBox->Hide();
		fWriteUserCheckBox->Hide();
		fWriteGroupCheckBox->Hide();
		fWriteOtherCheckBox->Hide();
		fExecuteUserCheckBox->Hide();
		fExecuteGroupCheckBox->Hide();
		fExecuteOtherCheckBox->Hide();
	}
}
	
void
FilePermissionsView::MessageReceived(BMessage *message)
{
	switch(message->what) {
		case kPermissionsChanged:
			if (fModel != NULL) {
				
				mode_t newPermissions = 0;
				newPermissions = (mode_t)((fReadUserCheckBox->Value() ? S_IRUSR : 0)
					| (fReadGroupCheckBox->Value() ? S_IRGRP : 0)
					| (fReadOtherCheckBox->Value() ? S_IROTH : 0)
								 
					| (fWriteUserCheckBox->Value() ? S_IWUSR : 0)
					| (fWriteGroupCheckBox->Value() ? S_IWGRP : 0)
					| (fWriteOtherCheckBox->Value() ? S_IWOTH : 0)

					| (fExecuteUserCheckBox->Value() ? S_IXUSR : 0)
					| (fExecuteGroupCheckBox->Value() ? S_IXGRP :0)
					| (fExecuteOtherCheckBox->Value() ? S_IXOTH : 0));
				
				
				BNode node(fModel->EntryRef());

				if (node.InitCheck() == B_OK) 
					node.SetPermissions(newPermissions);	
				else {
					ModelChanged(fModel);
					beep();	
				}
			}
			break;
			
		case kNewOwnerEntered:
			if (fModel != NULL) {
				uid_t owner;
				if (sscanf(fOwnerTextControl->Text(), "%d", &owner) == 1) {
					BNode node(fModel->EntryRef());
					if (node.InitCheck() == B_OK)
						node.SetOwner(owner);
					else {
						ModelChanged(fModel);
						beep();	
					}
				} else {
					ModelChanged(fModel);
					beep();	
				}
			}
			break;
	
		case kNewGrupEntered:
			if (fModel != NULL) {
				gid_t group;
				if (sscanf(fGroupTextControl->Text(), "%d", &group) == 1) {
					BNode node(fModel->EntryRef());
					if (node.InitCheck() == B_OK)
						node.SetGroup(group);
					else {
						ModelChanged(fModel);
						beep();	
					}
				} else {
					ModelChanged(fModel);
					beep();	
				}
			}
			break;
			
		default:
			_inherited::MessageReceived(message);
			break;
	}
}

void
FilePermissionsView::AttachedToWindow()
{
	fReadUserCheckBox->SetTarget(this);
	fReadGroupCheckBox->SetTarget(this);
	fReadOtherCheckBox->SetTarget(this);
	fWriteUserCheckBox->SetTarget(this);
	fWriteGroupCheckBox->SetTarget(this);
	fWriteOtherCheckBox->SetTarget(this);
	fExecuteUserCheckBox->SetTarget(this);
	fExecuteGroupCheckBox->SetTarget(this);
	fExecuteOtherCheckBox->SetTarget(this);
	
	fOwnerTextControl->SetTarget(this);
	fGroupTextControl->SetTarget(this);
}

