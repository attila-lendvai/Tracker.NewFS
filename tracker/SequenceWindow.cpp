#include "SequenceWindow.h"
#include "IconCache.h"
#include "LanguageTheme.h"
#include "Tracker.h"
#include <ScrollView.h>
#include <Screen.h>

const int32 kAccept = 'acpt';
const char *sequence_strings[6] = { "Use scalable icon", "Use bitmap icon with exact size", "Use default icon with exact size", "Scale down bigger bitmap icon", "Scale up smaller bitmap icon", "Fallback to default icon" };

SequenceWindow::SequenceWindow(BRect frame)
	:	BWindow(frame, LOCALE("Edit Icon Lookup Sequence"), B_TITLED_WINDOW, B_NOT_MINIMIZABLE | B_NOT_RESIZABLE | B_NO_WORKSPACE_ACTIVATION | B_NOT_ANCHORED_ON_ACTIVATE | B_ASYNCHRONOUS_CONTROLS | B_NOT_ZOOMABLE)
{
	BView *back_view = new BView(Bounds(), "Background", B_FOLLOW_ALL_SIDES, 0);
	back_view->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	AddChild(back_view);
	
	float borderspacing = 5, itemspacing = 4;
	BRect frame = Bounds();
	frame.InsetBy(borderspacing, borderspacing);
	
	float button_width, button_height;
	BButton test(frame, "test", "test", NULL);
	test.GetPreferredSize(&button_width, &button_height);
	
	frame.InsetBy(2, 2);
	fSequence = new BListView(frame, "fSequence", B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL);	fSequence->ResizeTo(Bounds().Width() - borderspacing * 2, Bounds().Height() - button_height - itemspacing * 2 - borderspacing);
	fSequence->ResizeTo(frame.Width() - B_V_SCROLL_BAR_WIDTH, fSequence->Bounds().Height());
	BScrollView *scroll = new BScrollView("scrollview", fSequence, B_FOLLOW_ALL_SIDES, B_WILL_DRAW, false, true);
	back_view->AddChild(scroll);
	frame.InsetBy(-2, -2);
	frame.OffsetTo(frame.left, fSequence->Frame().bottom + itemspacing * 2);
	
	fMoveDown = new BButton(frame, "fMoveDown", LOCALE("Move Down"), new BMessage(kSequenceMovedDown), B_FOLLOW_LEFT | B_FOLLOW_BOTTOM);
	fMoveDown->ResizeToPreferred();
	back_view->AddChild(fMoveDown);
	frame.OffsetBy(fMoveDown->Bounds().Width() + itemspacing, 0);
	
	fMoveUp = new BButton(frame, "fMoveUp", LOCALE("Move Up"), new BMessage(kSequenceMovedUp), B_FOLLOW_LEFT | B_FOLLOW_BOTTOM);
	fMoveUp->ResizeToPreferred();
	back_view->AddChild(fMoveUp);
	
	fAccept = new BButton(frame, "fAccept", LOCALE("Accept"), new BMessage(kAccept), B_FOLLOW_RIGHT | B_FOLLOW_BOTTOM);
	fAccept->ResizeToPreferred();
	fAccept->MoveTo(Bounds().Width() - borderspacing - fAccept->Bounds().Width(), frame.top);
	back_view->AddChild(fAccept);
	
	// fill in the strings
	const char *sequence = gTrackerSettings.IconThemeLookupSequence();
	if (strlen(sequence) != 6) {
		gTrackerSettings.SetIconThemeLookupSequence("badecf");
		sequence = gTrackerSettings.IconThemeLookupSequence();
	}
	
	float scroll_width = 0;
	for (int i = 0; i < 6; i++) {
		uint32 index = sequence[i] - 'a';
		if (index >= 6)
			continue;
		
		const char *string = LOCALE(sequence_strings[index]);
		fSequence->AddItem(new BStringItem(string));
		scroll_width = MAX(scroll_width, fSequence->StringWidth(string));
	}
	
	scroll_width = MAX(scroll_width + 2 * B_V_SCROLL_BAR_WIDTH + 3 * itemspacing, fSequence->Bounds().Width());
	button_width = fMoveDown->Bounds().Width() + fMoveUp->Bounds().Width() + fAccept->Bounds().Width() + 4 * itemspacing;
	float min_width = MAX(scroll_width, button_width);
	if (min_width > frame.Width())
		ResizeTo(min_width, frame.Height());
	
	// set the targets
	fSequence->SetTarget(this);
	fMoveUp->SetTarget(this);
	fMoveDown->SetTarget(this);
	fAccept->SetTarget(this);
}

void
SequenceWindow::MoveCloseToMouse()
{
	uint32 buttons;
	BPoint mousePosition;
	
	ChildAt((int32)0)->GetMouse(&mousePosition, &buttons);
	ConvertToScreen(&mousePosition);
	
	// Position the window centered around the mouse...
	BPoint windowPosition = BPoint(mousePosition.x - Frame().Width() / 2,
		mousePosition.y	- Frame().Height() / 2);
	
	// ... unless that's outside of the current screen size:
	BScreen screen;
	windowPosition.x = MAX(0, MIN(screen.Frame().right - Frame().Width(),
		windowPosition.x));
	windowPosition.y = MAX(0, MIN(screen.Frame().bottom - Frame().Height(),
		windowPosition.y));
	
	MoveTo(windowPosition);
}

void
SequenceWindow::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case kSequenceMovedDown:
		case kSequenceMovedUp: {
			int32 selection = fSequence->CurrentSelection();
			fSequence->SwapItems(selection, selection + (message->what == kSequenceMovedUp ? -1 : 1));
			
		} break;
		
		case kAccept: {
			BStringItem *item = NULL;
			char new_sequence[7];
			memset(new_sequence, 0, 7);
			int index = 0;
			
			for (int i = 0; i < fSequence->CountItems(); i++) {
				item = (BStringItem *)fSequence->ItemAt(i);
				if (!item)
					continue;
				
				for (int j = 0; j < 6; j++) {
					if (BString(item->Text()).Compare(LOCALE(sequence_strings[j])) == 0) {
						new_sequence[index++] = 'a' + j;
						break;
					}
				}
			}
			
			gTrackerSettings.SetIconThemeLookupSequence(new_sequence);
			GetIconTheme()->RefreshTheme();
			Close();
		} break;
		
		default: BWindow::MessageReceived(message); break;
	}
}
