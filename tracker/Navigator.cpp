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
#include "Commands.h"
#include "ContainerWindow.h"
#include "IconTheme.h"
#include "Model.h"
#include "Navigator.h"
#include "PoseView.h"
#include "TFSContext.h"
#include "Tracker.h"
#include "TrackerFilters.h"
#include "TrackerSettings.h"
#include "TrackerString.h"
#include <TextControl.h>
#include <Window.h>
#include <Picture.h>
#include <Alert.h>

namespace BPrivate {

static const int32 kMaxHistory = 32;

static const rgb_color kBgColor = {220, 220, 220, 255};
static const rgb_color kShineColor = {255, 255, 255, 255};
static const rgb_color kHalfDarkColor = {200, 200, 200, 255};
static const rgb_color kDarkColor = {166, 166, 166, 255};

}

// BPictureButton() will crash when giving zero pointers,
// although we really want and have to set up the 
// pictures when we can, e.g. on a AttachedToWindow.
static BPicture sPicture;

BNavigatorButton::BNavigatorButton(BRect rect, const char *name, BMessage *message, 
	int32 resIDon, int32 resIDoff, int32 resIDdisabled)
	:	BPictureButton(rect, name, &sPicture, &sPicture, message),
		fResIDOn(resIDon),
		fResIDOff(resIDoff),
		fResIDDisabled(resIDdisabled)
{
	// Clear to background color to 
	// avoid ugly border on click
	SetViewColor(kBgColor);
	SetHighColor(kBgColor);
	SetLowColor(kBgColor);
}

BNavigatorButton::~BNavigatorButton()
{
}

void
BNavigatorButton::AttachedToWindow()
{
	SetPictures();
}

void
BNavigatorButton::SetPictures()
{
	BBitmap *bmpOn = 0;
	GetIconTheme()->GetThemeIconForResID(fResIDOn, B_MINI_ICON, bmpOn);
	SetPicture(bmpOn, true, true);
	delete bmpOn;
	
	BBitmap *bmpOff = 0;
	GetIconTheme()->GetThemeIconForResID(fResIDOff, B_MINI_ICON, bmpOff);
	SetPicture(bmpOff, true, false);
	delete bmpOff;

	BBitmap *bmpDisabled = 0;
	GetIconTheme()->GetThemeIconForResID(fResIDDisabled, B_MINI_ICON, bmpDisabled);
	SetPicture(bmpDisabled, false, false);
	SetPicture(bmpDisabled, false, true);
	delete bmpDisabled;
}

void
BNavigatorButton::SetPicture(BBitmap *bitmap, bool enabled, bool on)
{
	if (!bitmap)
		return;
	
	BPicture picture;
	BView view(bitmap->Bounds(), "", 0, 0);
	AddChild(&view);
	view.BeginPicture(&picture);
	view.SetHighColor(kBgColor);
	view.FillRect(view.Bounds());
	view.SetDrawingMode(B_OP_ALPHA);
	view.DrawBitmap(bitmap, BPoint(0, 0));
	view.EndPicture();
	RemoveChild(&view);
	if (enabled) {
		if (on)
			SetEnabledOn(&picture);
		else
			SetEnabledOff(&picture);
	} else {
		if (on)
			SetDisabledOn(&picture);
		else
			SetDisabledOff(&picture);
	}	
}

BNavigator::BNavigator(const Model *model, BRect rect, uint32 resizeMask)
	:	BView(rect, "Navigator", resizeMask, B_WILL_DRAW),
	fBack(NULL),
	fForw(NULL),
	fUp(NULL),
	fHome(NULL),
	fBackHistory(8, true),
	fForwHistory(8, true)
{
	// Get initial path
	model->GetPath(&fPath);
	
	SetViewColor(kBgColor);

	float top = 2 + (be_plain_font->Size() - 8) / 2;

	// Set up widgets
	fBack = new BNavigatorButton(BRect(3, top, 21, top + 17), "Back",
		new BMessage(kNavigatorCommandBackward), kResBackNavActiveSel,
		kResBackNavActive, kResBackNavInactive);
	fBack->SetEnabled(false);
	AddChild(fBack);

	fForw = new BNavigatorButton(BRect(35, top, 53, top + 17), "Forw",
		new BMessage(kNavigatorCommandForward), kResForwNavActiveSel,
		kResForwNavActive, kResForwNavInactive);
	fForw->SetEnabled(false);
	AddChild(fForw);

	fUp = new BNavigatorButton(BRect(67, top, 84, top + 17), "Up",
		new BMessage(kNavigatorCommandUp), kResUpNavActiveSel,
		kResUpNavActive, kResUpNavInactive);
	fUp->SetEnabled(false);
	AddChild(fUp);
	
	int offset = 97;

	fHome = new BNavigatorButton(BRect(97, top, 117, top + 17), "Home",
		new BMessage(kNavigatorCommandGoHome), kResHomeNavActiveSel,
		kResHomeNavActive, kResHomeNavInactive);
	fHome->SetEnabled(false);
	if (!gTrackerSettings.ShowHomeButton())
		fHome->Hide();
	else
		offset += 25;
	AddChild(fHome);
	
	fLocation = new BTextControl(BRect(offset, 2, rect.Width() - 2, 21),
		"Location", "", "", new BMessage(kNavigatorCommandLocation),
		B_FOLLOW_LEFT_RIGHT);
	fLocation->SetDivider(0);
	fLocation->SetModificationMessage(new BMessage(kNavigatorCommandFilter));
	AddChild(fLocation);
	
	fLocation->TextView()->AddFilter(new BMessageFilter(B_KEY_DOWN, &BNavigator::LocationBarFilter));
}

BNavigator::~BNavigator()
{
}

void
BNavigator::RefreshButtons()
{
	if (fBack)
		fBack->SetPictures();
	if (fForw)
		fForw->SetPictures();
	if (fUp)
		fUp->SetPictures();
	if (fHome)
		fHome->SetPictures();
}

void 
BNavigator::AttachedToWindow()
{	
	// Inital setup of widget states
	UpdateLocation(0, kActionSet);

	// All messages should arrive here
	fBack->SetTarget(this);
	fForw->SetTarget(this);
	fUp->SetTarget(this);
	fHome->SetTarget(this);
	fLocation->SetTarget(this);
}

void 
BNavigator::Draw(BRect)
{
	// Draws a beveled smooth border
	BeginLineArray(4);
	AddLine(Bounds().LeftTop(), Bounds().RightTop(), kShineColor);
	AddLine(Bounds().LeftTop(), Bounds().LeftBottom() - BPoint(0, 1), kShineColor);
	AddLine(Bounds().LeftBottom() - BPoint(-1, 1), Bounds().RightBottom() - BPoint(0, 1), kHalfDarkColor);
	AddLine(Bounds().LeftBottom(), Bounds().RightBottom(), kDarkColor);
	EndLineArray();
}

void 
BNavigator::MessageReceived(BMessage *message)
{
	switch (message->what) {		
		case kNavigatorCommandBackward:
			GoBackward((modifiers() & B_OPTION_KEY) == B_OPTION_KEY);
			break;

		case kNavigatorCommandForward:
			GoForward((modifiers() & B_OPTION_KEY) == B_OPTION_KEY);
			break;

		case kNavigatorCommandUp:
			GoUp((modifiers() & B_OPTION_KEY) == B_OPTION_KEY);
			break;

		case kNavigatorCommandLocation:
			GoTo();
			break;
			
		case kNavigatorCommandGoHome:
			fLocation->SetText(gTrackerSettings.HomeButtonDirectory());
			GoTo();
			break;
		
		case kNavigatorCommandFilter:
			{
				DoFiltering();
				break;
			}
		
		case kIconThemeChanged:
			break;
		
		default: {
			// Catch any dropped refs and try 
			// to switch to this new directory
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK) {
				BMessage message(kSwitchDirectory);
				BEntry entry(&ref, true);
				if (!entry.IsDirectory()) {
					entry.GetRef(&ref);
					BPath path(&ref);
						path.GetParent(&path);
						get_ref_for_path(path.Path(), &ref);
				}
				message.AddRef("refs", &ref);
				message.AddInt32("action", kActionSet);
				Window()->PostMessage(&message);
			}
		}
	}
}

void 
BNavigator::GoBackward(bool option)
{
	fSwitching = true;
	int32 itemCount = fBackHistory.CountItems();
	if (itemCount >= 2 && fBackHistory.ItemAt(itemCount - 2)) {
		BEntry entry;
		if (entry.SetTo(fBackHistory.ItemAt(itemCount - 2)->Path()) == B_OK)
			SendNavigationMessage(kActionBackward, &entry, option);
	}
	fSwitching = false;
}

void 
BNavigator::GoForward(bool option)
{
	fSwitching = true;
	if (fForwHistory.CountItems() >= 1) {
		BEntry entry;
		if (entry.SetTo(fForwHistory.LastItem()->Path()) == B_OK)
			SendNavigationMessage(kActionForward, &entry, option);
	}
	fSwitching = false;
}

void 
BNavigator::GoUp(bool option)
{
	fSwitching = true;
	BEntry entry;
	if (entry.SetTo(fPath.Path()) == B_OK) {
		BEntry parentEntry;
		if (entry.GetParent(&parentEntry) == B_OK)
			SendNavigationMessage(kActionUp, &parentEntry, option);
	}
	fSwitching = false;
}

void
BNavigator::SendNavigationMessage(NavigationAction action, BEntry *entry, bool option)
{
	entry_ref ref;

	if (entry->GetRef(&ref) == B_OK) {
		BMessage message;
		message.AddRef("refs", &ref);
		message.AddInt32("action", action);
		
		// get the node of this folder for selecting it in the new location
		const node_ref *nodeRef;
		if (Window() && Window()->TargetModel())
			nodeRef = Window()->TargetModel()->NodeRef();
		else
			nodeRef = NULL;
		
		// if the option key was held down, open in new window (send message to be_app)
		// otherwise send message to this window. TTracker (be_app) understands nodeRefToSlection,
		// BContainerWindow doesn't, so we have to select the item manually
		if (option) {
			message.what = B_REFS_RECEIVED;
			if (nodeRef)
				message.AddData("nodeRefToSelect", B_RAW_TYPE, nodeRef, sizeof(node_ref));
			be_app->PostMessage(&message);
		} else {
			message.what = kSwitchDirectory;
			Window()->PostMessage(&message);
			UnlockLooper();
				// This is to prevent a dead-lock situation. SelectChildInParentSoon()
				// eventually locks the TaskLoop::fLock. Later, when StandAloneTaskLoop::Run()
				// runs, it also locks TaskLoop::fLock and subsequently locks this window's looper.
				// Therefore we can't call SelectChildInParentSoon with our Looper locked,
				// because we would get different orders of locking (thus the risk of dead-locking).
				//
				// Todo: Change the locking behaviour of StandAloneTaskLoop::Run() and sub-
				// sequently called functions.
			if (nodeRef && !Window()->PoseView()->IsFilePanel())
				dynamic_cast<TTracker *>(be_app)->SelectChildInParentSoon(&ref, nodeRef);
			LockLooper();
		}
	}
}

void 
BNavigator::GoTo()
{
	fSwitching = true;
	BString pathname = fLocation->Text();

	if (pathname.Compare("") == 0)
		pathname = "/";
	
	BEntry entry;
	entry_ref ref;
	
	if (!Window()) {
		fSwitching = false;
		return;
	}

	if (entry.SetTo(pathname.String()) == B_OK
		&& entry.IsDirectory()
		&& entry.GetRef(&ref) == B_OK) {
		BMessage message(kSwitchDirectory);
		message.AddRef("refs", &ref);
		message.AddInt32("action", kActionLocation);
		Window()->PostMessage(&message);		
	} else if (fLocation->TextView()->IsFocus() && Window()->PoseView()->CountItems()) {
		Window()->PostMessage(new BMessage(kOpenSelectionOrFirst), Window()->PoseView());
	} else
		UpdateLocation(Window()->TargetModel(), kActionUpdatePath);
	
	Window()->PostMessage(new BMessage(kHideNoneMatchingEntries));
	fSwitching = false;
}

void 
BNavigator::UpdateLocation(const Model *newmodel, int32 action)
{
	if (newmodel)
		newmodel->GetPath(&fPath);
	
	// Modify history according to commands
	switch (action) {
		case kActionBackward:
			fForwHistory.AddItem(fBackHistory.RemoveItemAt(fBackHistory.CountItems()-1));
			break;
		case kActionForward:
			fBackHistory.AddItem(fForwHistory.RemoveItemAt(fForwHistory.CountItems()-1));
			break;
		case kActionUpdatePath:
			break;
		default:
			fForwHistory.MakeEmpty();
			fBackHistory.AddItem(new BPath(fPath));

			while (fBackHistory.CountItems() > kMaxHistory)
				fBackHistory.RemoveItem(fBackHistory.FirstItem(), true);
			break;			
	}
	
	// Enable Up button when there is any parent
	BEntry entry;
	if (entry.SetTo(fPath.Path()) == B_OK) {
		BEntry parentEntry;
		fUp->SetEnabled(entry.GetParent(&parentEntry) == B_OK);
	}
	
	// Enable history buttons if history contains something
	fForw->SetEnabled(fForwHistory.CountItems() > 0);
	fBack->SetEnabled(fBackHistory.CountItems() > 1);
	
	// Enable home button if not in homedir
	fHome->SetEnabled(strcmp(fPath.Path(), gTrackerSettings.HomeButtonDirectory()) != 0);
	
	// Avoid loss of selection and cursor position
	if (action != kActionLocation) {
		BString expression;
		Expression(expression);
		BString buffer = BString("");
		buffer << fPath.Path() << ((BString("/")).Compare(fPath.Path()) == 0 ? "" : "/") << (fSwitching ? expression : "");
		fLocation->SetText(buffer.String());
		fLocation->TextView()->Select(buffer.Length(), buffer.Length());
	}
}

float
BNavigator::CalcNavigatorHeight(void)
{
	// Empiric formula from how much space the textview
	// will take once it is attached (using be_plain_font):
	return  ceilf(11.0f + be_plain_font->Size()*(1.0f + 7.0f / 30.0f));
}

void 
BNavigator::ShowHomeButton(bool enabled)
{	
	if (enabled && !this->IsHidden()) {
		if (fHome->IsHidden()) {
			fHome->Show();
			fLocation->MoveTo(122, 2);
			fLocation->ResizeBy(-25, 0);
		}
	}
	else {
		if (!fHome->IsHidden()) {
			fHome->Hide();
			fLocation->MoveTo(97, 2);
			fLocation->ResizeBy(25, 0);
		}
	}
}

void
BNavigator::ClearExpression()
{
	BString buffer = BString("");
	buffer << fPath.Path() << ((BString("/")).Compare(fPath.Path()) == 0 ? "" : "/");
	fLocation->SetText(buffer.String());
	fLocation->TextView()->Select(buffer.Length(), buffer.Length());
}	

bool
BNavigator::Expression(BString &result)
{
	BString location = BString(fLocation->Text());
	int32 last = location.FindLast('/') + 1;
	BString teststring = BString(fPath.Path());
	if (teststring.Compare("/") != 0) // if we are in root we don't want a "//"
		teststring.Append("/");

	if (location.Compare(teststring, teststring.Length()) != 0)
		return false;
	else
		location.CopyInto(result, last, location.Length() - last);

	return true;
}

void
BNavigator::DoFiltering()
{
	if (BString(fLocation->Text()).FindFirst(B_ESCAPE) >= 0) {
		ClearExpression();
		return;
	}
	
	Window()->PostMessage(new BMessage(kHideNoneMatchingEntries));
}

filter_result
BNavigator::LocationBarFilter(BMessage *message, BHandler **target, BMessageFilter *filter)
{
	BView *view = dynamic_cast<BView *>(*target);
	
	if (view) {
		BContainerWindow *window = dynamic_cast<BContainerWindow *>(view->Window());
		if (window) {
			int32 raw;
			message->FindInt32("raw_char", &raw);
			if (raw == B_DOWN_ARROW || raw == B_UP_ARROW
				|| raw == B_PAGE_UP || raw == B_PAGE_DOWN) {
				int32 modifiers;
				message->FindInt32("modifiers", &modifiers);
				if ((modifiers & B_COMMAND_KEY) && !(modifiers & B_SHIFT_KEY) && !(modifiers & B_OPTION_KEY)) {
					// let navigation messages pass
					return B_DISPATCH_MESSAGE;
				}
				
				char key = (uint8)raw;
				window->PoseView()->KeyDown(&key, 1);
				return B_SKIP_MESSAGE;
			} else if (raw == B_TAB) {
				BString expression;
				if (window->Navigator()->Expression(expression) && expression.Length()) {
					BMessage message(kNavigatorTabCompletion);
					window->PoseView()->MessageReceived(&message);
					return B_SKIP_MESSAGE;
				}
			}
		}
	}
	
	return B_DISPATCH_MESSAGE;
}
