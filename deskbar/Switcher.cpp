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

#ifndef _ALLOW_STICKY_
#define _ALLOW_STICKY_ 0
#endif

#include <Bitmap.h>
#include <Debug.h>
#include <Font.h>
#include <Mime.h>
#include <Node.h>
#include <NodeInfo.h>
#include <Roster.h>
#include <Screen.h>
#include <String.h>

#include <string.h>
#include <stdlib.h>
#include <float.h>

#include "BarApp.h"
#include "Switcher.h"
#include "ResourceSet.h"
#include "WindowMenuItem.h"
#include "icons.h"

static bool IsKeyDown(int32 key);
static bool OKToUse(const TTeamGroup *);
static bool IsWindowOK(const window_info *);
static int SmartStrcmp(const char *s1, const char *s2);


inline bool	IsVisibleInCurrentWorkspace(const window_info *windowInfo)
{
	/*
	 The window list is always ordered from the top
	 front visible window (the first on the list), going down through all
	 the other visible windows, then all the hidden or non workspace
	 visible window at the end.
  
	 layer > 2 : normal visible window.
	 layer == 2 : reserved for the desktop window (visible also).
	 layer < 2 : hidden (0) and non workspace visible window (1)
	*/
	return windowInfo->layer > 2;
}


TTeamGroup::TTeamGroup()
	:	fTeams(NULL),
		fFlags(0),
		fName(NULL),
		fSmallIcon(NULL),
		fLargeIcon(NULL)
		
{
	fSig[0] = '\0';
}


TTeamGroup::TTeamGroup(BList *teams, uint32 flags, char *name,
	const char *sig)
	:	fTeams(teams),
		fFlags(flags),
		fName(name),
		fSmallIcon(NULL),
		fLargeIcon(NULL)
{
	strcpy(fSig, sig);

	fSmallIcon = new BBitmap(BRect(0,0,15,15), B_COLOR_8_BIT);
	fLargeIcon = new BBitmap(BRect(0,0,31,31), B_COLOR_8_BIT);

	app_info appInfo;
	if (be_roster->GetAppInfo(sig, &appInfo) == B_OK) {
		BNode node(&(appInfo.ref));
		if (node.InitCheck() == B_OK) {
			BNodeInfo nodeInfo(&node);
			if (nodeInfo.InitCheck() == B_OK) {
				nodeInfo.GetTrackerIcon(fSmallIcon, B_MINI_ICON);
				nodeInfo.GetTrackerIcon(fLargeIcon, B_LARGE_ICON);
			}
		}
	}
}


TTeamGroup::~TTeamGroup()
{
	delete fTeams;
	free(fName);
	delete fSmallIcon;
	delete fLargeIcon;
}


void
TTeamGroup::Draw(BView *view, BRect bounds, bool main)
{
	BRect rect;
	if (main) {
		rect = fLargeIcon->Bounds();
		rect.OffsetTo(bounds.LeftTop());
		rect.OffsetBy(2,2);
		view->DrawBitmap(fLargeIcon, rect);
	} else {
		rect = fSmallIcon->Bounds();
		rect.OffsetTo(bounds.LeftTop());
		rect.OffsetBy(10,10);
		view->DrawBitmap(fSmallIcon, rect);
	}
}


//	#pragma mark -

const int32 kHorizontalMargin = 11;
const int32 kVerticalMargin = 10;

// SLOT_SIZE must be divisible by 4. That's because of the scrolling
// animation. If this needs to change then look at TIconView::Update()

const int32 kSlotSize = 36;
const int32 kScrollStep = kSlotSize / 2;
const int32 kNumSlots = 7;
const int32 kCenterSlot = 3;

TSwitchMgr::TSwitchMgr(BPoint point)
	:	BHandler("SwitchMgr"),
		fMainMonitor(create_sem(1, "main_monitor")),
		fBlock(false),
		fSkipUntil(0),
		fGroupList(10),
		fCurIndex(0),
		fCurSlot(0),
		fWindowID(-1),
		fLastActivity(0)
{
	BRect rect(point.x, point.y,
		point.x + (kSlotSize * kNumSlots) - 1 + (2 * kHorizontalMargin),
		point.y + 82);
	fWindow = new TSwitcherWindow(rect, this);
	fWindow->AddHandler(this);

	fWindow->Lock();
	fWindow->Run();

	BList tmpList;
	TBarApp::Subscribe(BMessenger(this), &tmpList);

	for (int32 i = 0; ; i++) {
		BarTeamInfo	*barTeamInfo = (BarTeamInfo	*)tmpList.ItemAt(i);
		if (!barTeamInfo)
			break;

		TTeamGroup *tinfo = new TTeamGroup(barTeamInfo->teams, barTeamInfo->flags,
			barTeamInfo->name, barTeamInfo->sig);
		fGroupList.AddItem(tinfo);

		barTeamInfo->teams = NULL;
		barTeamInfo->name = NULL;

		delete barTeamInfo;
	}
	fWindow->Unlock();
}


TSwitchMgr::~TSwitchMgr()
{
	for (int32 i = fGroupList.CountItems(); i-- > 0;) {
		TTeamGroup *teamInfo = static_cast<TTeamGroup *>(fGroupList.ItemAt(i));
		delete teamInfo;
	}
}


void
TSwitchMgr::MessageReceived(BMessage *message)
{
	status_t err;

	switch (message->what) {
		case B_SOME_APP_QUIT:
			{
				// This is only sent when last team of a matching set quits
				team_id teamID;
				int i = 0;
				TTeamGroup *tinfo;
				message->FindInt32("team", &teamID);
				while ((tinfo = (TTeamGroup *) fGroupList.ItemAt(i)) != NULL) {
					if (tinfo->TeamList()->HasItem((void *)teamID)) {
						fGroupList.RemoveItem(i);
	
						if (OKToUse(tinfo)) {
							fWindow->Redraw(i);
							if (i <= fCurIndex) {
								fCurIndex--;
								CycleApp(true);
							}
						}
						delete tinfo;
						break;
					}
					i++;
				}
				break;
			}

		case B_SOME_APP_LAUNCHED:
			{
				BList *teams;
				const char *name;
				BBitmap	*smallIcon;
				uint32 flags;
				const char *sig;
	
				if (message->FindPointer("teams", (void **)&teams) != B_OK) 
					break;

				if (message->FindPointer("icon", (void **)&smallIcon) != B_OK) {
					delete teams;
					break;
				}
				delete smallIcon;
				if (message->FindString("sig", &sig) != B_OK) {
					delete teams;
					break;
				}
				if (message->FindInt32("flags", (int32 *)&flags) != B_OK) {
					delete teams;
					break;
				}
				if (message->FindString("name", &name) != B_OK) {
					delete teams;
					break;
				}
	
				TTeamGroup *tinfo = new TTeamGroup(teams, flags, strdup(name), sig);
				
				fGroupList.AddItem(tinfo);
				if (OKToUse(tinfo)) 
					fWindow->Redraw(fGroupList.CountItems() - 1);

				break;
			}
		case msg_AddTeam:
			{
				const char *sig = message->FindString("sig");
				team_id team = message->FindInt32("team");			
	
				int32 numItems = fGroupList.CountItems();
				for (int32 i = 0; i < numItems; i++) {
					TTeamGroup *tinfo = (TTeamGroup *)fGroupList.ItemAt(i);
					if (strcasecmp(tinfo->Sig(), sig) == 0) {
						if (!(tinfo->TeamList()->HasItem((void *)team)))
							tinfo->TeamList()->AddItem((void *)team);
						break;
					}
				}		
				break;
			}
		case msg_RemoveTeam:
			{
				team_id team = message->FindInt32("team");
	
				int32 numItems = fGroupList.CountItems();
				for (int32 i = 0; i < numItems; i++) {
					TTeamGroup *tinfo = (TTeamGroup *)fGroupList.ItemAt(i);
					if (tinfo->TeamList()->HasItem((void *)team)) {
						tinfo->TeamList()->RemoveItem((void *)team);
						break;
					}
				}		
				break;
			}

		case 'TASK':
			{
				
				// The first TASK message calls MainEntry. Subsequent ones
				// call Process().
				bigtime_t time;
				message->FindInt64("when", (int64 *)&time);
	
				// The fSkipUntil stuff can be removed once the new input_server
				// starts differentiating initial key_downs from KeyDowns generated
				// by auto-repeat. Until then the fSkipUntil stuff helps, but it
				// isn't perfect.
	
				if (time < fSkipUntil)
					break;

				err = acquire_sem_etc(fMainMonitor, 1, B_TIMEOUT, 0);
				if (err != B_OK) {
					if (!fWindow->IsHidden() && !fBlock) {
						// Want to skip TASK msgs posted before the window
						// was made visible. Better UI feel if we do this.
						if (time > fSkipUntil) {
							uint32	mods;
							message->FindInt32("modifiers", (int32 *)&mods);
							Process((mods & B_SHIFT_KEY) == 0, (mods & B_OPTION_KEY) != 0);
						}
					}
				} else 
					MainEntry(message);

				break;
			}

		default:
			break;
	}
}


void
TSwitchMgr::MainEntry(BMessage *message)
{
	bigtime_t keyRepeatRate;
	get_key_repeat_delay(&keyRepeatRate);

	if (keyRepeatRate < 200000) 
		keyRepeatRate = 200000;

	bigtime_t timeout = system_time() + keyRepeatRate;

	app_info appInfo;
	be_roster->GetActiveAppInfo(&appInfo);

	int32 index;
	fCurIndex = (FindTeam(appInfo.team, &index) != NULL) ? index : 0;

	int32 key;
	message->FindInt32("key", (int32 *)&key);

	uint32 modifierKeys = 0;
	while (system_time() < timeout) {
		modifierKeys = modifiers();
		if (!IsKeyDown(key)) {
			QuickSwitch(message);
			return;
		}
		if ((modifierKeys & B_CONTROL_KEY) == 0) {
			QuickSwitch(message);
			return;
		}
		snooze(50000);
	}

	Process((modifierKeys & B_SHIFT_KEY) == 0, (modifierKeys & B_OPTION_KEY) != 0);
}


void
TSwitchMgr::Stop(bool do_action, uint32 )
{
	fWindow->Hide();
	if (do_action)
		ActivateApp(true, true);
	
	release_sem(fMainMonitor);
}


TTeamGroup *
TSwitchMgr::FindTeam(team_id teamID, int32 *index)
{
	int i = 0;
	TTeamGroup	*tinfo;
	while ((tinfo = (TTeamGroup *)fGroupList.ItemAt(i)) != NULL) {
		if (tinfo->TeamList()->HasItem((void*) teamID)) {
			*index = i;
			return tinfo;
		}
		i++;
	}

	return NULL;
}


void
TSwitchMgr::Process(bool forward, bool byWindow)
{
	bool hidden = false;
	if (fWindow->Lock()) {
		hidden = fWindow->IsHidden();
		fWindow->Unlock();
	}

	if (byWindow) {
		// If hidden we need to get things started by switching to correct app
		if (hidden)
			SwitchToApp(fCurIndex, fCurIndex, forward);
		CycleWindow(forward, false);
	} else
		CycleApp(forward, false);

	if (hidden) {
		// more auto keyrepeat code
		// Because of key repeats we don't want to respond to any extraneous
		// 'TASK' messages until the window is completely shown. So block here.
		// the WindowActivated hook function will unblock.
		fBlock = true;

		if (fWindow->Lock()) {
			BRect screenFrame = BScreen().Frame();
			BRect windowFrame = fWindow->Frame();

			if (!screenFrame.Contains(windowFrame)) {
				// center the window
				BPoint point((screenFrame.left + screenFrame.right) / 2,
					(screenFrame.top + screenFrame.bottom) / 2);

				point.x -= (windowFrame.Width() / 2);
				point.y -= (windowFrame.Height() / 2);
				fWindow->MoveTo(point);
			}

			fWindow->Show();
			fWindow->Unlock();
		}
	}
}


void
TSwitchMgr::QuickSwitch(BMessage *message)
{
	uint32 modifiers = 0;
	message->FindInt32("modifiers", (int32 *) &modifiers);

	team_id team;
	if (message->FindInt32("team", &team) == B_OK) {
	
		bool forward = ((modifiers & B_SHIFT_KEY) == 0);
	
		if ((modifiers & B_OPTION_KEY) != 0) 
			SwitchWindow(team, forward, true);
		else 
			CycleApp(forward, true);
	}

	release_sem(fMainMonitor);
}


int32
TSwitchMgr::CountVisibleGroups()
{
	int32 result = 0;

	int32 count = fGroupList.CountItems();
	for (int32 i = 0; i < count; i++) {
		if (!OKToUse((TTeamGroup *) fGroupList.ItemAt(i)))
			continue;

		result++;
	}
	return result;
}


void
TSwitchMgr::CycleWindow(bool forward, bool wrap)
{
	int32 max = CountWindows(fCurIndex);
	int32 prev = fCurWindow;
	int32 next = fCurWindow;

	if (forward) {
		next++;
		if (next >= max) {
			if (!wrap)
				return;
			next = 0;
		}
	} else {
		next--;
		if (next < 0) {
			if (!wrap)
				return;
			next = max - 1;
		}
	}
	fCurWindow = next;

	if (fCurWindow != prev)
		fWindow->WindowView()->ShowIndex(fCurWindow);
}


void
TSwitchMgr::CycleApp(bool forward, bool activateNow)
{
	int32 startIndex = fCurIndex;
	int32 max = fGroupList.CountItems();

	for (;;) {
		if (forward) {
			fCurIndex++;
			if (fCurIndex >= max) 
				fCurIndex = 0;
		} else {
			fCurIndex--;
			if (fCurIndex < 0) 
				fCurIndex = max - 1;
		}
		if ((fCurIndex == startIndex)) 
			// we've gone completely through the list without finding
			// an good app. Oh well.
			return;


		if (!OKToUse((TTeamGroup *)fGroupList.ItemAt(fCurIndex)))
			continue;

		// if we're here then we found a good one
		SwitchToApp(startIndex, fCurIndex, forward);

		if (!activateNow) 
			break;

		if (ActivateApp(false, false))
			break;
	}
}


void
TSwitchMgr::SwitchToApp(int32 previousIndex, int32 newIndex, bool forward)
{
	int32 previousSlot = fCurSlot;

	fCurIndex = newIndex;
	fCurSlot = fWindow->SlotOf(fCurIndex);
	fCurWindow = 0;

	fWindow->Update(previousIndex, fCurIndex, previousSlot, fCurSlot, forward);
}


static int32
LowBitIndex(uint32 value)
{
	int32 result = 0;
	int32 bitMask = 1;

	if (value == 0)
		return -1;

	while (result < 32 && (value & bitMask) == 0) {
		result++;
		bitMask = bitMask << 1;
	}
	return result;
}


bool
TSwitchMgr::ActivateApp(bool forceShow, bool allowWorkspaceSwitch)
{

	// Let's get the info about the selected window. If it doesn't exist
	// anymore then get info about first window. If that doesn't exist then
	// do nothing.
	window_info	*windowInfo = WindowInfo(fCurIndex, fCurWindow);
	if (!windowInfo) {
		windowInfo = WindowInfo(fCurIndex, 0);
		if (!windowInfo)
			return false;
	}
	
	int32 currentWorkspace = current_workspace();
	TTeamGroup *teamGroup = (TTeamGroup *) fGroupList.ItemAt(fCurIndex);
	// Let's handle the easy case first: There's only 1 team in the group
	if (teamGroup->TeamList()->CountItems() == 1) {
		bool result;
		if (forceShow && (fCurWindow != 0 || windowInfo->is_mini))
			do_window_action(windowInfo->id, B_BRING_TO_FRONT,
				BRect(0, 0, 0, 0), false);
		
		if (!forceShow && windowInfo->is_mini)
			// we aren't unhiding minimized windows, so we can't do
			// anything here
			result = false;
		else if (!allowWorkspaceSwitch
			&& (windowInfo->workspaces & (1 << currentWorkspace)) == 0)
			// we're not supposed to switch workspaces so abort.
			result = false;
		else {
			result = true;
			be_roster->ActivateApp((team_id) teamGroup->TeamList()->ItemAt(0));
		}
		
		ASSERT(windowInfo);
		free(windowInfo);
		return result;
	}

	// Now the trickier case. We're trying to Bring to the Front a group
	// of teams. The current window (defined by fCurWindow) will define
	// which workspace we're going to. Then, once that is determined we
	// want to bring to the front every window of the group of teams that
	// lives in that workspace.
	
	if ((windowInfo->workspaces & (1 << currentWorkspace)) == 0) {
		if (!allowWorkspaceSwitch) {
			// If the first window in the list isn't in current workspace,
			// then none are. So we can't switch to this app.
			ASSERT(windowInfo);
			free(windowInfo);
			return false;
		}
		int32 dest_ws = LowBitIndex(windowInfo->workspaces);
		// now switch to that workspace
		activate_workspace(dest_ws);
	}

	if (!forceShow && windowInfo->is_mini) {
		// If the first window in the list is hiddenm then no windows in
		// this group are visible. So we can't switch to this app.
		ASSERT(windowInfo);
		free(windowInfo);
		return false;
	}

	int32 tokenCount;
	int32 *tokens = get_token_list(-1, &tokenCount);
	if (!tokens) {
		ASSERT(windowInfo);
		free(windowInfo);
		return true;	// weird error, so don't try to recover
	}

	BList windowsToActivate;

	// Now we go through all the windows in the current workspace list in order.
	// As we hit member teams we build the "activate" list.
	for (int32 i = 0; i < tokenCount; i++) {
		window_info	*matchWindowInfo = get_window_info(tokens[i]);
		if (!matchWindowInfo) 
			// That window probably closed. Just go to the next one.
			continue;
		if (!IsVisibleInCurrentWorkspace(matchWindowInfo)) {
			// first non-visible in workspace window means we're done.
			free(matchWindowInfo);
			break;
		}
		if ((matchWindowInfo->id != windowInfo->id)
			&& teamGroup->TeamList()->HasItem((void *)matchWindowInfo->team))
				windowsToActivate.AddItem((void *)matchWindowInfo->id);

		free(matchWindowInfo);
	}

	free(tokens);

	// Want to go through the list backwards to keep windows in same relative
	// order.
	int32 i = windowsToActivate.CountItems() - 1;
	for (; i >= 0; i--) {
		int32 wid = (int32) windowsToActivate.ItemAt(i);
		do_window_action(wid, B_BRING_TO_FRONT, BRect(0, 0, 0, 0), false);
	}

	// now bring the select window on top of everything.

	do_window_action(windowInfo->id, B_BRING_TO_FRONT, BRect(0, 0, 0, 0), false);
	free(windowInfo);
	return true;
}


window_info *
TSwitchMgr::WindowInfo(int32 groupIndex, int32 windowIndex)
{

	TTeamGroup *teamGroup = (TTeamGroup *) fGroupList.ItemAt(groupIndex);
	if (!teamGroup)
		return NULL;
	
	int32 tokenCount;
	int32 *tokens = get_token_list(-1, &tokenCount);
	if (!tokens)
		return NULL;
	
	int32 matches = 0;

	// Want to find the "windowIndex'th" window in window order that belongs
	// the the specified group (groupIndex). Since multiple teams can belong to
	// the same group (multiple-launch apps) we get the list of _every_
	// window and go from there.

	window_info	*result = NULL;
	for (int32 i = 0; i < tokenCount; i++) {
		window_info	*windowInfo = get_window_info(tokens[i]);
		if (windowInfo) {
			// skip hidden/special windows
			if (IsWindowOK(windowInfo)
				&& (teamGroup->TeamList()->HasItem((void *)windowInfo->team))) {
				// this window belongs to the team!
				if (matches == windowIndex) {
					// we found it!
					result = windowInfo;
					break;
				}
				matches++;
			}
			free(windowInfo);
		}
		// else - that window probably closed. Just go to the next one.
	}

	free(tokens);

	return result;
}


int32
TSwitchMgr::CountWindows(int32 groupIndex, bool )
{
	TTeamGroup *teamGroup = (TTeamGroup *)fGroupList.ItemAt(groupIndex);
	if (!teamGroup)
		return 0;
	
	int32 result = 0;

	for (int32 i = 0; ; i++) {
		team_id	teamID = (team_id)teamGroup->TeamList()->ItemAt(i);
		if (teamID == 0)
			break;
	
		int32 count;
		int32 *tokens = get_token_list(teamID, &count);
		if (!tokens)
			continue;

		for (int32 i = 0; i < count; i++) {
			window_info	*windowInfo = get_window_info(tokens[i]);
			if (windowInfo) {
				if (IsWindowOK(windowInfo))
					result++;
				free(windowInfo);
			}
		}
		free(tokens);
	}

	return result;
}


void
TSwitchMgr::ActivateWindow(int32 windowID)
{
	if (windowID == -1)
		windowID = fWindowID;

	do_window_action(windowID, B_BRING_TO_FRONT, BRect(0, 0, 0, 0), false);
}


void
TSwitchMgr::SwitchWindow(team_id team, bool, bool activate)
{
	// Find the _last_ window in the current workspace that belongs
	// to the group. This is the window to activate.

	int32 index;
	TTeamGroup*teamGroup = FindTeam(team, &index);

	// cycle through the window in the active application
	int32 count;
	int32 *tokens = get_token_list(-1, &count);
	for (int32 i = count-1; i>=0; i--) {
		window_info	*windowInfo;
		windowInfo = get_window_info(tokens[i]);
		if (windowInfo && IsVisibleInCurrentWorkspace(windowInfo)
			&& teamGroup->TeamList()->HasItem((void *)windowInfo->team)) {
				fWindowID = windowInfo->id;
				if (activate) 
					ActivateWindow(windowInfo->id);

				free(windowInfo);
				break;
		}
		free(windowInfo);
	}
	free(tokens);
}


void
TSwitchMgr::Unblock()
{
	fBlock = false;
	fSkipUntil = system_time();
}


int32
TSwitchMgr::CurIndex()
{
	return fCurIndex;
}


int32
TSwitchMgr::CurWindow()
{
	return fCurWindow;
}


int32
TSwitchMgr::CurSlot()
{
	return fCurSlot;
}


BList *
TSwitchMgr::GroupList()
{
	return &fGroupList;
}


bigtime_t
TSwitchMgr::IdleTime()
{
	return system_time() - fLastActivity;
}


//	#pragma mark -


TBox::TBox(BRect bounds, TSwitchMgr *mgr, TSwitcherWindow *window, TIconView *iview)
	:	BBox(bounds, "top", B_FOLLOW_NONE, B_WILL_DRAW, B_PLAIN_BORDER),
		fMgr(mgr),
		fWindow(window),
		fIconView(iview),
		fLeftScroller(false),
		fRightScroller(false)
{
}


void
TBox::AllAttached()
{
	BRect centerRect(kCenterSlot * kSlotSize, 0,
		(kCenterSlot + 1) * kSlotSize - 1, kSlotSize - 1);
	BRect frame = fIconView->Frame();

	// scroll the centerRect to correct location
	centerRect.OffsetBy(frame.left, frame.top);

	// switch to local coords
	fIconView->ConvertToParent(&centerRect);

	fCenter = centerRect;
}


void
TBox::MouseDown(BPoint where)
{
	if (!fLeftScroller && !fRightScroller && !fUpScroller && !fDownScroller)
		return;
	
	BRect frame = fIconView->Frame();
	BRect bounds = Bounds();

	if (fLeftScroller) {
		BRect lhit(0, frame.top, frame.left, frame.bottom);
		if (lhit.Contains(where)) {
			// Want to scroll by NUMSLOTS-1 slots
			int32 previousIndex = fMgr->CurIndex();
			int32 previousSlot = fMgr->CurSlot();
			int32 newSlot = previousSlot - (kNumSlots - 1);
			if (newSlot < 0)
				newSlot = 0;
			int32 newIndex = fIconView->IndexAt(newSlot);
			
			fMgr->SwitchToApp(previousIndex, newIndex, false);
		}
	}

	if (fRightScroller) {
		BRect rhit(frame.right, frame.top, bounds.right, frame.bottom);
		if (rhit.Contains(where)) {
			// Want to scroll by NUMSLOTS-1 slots
			int32 previousIndex = fMgr->CurIndex();
			int32 previousSlot = fMgr->CurSlot();
			int32 newSlot = previousSlot + (kNumSlots-1);
			int32 newIndex = fIconView->IndexAt(newSlot);
			
			if (newIndex < 0) {
				// don't have a page full to scroll
				int32 valid = fMgr->CountVisibleGroups();
				newIndex = fIconView->IndexAt(valid-1);
			}
			fMgr->SwitchToApp(previousIndex, newIndex, true);
		}
	}

	frame = fWindow->WindowView()->Frame();
	if (fUpScroller) {
		BRect hit1(frame.left - 10, frame.top, frame.left, (frame.top+frame.bottom)/2);
		BRect hit2(frame.right, frame.top, frame.right + 10, (frame.top+frame.bottom)/2);
		if (hit1.Contains(where) || hit2.Contains(where)) {
			// Want to scroll up 1 window
			fMgr->CycleWindow(false, false);
		}
	}

	if (fDownScroller) {
		BRect hit1(frame.left - 10, (frame.top+frame.bottom) / 2, frame.left, frame.bottom);
		BRect hit2(frame.right, (frame.top+frame.bottom) / 2, frame.right + 10, frame.bottom);
		if (hit1.Contains(where) || hit2.Contains(where)) {
			// Want to scroll down 1 window 
			fMgr->CycleWindow(true, false);
		}
	}
}


void
TBox::Draw(BRect update)
{
	static const int32 kChildInset = 7;
	static const int32 kWedge = 6;

	BBox::Draw(update);

	// The fancy border around the icon view

	BRect bounds = Bounds();
	float height = fIconView->Bounds().Height();
	float center = (bounds.right + bounds.left) / 2;

	BRect box(3, 3, bounds.right - 3, 3 + height + kChildInset * 2);
	rgb_color white = {255,255,255,255};
	rgb_color standardGray = {216, 216, 216, 255};
	rgb_color veryDarkGray = {128, 128, 128, 255};
	rgb_color darkGray = {184, 184, 184, 255};

	// Fill the area with dark gray
	SetHighColor(184,184,184);
	box.InsetBy(1,1);
	FillRect(box);

	box.InsetBy(-1,-1);

	BeginLineArray(50);

	// The main frame around the icon view
	AddLine(box.LeftTop(), BPoint(center-kWedge, box.top), veryDarkGray);
	AddLine(BPoint(center+kWedge, box.top), box.RightTop(), veryDarkGray);

	AddLine(box.LeftBottom(), BPoint(center-kWedge, box.bottom), veryDarkGray);
	AddLine(BPoint(center+kWedge, box.bottom), box.RightBottom(), veryDarkGray);
	AddLine(box.LeftBottom() + BPoint(1, 1),
		BPoint(center-kWedge, box.bottom + 1), white);
	AddLine(BPoint(center+kWedge, box.bottom) + BPoint(0, 1),
		box.RightBottom() + BPoint(1, 1), white);

	AddLine(box.LeftTop(), box.LeftBottom(), veryDarkGray);
	AddLine(box.RightTop(), box.RightBottom(), veryDarkGray);
	AddLine(box.RightTop() + BPoint(1, 1),
		box.RightBottom() + BPoint(1, 1), white);

	// downward pointing area at top of frame
	BPoint point(center - kWedge, box.top);
	AddLine(point, point + BPoint(kWedge, kWedge), veryDarkGray);
	AddLine(point + BPoint(kWedge, kWedge),
		BPoint(center+kWedge, point.y), veryDarkGray);

	AddLine(point + BPoint(1, 0),
		point + BPoint(1, 0) + BPoint(kWedge - 1, kWedge - 1), white);

	AddLine(point + BPoint(2, -1) + BPoint(kWedge - 1, kWedge - 1),
		BPoint(center+kWedge-1, point.y), darkGray);

	BPoint topPoint = point;

	// upward pointing area at bottom of frame
	point.y = box.bottom;
	point.x = center - kWedge;
	AddLine(point, point + BPoint(kWedge, -kWedge), veryDarkGray);
	AddLine(point + BPoint(kWedge, -kWedge),
		BPoint(center+kWedge, point.y), veryDarkGray);

	AddLine(point + BPoint(1, 0),
		point + BPoint(1, 0) + BPoint(kWedge - 1, -(kWedge - 1)), white);

	AddLine(point + BPoint(2 , 1) + BPoint(kWedge - 1, -(kWedge - 1)),
		BPoint(center + kWedge - 1, point.y), darkGray);

	BPoint bottomPoint = point;

	EndLineArray();

	// fill the downward pointing arrow area
	SetHighColor(standardGray);
	FillTriangle(topPoint + BPoint(2, 0),
		topPoint + BPoint(2, 0) + BPoint(kWedge - 2, kWedge - 2),
		BPoint(center + kWedge - 2, topPoint.y));

	// fill the upward pointing arrow area
	SetHighColor(standardGray);
	FillTriangle(bottomPoint + BPoint(2,0),
		bottomPoint + BPoint(2, 0) + BPoint(kWedge - 2, -(kWedge - 2)),
		BPoint(center + kWedge - 2, bottomPoint.y));

	DrawIconScrollers(false);
	DrawWindowScrollers(false);

}


void
TBox::DrawIconScrollers(bool force)
{
	bool updateLeft = false;
	bool updateRight = false;
	rgb_color leftc;
	rgb_color rightc;
	rgb_color bkg = {184, 184, 184, 255};
	rgb_color dark = {0, 96, 96, 255};

	BRect rect = fIconView->Bounds();
	if (rect.left > (kSlotSize * kCenterSlot)) {
		updateLeft = true;
		fLeftScroller = true;
		leftc = dark;
	} else {
		fLeftScroller = false;
		if (force) {
			updateLeft = true;
			leftc = bkg;
		}
	}

	int32 maxIndex = fMgr->GroupList()->CountItems() - 1;
			// last_frame is in fIconView coordinate space
	BRect lastFrame = fIconView->FrameOf(maxIndex);

	if (lastFrame.right > rect.right) {
		updateRight = true;
		fRightScroller = true;
		rightc = dark;
	} else {
		fRightScroller = false;
		if (force) {
			updateRight = true;
			rightc = bkg;
		}
	}

	rect = fIconView->Frame();
	if (updateLeft) {
		SetHighColor(leftc);
		BPoint	pt1, pt2, pt3;
		pt1.x = rect.left - 5;
		pt1.y = floorf((rect.bottom + rect.top) / 2);
		pt2.x = pt3.x = pt1.x + 3;
		pt2.y = pt1.y - 3;
		pt3.y = pt1.y + 3;
		FillTriangle(pt1, pt2, pt3);
	}
	if (updateRight) {
		SetHighColor(rightc);
		BPoint	pt1, pt2, pt3;
		pt1.x = rect.right + 4;
		pt1.y = rintf((rect.bottom + rect.top) / 2);
		pt2.x = pt3.x = pt1.x - 4;
		pt2.y = pt1.y - 4;
		pt3.y = pt1.y + 4;
		FillTriangle(pt1, pt2, pt3);
	}
}


void
TBox::DrawWindowScrollers(bool force)
{
	bool updateUp = false;
	bool updateDown = false;
	rgb_color upColor;
	rgb_color downColor;
	rgb_color bkg = {216,216,216,255};
	rgb_color dark = {96,96,96,255};

	BRect rect = fWindow->WindowView()->Bounds();
	if (rect.top != 0) {
		updateUp = true;
		fUpScroller = true;
		upColor = dark;
	} else {
		fUpScroller = false;
		if (force) {
			updateUp = true;
			upColor = bkg;
		}
	}

	int32 groupIndex = fMgr->CurIndex();
	int32 maxIndex = fMgr->CountWindows(groupIndex) - 1;
	
	BRect lastFrame(0, 0, 0, 0);
	if (maxIndex >= 0) {
		lastFrame = fWindow->WindowView()->FrameOf(maxIndex);
	}
	if (maxIndex >= 0 && lastFrame.bottom > rect.bottom) {
		updateDown = true;
		fDownScroller = true;
		downColor = dark;
	} else {
		fDownScroller = false;
		if (force) {
			updateDown = true;
			downColor = bkg;
		}
	}

	rect = fWindow->WindowView()->Frame();
	rect.InsetBy(-3, 0);
	if (updateUp) {
		SetHighColor(upColor);
		BPoint	pt1, pt2, pt3;
		pt1.x = rect.left - 6;
		pt1.y = rect.top + 3;
		pt2.y = pt3.y = pt1.y + 4;
		pt2.x = pt1.x - 4;
		pt3.x = pt1.x + 4;
		FillTriangle(pt1, pt2, pt3);

		pt1.x += rect.Width() + 12;
		pt2.x += rect.Width() + 12;
		pt3.x += rect.Width() + 12;
		FillTriangle(pt1, pt2, pt3);
	}
	if (updateDown) {
		SetHighColor(downColor);
		BPoint	pt1, pt2, pt3;
		pt1.x = rect.left - 6;
		pt1.y = rect.bottom - 3;
		pt2.y = pt3.y = pt1.y - 4;
		pt2.x = pt1.x - 4;
		pt3.x = pt1.x + 4;
		FillTriangle(pt1, pt2, pt3);

		pt1.x += rect.Width() + 12;
		pt2.x += rect.Width() + 12;
		pt3.x += rect.Width() + 12;
		FillTriangle(pt1, pt2, pt3);

	}
	Sync();
}


//	#pragma mark -


TSwitcherWindow::TSwitcherWindow(BRect frame, TSwitchMgr *mgr)
	:	BWindow(frame, "Twitcher", B_MODAL_WINDOW_LOOK,
			B_MODAL_ALL_WINDOW_FEEL,
			B_NOT_MINIMIZABLE | B_NOT_ZOOMABLE | B_NOT_RESIZABLE, B_ALL_WORKSPACES),
		fMgr(mgr),
		fHairTrigger(true)
{
	BRect rect = frame;
	rect.OffsetTo(B_ORIGIN);
	rect.InsetBy(kHorizontalMargin, 0);
	rect.top = kVerticalMargin;
	rect.bottom = rect.top + kSlotSize - 1;

	fIconView = new TIconView(rect, mgr, this);

	rect.top = rect.bottom + (kVerticalMargin * 1 + 4);
	rect.InsetBy(9, 0);

	fWindowView = new TWindowView(rect, mgr, this);
	fWindowView->ResizeToPreferred();

	fTopView = new TBox(Bounds(), fMgr, this, fIconView);
	AddChild(fTopView);

	SetPulseRate(0);
	fTopView->AddChild(fIconView);
	fTopView->AddChild(fWindowView);
}


TSwitcherWindow::~TSwitcherWindow()
{
}



void
TSwitcherWindow::DispatchMessage(BMessage *message, BHandler *handler)
{
	if (message->what == B_KEY_DOWN) {
		// large timeout - effective kills key repeats
		if (fMgr->IdleTime() < 10000000) {
			return;
		}
		fMgr->Touch();
	} else if (message->what == B_KEY_UP) 
		fMgr->Touch(true);

	BWindow::DispatchMessage(message, handler);
}


void
TSwitcherWindow::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case B_KEY_DOWN:
			uint32 ch;
			uint32 modifiers;
			message->FindInt32("raw_char", 0, (int32 *)&ch);
			message->FindInt32("modifiers", 0, (int32 *)&modifiers);
			DoKey(ch, modifiers);
			break;

		default:
			BWindow::MessageReceived(message);
	}
}


void
TSwitcherWindow::Redraw(int32 index)
{
	BRect frame = fIconView->FrameOf(index);
	frame.right = fIconView->Bounds().right;
	fIconView->Invalidate(frame);
}


void
TSwitcherWindow::DoKey(uint32 key, uint32 modifiers)
{
	bool forward = ((modifiers & B_SHIFT_KEY) == 0);

	switch (key) {
		case '~': 
			fMgr->CycleWindow(false, false);
			break;

		case '`': 
			fMgr->CycleWindow(true, false);
			break;

#if _ALLOW_STICKY_
		case 's': 
		case 'S':
			if (fHairTrigger) {
				SetLook(B_TITLED_WINDOW_LOOK);
				fHairTrigger = false;
			} else {
				SetLook(B_MODAL_WINDOW_LOOK);
				fHairTrigger = true;
			}
			break;
#endif
		case B_RIGHT_ARROW: 
			fMgr->CycleApp(true, false);
			break;

		case B_LEFT_ARROW: 
			fMgr->CycleApp(false, false);
			break;

		case B_UP_ARROW: 
			fMgr->CycleWindow(false, false);
			break;

		case B_DOWN_ARROW: 
			fMgr->CycleWindow(true, false);
			break;

		case B_TAB: 
			fMgr->CycleApp(forward, false);
			break;

		case B_ESCAPE: 
			fMgr->Stop(false, 0);
			break;

		case B_SPACE:
		case B_ENTER: 
			fMgr->Stop(true, modifiers);
			break;

	}
}


bool
TSwitcherWindow::QuitRequested()
{
	((TBarApp *) be_app)->Settings()->switcherLoc = Frame().LeftTop();
	fMgr->Stop(false, 0);
	return false;
}


void
TSwitcherWindow::WindowActivated(bool state)
{
	if (state)
		fMgr->Unblock();
}


void
TSwitcherWindow::Update(int32 prev, int32 cur, int32 previousSlot,
	int32 currentSlot, bool forward)
{
	if (!IsHidden()) 
		fIconView->Update(prev, cur, previousSlot, currentSlot, forward);
	else 
		fIconView->CenterOn(cur);

	fWindowView->UpdateGroup(cur, 0);
}


void
TSwitcherWindow::Hide()
{
	fIconView->Hiding();
	SetPulseRate(0);
	BWindow::Hide();
}


void
TSwitcherWindow::Show()
{
	fHairTrigger = true;
	fIconView->Showing();
	SetPulseRate(100000);
	fMgr->Touch();
	SetLook(B_MODAL_WINDOW_LOOK);
	BWindow::Show();
}


TBox *
TSwitcherWindow::TopView()
{
	return fTopView;
}


bool
TSwitcherWindow::HairTrigger()
{
	return fHairTrigger;
}


//	#pragma mark -


TIconView::TIconView(BRect frame, TSwitchMgr *mgr, TSwitcherWindow *switcherWindow)
	:	BView(frame, "main_view", B_FOLLOW_NONE,
			B_WILL_DRAW | B_PULSE_NEEDED),
		fAutoScrolling(false),
		fSwitcher(switcherWindow),
		fMgr(mgr)
{
	BRect rect(0, 0, kSlotSize - 1, kSlotSize - 1);

	fOffView = new BView(rect, "off_view", B_FOLLOW_NONE, B_WILL_DRAW);
	fOffView->SetHighColor(184, 184, 184);
	fOffBitmap = new BBitmap(rect, B_COLOR_8_BIT, true);
	fOffBitmap->AddChild(fOffView);

	fCurSmall = new BBitmap(BRect(0, 0, 15, 15), B_COLOR_8_BIT);
	fCurLarge = new BBitmap(BRect(0, 0, 31, 31), B_COLOR_8_BIT);
	
	SetViewColor(184, 184, 184);
	SetLowColor(184, 184, 184);
}


TIconView::~TIconView()
{
	delete fCurSmall;
	delete fCurLarge;
	delete fOffBitmap;
}


void
TIconView::KeyDown(const char *, int32)
{
}


void
TIconView::CacheIcons(TTeamGroup *teamGroup)
{
	const BBitmap *bitmap = teamGroup->SmallIcon();
	ASSERT(bitmap);
	fCurSmall->SetBits(bitmap->Bits(), bitmap->BitsLength(), 0,
		B_COLOR_8_BIT);

	bitmap = teamGroup->LargeIcon();
	ASSERT(bitmap);
	fCurLarge->SetBits(bitmap->Bits(), bitmap->BitsLength(), 0,
		B_COLOR_8_BIT);
}


void
TIconView::AnimateIcon(BBitmap *startIcon, BBitmap *endIcon)
{
	BRect centerRect(kCenterSlot*kSlotSize, 0,
		(kCenterSlot + 1) * kSlotSize - 1, kSlotSize - 1);
	BRect startIconBounds = startIcon->Bounds();
	BRect bounds = Bounds();
	float width = startIconBounds.Width();
	int32 amount = (width < 20) ? -2 : 2;


	// center the starting icon inside of centerRect
	float off = (centerRect.Width() - width) / 2;
	startIconBounds.OffsetTo(BPoint(off,off));

	// scroll the centerRect to correct location
	centerRect.OffsetBy(bounds.left, 0);

	BRect destRect = fOffBitmap->Bounds();
	// scroll to the centerRect location
	destRect.OffsetTo(centerRect.left, 0);
	// center the destRect inside of centerRect.
	off = (centerRect.Width() - destRect.Width()) / 2;
	destRect.OffsetBy(BPoint(off,off));

	fOffBitmap->Lock();
	fOffView->SetDrawingMode(B_OP_OVER);
	for (int i = 0; i < 2; i++) {
		startIconBounds.InsetBy(amount,amount);
		snooze(20000);
		fOffView->FillRect(fOffView->Bounds());
		fOffView->DrawBitmap(startIcon, startIconBounds);
		fOffView->Sync();
		DrawBitmap(fOffBitmap, destRect);
	}
	for (int i = 0; i < 2; i++) {
		startIconBounds.InsetBy(amount,amount);
		snooze(20000);
		fOffView->FillRect(fOffView->Bounds());
		fOffView->DrawBitmap(endIcon, startIconBounds);
		fOffView->Sync();
		DrawBitmap(fOffBitmap, destRect);
	}

	fOffView->SetDrawingMode(B_OP_COPY);
	fOffBitmap->Unlock();
}


void
TIconView::Update(int32, int32 cur, int32 previousSlot, int32 currentSlot,
	bool forward)
{
	// Animate the shrinking of the currently centered icon.
	AnimateIcon(fCurLarge, fCurSmall);
	
	int32 nslots = abs(previousSlot - currentSlot);
	int32 stepSize = kScrollStep;

	if (forward && (currentSlot < previousSlot)) {
		// we were at the end of the list and we just moved to the start
		forward = false;
		if (previousSlot - currentSlot > 4)
			stepSize = stepSize*2;

	} else if (!forward && (currentSlot > previousSlot)) {
		// we're are moving backwards and we just hit start of list and
		// we wrapped to the end.
		forward = true;
		if (currentSlot - previousSlot > 4)
			stepSize = stepSize*2;
	}

	int32 scrollValue = forward ? stepSize : -stepSize;
	int32 total = 0;

	fAutoScrolling = true;
	while (total < (nslots * kSlotSize)) {
		ScrollBy(scrollValue, 0);
		snooze(1000);
		total += stepSize;
		Window()->UpdateIfNeeded();
	}
	fAutoScrolling = false;

	TTeamGroup *teamGroup = (TTeamGroup *)fMgr->GroupList()->ItemAt(cur);
	ASSERT(teamGroup);
	CacheIcons(teamGroup);

	// Animate the expansion of the currently centered icon
	AnimateIcon(fCurSmall, fCurLarge);
}


void
TIconView::CenterOn(int32 index)
{
	BRect rect = FrameOf(index);
	ScrollTo(rect.left - (kCenterSlot * kSlotSize), 0);
}


int32
TIconView::ItemAtPoint(BPoint point) const
{
	float tmpPointVerticalIndex = (point.x / kSlotSize) - kCenterSlot;
	if (tmpPointVerticalIndex < 0)
		return -1;

	int32 pointVerticalIndex = (int32)tmpPointVerticalIndex;
	
	for (int32 i = 0, verticalIndex = 0; ; i++) {

		TTeamGroup *teamGroup = (TTeamGroup *)fMgr->GroupList()->ItemAt(i);
		if (teamGroup == NULL)
			break;
		
		if (!OKToUse(teamGroup))
			continue;

		if (verticalIndex == pointVerticalIndex) 
			return i;

		verticalIndex++;
	}
	return -1;
}


void
TIconView::ScrollTo(BPoint where)
{
	BView::ScrollTo(where);
	fSwitcher->TopView()->DrawIconScrollers(true);
}


int32
TIconView::IndexAt(int32 slot) const
{
	BList *list = fMgr->GroupList();
	int32 count = list->CountItems();
	int32 slotIndex = 0;

	for (int32 i = 0; i < count; i++) {
		TTeamGroup *teamGroup = (TTeamGroup *)list->ItemAt(i);

		if (!OKToUse(teamGroup))
			continue;

		if (slotIndex == slot) {
			return i;
		}
		slotIndex++;
	}
	return -1;
}


int32
TIconView::SlotOf(int32 index) const
{
	BRect rect = FrameOf(index);
	return (int32)(rect.left / kSlotSize) - kCenterSlot;
}


BRect
TIconView::FrameOf(int32 index) const 
{
	BList *list = fMgr->GroupList();
	int32 visi = kCenterSlot - 1;
		// first few slots in view are empty

	TTeamGroup *teamGroup;
	for (int32 i = 0; i <= index; i++) {
		teamGroup = (TTeamGroup *) list->ItemAt(i);

		if (!OKToUse(teamGroup))
			continue;

		visi++;
	}

	return BRect(visi * kSlotSize, 0, (visi + 1) * kSlotSize - 1, kSlotSize - 1);
}


void
TIconView::DrawTeams(BRect update)
{
	int32 mainIndex = fMgr->CurIndex();
	BList *list = fMgr->GroupList();
	int32 count = list->CountItems();

	BRect rect(kCenterSlot * kSlotSize, 0,
		(kCenterSlot + 1) * kSlotSize - 1, kSlotSize - 1);

	for (int32 i = 0; i < count; i++) {
		TTeamGroup *teamGroup = (TTeamGroup *) list->ItemAt(i);

		if (!OKToUse(teamGroup))
			continue;

		if (rect.Intersects(update) && teamGroup) {
			SetDrawingMode(B_OP_OVER);
			
			teamGroup->Draw(this, rect, !fAutoScrolling && (i == mainIndex));

			if (i == mainIndex) 
				CacheIcons(teamGroup);

			SetDrawingMode(B_OP_COPY);
		}
		rect.OffsetBy(kSlotSize,0);
	}
}


void
TIconView::Draw(BRect update)
{
	DrawTeams(update);
}


void
TIconView::MouseDown(BPoint where)
{
	int32 index = ItemAtPoint(where);
	if (index >= 0) {
		int32 previousIndex = fMgr->CurIndex();
		int32 previousSlot = fMgr->CurSlot();
		int32 currentSlot = SlotOf(index);
		fMgr->SwitchToApp(previousIndex, index, (currentSlot > previousSlot));
	}
}


void
TIconView::Pulse()
{
	uint32 modifiersKeys = modifiers();
	if (fSwitcher->HairTrigger() && (modifiersKeys & B_CONTROL_KEY) == 0) {
		fMgr->Stop(true, modifiersKeys);
		return;
	}
	
	if (!fSwitcher->HairTrigger()) {
		uint32 buttons;
		BPoint point;
		GetMouse(&point, &buttons);
		if (buttons != 0) {
			point = ConvertToScreen(point);
			if (!Window()->Frame().Contains(point))
				fMgr->Stop(false, 0);
		}
	}
}


void
TIconView::Showing()
{
}


void
TIconView::Hiding()
{
	ScrollTo(B_ORIGIN);
}


//	#pragma mark -


TWindowView::TWindowView(BRect rect, TSwitchMgr *mgr, TSwitcherWindow *window)
	:	BView(rect, "wlist_view", B_FOLLOW_NONE, B_WILL_DRAW | B_PULSE_NEEDED),
			fCurToken(-1),
			fSwitcher(window),
			fMgr(mgr)
{
	SetViewColor(216,216,216);
	SetFont(be_plain_font);
}


void
TWindowView::ScrollTo(BPoint where)
{
	BView::ScrollTo(where);
	fSwitcher->TopView()->DrawWindowScrollers(true);
}



BRect
TWindowView::FrameOf(int32 index) const
{
	return BRect(0, index * fItemHeight, 100, ((index + 1) * fItemHeight) - 1);
}


const int32 kWindowScrollSteps = 3;

void
TWindowView::GetPreferredSize(float *w, float *h)
{
	font_height	fh;
	be_plain_font->GetHeight(&fh);
	fItemHeight = (int32) fh.ascent + fh.descent;

	// top & bottom margin
	fItemHeight = fItemHeight + 3 + 3;

	// want fItemHeight to be divisible by kWindowScrollSteps.
	fItemHeight = ((((int) fItemHeight) + kWindowScrollSteps) / kWindowScrollSteps) *
		kWindowScrollSteps;

	*h = fItemHeight;

	// leave width alone
	*w = Bounds().Width();
}


void
TWindowView::ShowIndex(int32 newIndex)
{
	// convert index to scroll location
	BPoint point(0, newIndex * fItemHeight);
	BRect bounds = Bounds();
	
	int32 groupIndex = fMgr->CurIndex();
	TTeamGroup *teamGroup = (TTeamGroup *)fMgr->GroupList()->ItemAt(groupIndex);
	if (!teamGroup)
		return;

	window_info	*windowInfo = fMgr->WindowInfo(groupIndex, newIndex);
	if (windowInfo == NULL)
		return;

	fCurToken = windowInfo->id;
	free(windowInfo);

	if (bounds.top == point.y)
		return;

	int32 oldIndex = (int32) (bounds.top / fItemHeight);

	int32 stepSize = (int32) (fItemHeight / kWindowScrollSteps);
	int32 scrollValue = (newIndex > oldIndex) ? stepSize : -stepSize;
	int32 total = 0;
	int32 nslots = abs(newIndex - oldIndex);

	while (total < (nslots * (int32)fItemHeight)) {
		ScrollBy(0, scrollValue);
		snooze(10000);
		total += stepSize;
		Window()->UpdateIfNeeded();
	}
}


void
TWindowView::Draw(BRect update)
{
	int32 groupIndex = fMgr->CurIndex();
	TTeamGroup *teamGroup = (TTeamGroup *) fMgr->GroupList()->ItemAt(groupIndex);
	if (!teamGroup)
		return;
	
	BRect bounds = Bounds();
	int32 windowIndex = (int32) (bounds.top / fItemHeight);
	BRect windowRect = bounds;

	windowRect.top = windowIndex * fItemHeight;
	windowRect.bottom = ((windowIndex+1) * fItemHeight) - 1;

	for (int32 i = 0; i < 3; i++) {
		if (!update.Intersects(windowRect)) {
			windowIndex++;
			windowRect.OffsetBy(0, fItemHeight);
			continue;
		}
		

		bool local = true;					// is window in cur workspace?
		bool minimized = false;
		BString title;

		window_info	*windowInfo = fMgr->WindowInfo(groupIndex, windowIndex);
		if (windowInfo != NULL) {
			
			if (SmartStrcmp(windowInfo->name, teamGroup->Name()) != 0)
				title << teamGroup->Name() << ": " << windowInfo->name;
			else 
				title = teamGroup->Name();
			
			int32 currentWorkspace = current_workspace();
			if ((windowInfo->workspaces & (1 << currentWorkspace)) == 0)
				local = false;

			minimized = windowInfo->is_mini;
			free(windowInfo);
		} else 
			title = teamGroup->Name();

		if (!title.Length())
			return;

		float stringWidth = StringWidth(title.String());
		float maxWidth = bounds.Width() - (14 + 5);

		if (stringWidth > maxWidth) {
			// window title is too long, need to truncate
			TruncateString(&title, B_TRUNCATE_END, maxWidth);
			stringWidth = maxWidth;
		}

		BPoint point((bounds.Width() - (stringWidth + 14 + 5)) / 2, windowRect.bottom - 4);
		BPoint p(point.x, (windowRect.top + windowRect.bottom) / 2);
		SetDrawingMode(B_OP_OVER);
		const BBitmap *bitmap = AppResSet()->FindBitmap(B_MESSAGE_TYPE,
			minimized ? R_WindowHiddenIcon : R_WindowShownIcon);
		p.y -= (bitmap->Bounds().bottom - bitmap->Bounds().top) / 2;
		DrawBitmap(bitmap, p);

		if (!local) {
			SetHighColor(96, 96, 96);
			p.x -= 8;
			p.y += 4;
			StrokeLine(p + BPoint(2, 2), p + BPoint(2, 2));
			StrokeLine(p + BPoint(4, 2), p + BPoint(6, 2));

			StrokeLine(p + BPoint(0, 5), p + BPoint(0, 5));
			StrokeLine(p + BPoint(2, 5), p + BPoint(6, 5));

			StrokeLine(p + BPoint(1, 8), p + BPoint(1, 8));
			StrokeLine(p + BPoint(3, 8), p + BPoint(6, 8));

			SetHighColor(0, 0, 0);
		}

		point.x += 21;
		MovePenTo(point);

		DrawString(title.String());
		SetDrawingMode(B_OP_COPY);

		windowIndex++;
		windowRect.OffsetBy(0, fItemHeight);
	}
}


void
TWindowView::UpdateGroup(int32 , int32 windowIndex)
{
	ScrollTo(0, windowIndex * fItemHeight);
	Invalidate(Bounds());
}


void
TWindowView::Pulse()
{
	// If selected window went away then reset to first window
	window_info	*windowInfo = get_window_info(fCurToken);
	if (windowInfo == NULL) {
		Invalidate();
		ShowIndex(0);
	} else
		free(windowInfo);
}




bool
IsKeyDown(int32 key)
{
	key_info keyInfo;

	get_key_info(&keyInfo);
	return (keyInfo.key_states[key >> 3] & (1 << ((7 - key) & 7))) != 0;
}


bool
IsWindowOK(const window_info *windowInfo)
{
	// is_mini (true means that the window is minimized).
	// if not, then 
	// show_hide >= 1 means that the window is hidden.
	//
	// If the window is both minimized and hidden, then you get :
	//	 TWindow->is_mini = false;
	//	 TWindow->was_mini = true;
	//	 TWindow->show_hide >= 1;

	if (windowInfo->w_type != _STD_W_TYPE_)
		return false;

	if (windowInfo->is_mini)
		return true;
	
	return windowInfo->show_hide_level <= 0;
}


bool
OKToUse(const TTeamGroup *teamGroup)
{
	if (!teamGroup)
		return false;

	// skip background applications
	if ((teamGroup->Flags() & B_BACKGROUND_APP) != 0)
		return false;

	// skip the Deakbar itself
	if (strcasecmp(teamGroup->Sig(), TASK_BAR_MIME_SIG) == 0)
		return false;

	return true;
}


int
SmartStrcmp(const char *s1, const char *s2)
{
	if (strcasecmp(s1, s2) == 0)
		return 0;
		
	// if the strings on differ in spaces or underscores they still match
	while (*s1 && *s2) {
		if ((*s1 == ' ') || (*s1 == '_')) {
			s1++;
			continue;
		}
		if ((*s2 == ' ') || (*s2 == '_')) {
			s2++;
			continue;
		}
		if (*s1 != *s2)
			return 1;		// they differ
		s1++;
		s2++;
	}

	// if one of the strings ended before the other
	// ??? could process trailing spaces & underscores!
	if (*s1)
		return 1;
	if (*s2)
		return 1;

	return 0;
}




