// Open Tracker License
//
// Terms and Conditions
//
// Copyright (c) 1991-2000, Be Incorporated. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
// 
// The above copyright notice and this permission notice applies to all licensees
// and shall be included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
// AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of Be Incorporated shall not be
// used in advertising or otherwise to promote the sale, use or other dealings in
// this Software without prior written authorization from Be Incorporated.
// 
// Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
// of Be Incorporated in the United States and other countries. Other brand product
// names are registered trademarks or trademarks of their respective holders.
// All rights reserved.

#include "TFSContext.h"
#include "Attributes.h"
#include "Commands.h"
#include "Tracker.h"
#include "Bitmaps.h"
#include "FSStatusWindow.h"

#include <Autolock.h>
#include <Screen.h>
#include <fs_attr.h>
#include <Roster.h>
#include <Alert.h>
#include <Beep.h>
#include <errno.h>
#include <math.h>
#include <Dragger.h>

#define FS_PRINT_COPY_SPEED		1		// at the end of each file print MB/sec copy speed (valid only if not paused, just for testing)

namespace BPrivate {

//static const bigtime_t	kPopUpDelay is in the header
static const bigtime_t	kRefreshRate					= 250000;	// should be in sync with FSContext.cpp
static const bigtime_t	kDetailsUpdateDelay				= 1000000;	// delay between updating details

static const float		kDetailsXOffsetFromRight			= 120;

static const float		kProgressViewXOffsetFromLeft	= 60;
static const float		kProgressViewMinimalWidth		= 200;
static const float		kButtonHeightMultiplier			= 2.;		// font height * kButtonHeightMultiplier

static const float		kStatusBarThickness				= 12;

static const float		kStatusViewMinimalWidth			= kProgressViewMinimalWidth + kProgressViewXOffsetFromLeft + 100;
static const float		kStatusViewPreferredWidth		= (300 > kStatusViewMinimalWidth) ? 300 : kStatusViewMinimalWidth;
static const float		kStatusViewPreferredHeight		= kStatusBarThickness * 3 * 1.5;

static const rgb_color	kStatusBarCopyColor				= {100, 200, 100, 0};
static const rgb_color	kStatusBarOtherColor			= {100, 100, 200, 0};

#define					kStringLowColor					  tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 1.05)
static const rgb_color	kStringHighColor				= {0, 0, 0, 0};

static const uint32		kSkipDirectoryKey				= B_CONTROL_KEY;
static const uint32		kSkipEntryKey					= B_SHIFT_KEY;

FSStatusWindow::ContainerView *
					FSStatusWindow::ContainerView				:: sReplicantInstance = 0;
FSStatusWindow *	FSStatusWindow								:: sSelf;
int32				FSStatusWindow								:: sInitialized = 0;
float				FSStatusWindow::StatusView					:: sFontHeight;
font_height			FSStatusWindow::StatusView					:: sFontHeightStruct;
bool				FSStatusWindow::StatusView::ProgressView	:: sDefaultVerboseState = false;



FSStatusWindow::FSStatusWindow() : inherited(BRect(0, 0, kStatusViewPreferredWidth, 100), "Tracker Status",
											B_FLOATING_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, // B_FLOATING_APP_WINDOW_FEEL,
											B_NOT_CLOSABLE + B_NOT_V_RESIZABLE + B_NOT_ZOOMABLE, B_ALL_WORKSPACES),
									mDragger(mContainer) {

	SetSizeLimits(kStatusViewMinimalWidth, 1000, 20, 1000);

	MoveTo(10000, 10000);
	Hide();
	Show();

	BRect bounds = Bounds();
	
	mContainer.ResizeTo(bounds.Width(), bounds.Height());
	mDragger.ResizeTo(bounds.Width(), bounds.Height());
	
	mDragger.AddChild(&mContainer);
	AddChild(&mDragger);
//	mDragger.SetViewColor(B_TRANSPARENT_COLOR);
}

DEBUG_ONLY(
void
FSStatusWindow::Show() {
	ASSERT(mContainer.CountChildren() >= 0);
	
	inherited::Show();
}
)

void
FSStatusWindow::Pack() {
	mContainer.Pack();
}

FSStatusWindow::~FSStatusWindow() {
	mDragger.RemoveSelf();
	mContainer.RemoveSelf();
}

void
FSStatusWindow::DispatchMessage(BMessage *msg, BHandler *handler) {

	if (msg -> what == B_MODIFIERS_CHANGED)
		mContainer.ModifiersChanged();
	
	inherited::DispatchMessage(msg, handler);
}

void
FSStatusWindow::Add(TFSContext &in_context) {
	ContainerView *view = &mContainer;

	if (ContainerView::HasReplicantInstance())
		view = &ContainerView::ReplicantInstance();

	if (view -> LockLooper()) {	// make sure the view is still inact
		view -> Add(in_context);
		view -> UnlockLooper();
	}
}

void
FSStatusWindow::Remove(const TFSContext &in_context) {
	ContainerView *view = &mContainer;
	
	if (ContainerView::HasReplicantInstance())
		view = &ContainerView::ReplicantInstance();

	if (view -> LockLooper()) {	// make sure the view is still inact
		view -> Remove(in_context);
		view -> UnlockLooper();
	}
}

bool
FSStatusWindow::ShouldPause(const TFSContext &in_context) {
	ContainerView *view = &mContainer;
	
	if (ContainerView::HasReplicantInstance())
		view = &ContainerView::ReplicantInstance();

	bool b = false;
	
	if (view -> LockLooper()) {	// make sure the view is still inact
		b = view -> ShouldPause(in_context);
		view -> UnlockLooper();
	}
	return b;
}

bool
FSStatusWindow::QuitRequested() {

	BAutolock l(this);

	if (mContainer.CountChildren() <= 0)
		return true;

	return false;
}





void
FSStatusWindow::ContainerView::initialize() {
#if FS_USE_ELAPSED_TIME
	mInitTime = B_INFINITE_TIMEOUT;
#endif
	mPackNeeded = true;
	mReplicant = false;
	mPulseMessageRunner = 0;

#if DEBUG
	SetViewColor(255, 0, 0);		// make it ugly red to catch eye
#else
	SetViewColor(B_TRANSPARENT_COLOR);
#endif
}

FSStatusWindow::ContainerView::ContainerView() : inherited(BRect(), "StatusView Container",
														B_FOLLOW_ALL, B_FRAME_EVENTS + B_WILL_DRAW) {
	initialize();
}

FSStatusWindow::ContainerView::ContainerView(BMessage *in_msg) : inherited(in_msg) {
	initialize();
}

FSStatusWindow::ContainerView::~ContainerView() {
	delete mPulseMessageRunner;
	
	if (mReplicant) {
		sReplicantInstance = 0;

		BAutolock l(&gStatusWindow());
	
		if (l.IsLocked()) {					// give possible running operation contexts to statuswindow
			ContainerView &cview = gStatusWindow().Container();
			
			int32 count = CountChildren();
			for (int32 i = 0;  i < count;  ++i) {
				StatusView *sview = assert_cast<StatusView *>(ChildAt(i));
				sview -> RemoveSelf();
				cview.AddChild(sview);
			}
			
			cview.Pack();
			
			if (cview.CountChildren() > 0) {
				if (gStatusWindow().IsHidden())
					gStatusWindow().Show();
				cview.SetPulseRate(kRefreshRate);
			}
		}
	}
}

void
FSStatusWindow::ContainerView::ModifiersChanged() {
	ASSERT(Window() -> IsLocked());

	int32 count = CountChildren();
	for (int32 i = 0;  i < count;  ++i)
		assert_cast<StatusView *>(ChildAt(i)) -> ModifiersChanged();
}

BArchivable *
FSStatusWindow::ContainerView::Instantiate(BMessage *in_msg) {
	if (dynamic_cast<TTracker *>(be_app) != 0) {		// allow instances only in tracker
		if (validate_instantiation(in_msg, kClassName)  &&  sReplicantInstance == 0) {	// allow only one replicant instance
			ContainerView &view(*new ContainerView(in_msg));
			view.mReplicant = true;
			sReplicantInstance = &view;
			return &view;
		}
	} else {
	
		beep();		// XXX alert?
	}
	
	return 0;
}

status_t
FSStatusWindow::ContainerView::Archive(BMessage *in_msg, bool in_deep) const {
//	status_t rc = inherited::Archive(in_msg, in_deep);
	
//	rc |= in_msg -> AddString("add_on", kTrackerSignature);	// don't add signature to avoid auto-loading when dropped into another team
//	rc |= in_msg -> AddString("class", kClassName);

	inherited::Archive(in_msg, in_deep);
	in_msg->AddString("add_on", kTrackerSignature);

	in_msg->AddRect("bounds", Bounds());
//	in_msg->AddInt32("face", OffscreenView->fFace);
	
if (find_instantiation_func(kClassName) != 0)	// XXX
puts("ok");
	
	return 0;
}

status_t
FSStatusWindow::CustomDragger::Archive(BMessage *msg, bool deep) {
//	status_t rc = inherited::Archive(msg, deep);
	
//	rc |= msg -> AddString("add_on", kTrackerSignature);	// don't add signature to avoid auto-loading when dropped into another team
//	rc |= msg -> AddString("class", kClassName);

	inherited::Archive(msg, deep);
	msg->AddString("add_on", kTrackerSignature);

	msg->AddRect("bounds", Bounds());
	
	return 0;
}


void
FSStatusWindow::ContainerView::AllAttached() {
	BWindow &win = *Window();

	if (mReplicant) {
		BAutolock l(&gStatusWindow());
	
		if (l.IsLocked()) {									// steal possible running operation contexts from statuswindow
			ContainerView &cview = gStatusWindow().Container();
			int32 count = cview.CountChildren();
			
			if (count > 0) {
				cview.SetPulseRate(0);
				gStatusWindow().Hide();
			
				for (int32 i = 0;  i < count;  ++i) {
					StatusView &sview = dynamic_cast<StatusView &>(*cview.ChildAt(i));
					if (&sview != 0) {
						sview.RemoveSelf();
						AddChild(&sview);
					}
				}
				Pack();
				cview.Pack();
			}
		}
	}
	
	if (BScreen(&win).Frame().Contains(win.Frame()) == false)
		win.MoveTo(BAlert::AlertPosition(win.Bounds().Width(), win.Bounds().Height()));
		
	if (mPackNeeded)
		Pack();

	SetPulseRate(kRefreshRate);
}

void
FSStatusWindow::ContainerView::SetPulseRate(bigtime_t rate) {
	if (rate == 0) {
	
		delete mPulseMessageRunner;
		mPulseMessageRunner = 0;
		
	} else {
	
		if (mPulseMessageRunner == 0)
			mPulseMessageRunner = new BMessageRunner(BMessenger(this, Window()), new BMessage(B_PULSE), rate);
		else
			mPulseMessageRunner -> SetInterval(rate);
	}
}

void
FSStatusWindow::ContainerView::CustomPulse() {
	ASSERT(Window() -> IsLocked());
	
#if FS_USE_ELAPSED_TIME

	if (Window() == &gStatusWindow()  &&  Window() -> IsHidden()) {
	
		int32 i, count = CountChildren();
		for (i = 0;  i < count; ++i) {
		
			TFSContext &context(assert_cast<StatusView *>(ChildAt(i)) -> FSContext());
			
			if (context.ElapsedTime() > kPopUpDelay) {
				Window() -> Show();
				if (context.HasDialogWindow())
					context.DialogWindow() -> Activate();
				break;
			}
		}
	}

#else
	if ((system_time() - mInitTime) > kPopUpDelay) {
	
		if (mPackNeeded)
			Pack();

		if (Window() == &gStatusWindow()  &&  Window() -> IsHidden()) {	// if window is hidden offscreen then move it to position

// XXX do we need it?
//			bigtime_t max = 0;								// search for the longest estimated operation
//			int32 count = CountChildren();
//			for (int32 i = 0;  i < count;  ++i) {
//				StatusView &view = *assert_cast<StatusView *>(ChildAt(i));
//				if (view.EstimatedTimeLeft() > max)
//					max = view.EstimatedTimeLeft();
//			}
//			
//			if (max  &&  max > kPopUpDelay) {					// and show window if it takes longer then kPopUpDelay
				ASSERT(CountChildren() >= 0);
				Window() -> Show();
			}
			
		} else {
		
			mInitTime = B_INFINITE_TIMEOUT;
		}
	}
#endif
	int32 count = CountChildren();
	for (int32 i = 0;  i < count;  ++i)		// ??? will call BDragger::Pulse, but it should be harmless
		assert_cast<StatusView *>(ChildAt(i)) -> CustomPulse();
}

void
FSStatusWindow::ContainerView::MessageReceived(BMessage *in_msg) {
	switch (in_msg -> what) {
		case B_PULSE:
			CustomPulse();
			break;
	
		default:
			inherited::MessageReceived(in_msg);
	}
}


void
FSStatusWindow::ContainerView::Add(TFSContext &in_context) {
	ASSERT(Window() -> IsLocked());
	
	StatusView &view(*new StatusView(in_context));
	view.Hide();
	AddChild(&view);
#if	FS_USE_ELAPSED_TIME
	mInitTime = system_time();
#endif
//	mPackNeeded = true;				// XXX remove is no problem
	SetPulseRate(kRefreshRate);
}

void
FSStatusWindow::ContainerView::Remove(const TFSContext &in_context) {
	ASSERT(Window() -> IsLocked());
		
	int32 i, count = CountChildren();
	for (i = 0;  i < count; ++i) {
	
		BView *view = ChildAt(i);
		
		if (*assert_cast<StatusView *>(view) == in_context) {
			RemoveChild(view);
			delete view;
			
			// resume next autopaused
			--count;
			for (;  i < count;  ++i) {
			
				TFSContext &context = assert_cast<StatusView *>(ChildAt(i)) -> FSContext();
				if (context.IsAutoPaused()) {
					context.ContinueAutoPaused();
					i = count;
					break;
				}
			}
			break;
		}
	}

	Pack();

	if (CountChildren() <= 0) {
		SetPulseRate(0);
		Window() -> Hide();
	}
}

bool
FSStatusWindow::ContainerView::ShouldPause(const TFSContext &in_context) {
	ASSERT(Looper() -> IsLocked());
	
	int32 count = CountChildren();
	if (count <= 1)
		return false;
		
	for (int32 i = 0;  i < count;  ++i) {
		StatusView *view = assert_cast<StatusView *>(ChildAt(i));
		TFSContext &context = view -> FSContext();
		
		if (&context == &in_context) {
			if (i == 0)			// first operation should be running
				return false;
		
			continue;
		}
		
		if (context.IsDeviceInvolved(in_context.SourceDevice())  ||
			context.IsDeviceInvolved(in_context.TargetDevice())) {

			return true;
		}
	}
	
	return false;
}

bool
FSStatusWindow::ContainerView::AttemptToQuit() {
	if (LockLooper()) {
		int32 count = CountChildren();
		for (int32 i = 0;  i < count; ++i)
			assert_cast<StatusView *>(ChildAt(i)) -> Cancel();

		UnlockLooper();
	}
	return false;
}

void
FSStatusWindow::ContainerView::Pack() {

	ASSERT(Window() -> IsLocked());

	mPackNeeded = false;

	Window() -> DisableUpdates();
	
	BRect rect(0, 0, 0, 0), our_bounds = Bounds();
	float total_height = 0;

	int32 count = CountChildren();
	for (int32 i = 0;  i < count;  ++i) {
		BView *_view = ChildAt(i);
		
		StatusView &view = *assert_cast<StatusView *>(_view);
		
		if (view.ShouldShow() == false)
			continue;
		
		view.GetPreferredSize(&rect.right, &rect.bottom);	// ask view about its preferred size
		rect.right = our_bounds.right;						// force width to be our width
		rect.top = total_height;
		rect.bottom += rect.top;
		view.ResizeTo(rect.Width(), rect.Height());
		view.MoveTo(rect.LeftTop());
		view.Show();

		total_height += rect.Height();						// sum up all width
//		total_height = floor(total_height + 2.9);			// and leave space for the separator
		total_height = total_height + 3;					// and leave space for the separator
	}
	
	total_height -= 2;										// compensate last separator

	RedrawDragger();									// redraw the rightbottom of the last two views
	RedrawDragger(1);									// redraw the rightbottom of the statusview at last - 1

	rect = Window() -> Bounds();
	Window() -> ResizeTo(rect.right, total_height - 1);		// XXX why -1?
	Window() -> EnableUpdates();
}

void
FSStatusWindow::ContainerView::RedrawDragger(int32 offset) {	// invalidate our last child to redraw the dragger
	ASSERT(Window() -> IsLocked());
	
	int32 count = CountChildren();
	
	if (count - offset > 0) {
	
		StatusView &view(*assert_cast<StatusView *>(ChildAt(count - 1 - offset)));

		BRect rect = view.mRightVisibleRect;
		rect.top = rect.bottom - 10;
		view.Invalidate(rect);
	}
}

void
FSStatusWindow::ContainerView::Draw(BRect) {

	rgb_color panel_color = ui_color(B_PANEL_BACKGROUND_COLOR);
	
	SetHighColor(tint_color(panel_color, B_LIGHTEN_2_TINT));
	SetLowColor(tint_color(panel_color, B_DARKEN_2_TINT));

	BRect bounds = Bounds();
	BPoint start = bounds.LeftTop(), end = bounds.RightTop();
	
	int32 count = CountChildren();
	for (int32 i = 0;  i < count;  ++i) {
		BView &view = *ChildAt(i);
		
		start.y = view.Frame().bottom + 1;
		end.y = start.y;
		StrokeLine(start, end, B_SOLID_LOW);
		
		start.y += 1;
		end.y += 1;
		StrokeLine(start, end, B_SOLID_HIGH);
	}
}




FSStatusWindow::StatusView::StatusView(TFSContext &in_context) :
								inherited(BRect(0, 0, kStatusViewPreferredWidth, kStatusViewPreferredHeight),
											"Status View", B_FOLLOW_LEFT_RIGHT, B_WILL_DRAW + B_FRAME_EVENTS),
								mLastDetailsUpdated(0),
								mContext(in_context),
								mProgressView(mContext),
								mExpandButton(BRect(), "Details"),
								mExpandStringView("Details"),
								mStopButton(CustomButton::kStop, TFSContext::kCancel),
								mPauseButton(CustomButton::kPause, TFSContext::kPause),
								mSkipButton(CustomButton::kSkipRight, TFSContext::kSkipOperation),
								mBitmap(0),
								mExpanded(false) {
								
	SetViewColor(B_TRANSPARENT_COLOR);

	AddChild(&mProgressView);
	AddChild(&mStopButton);
	AddChild(&mPauseButton);
	AddChild(&mSkipButton);
	AddChild(&mExpandButton);
	mExpandButton.AddChild(&mExpandStringView);
	mExpandButton.SetMessage(new BMessage(kExpandMessage));
//	mSkipButton.Hide();

	int32 id = 0;
	
	switch ((int32)mContext.RootOperation()) {
		case TFSContext::kDuplicating:
		case TFSContext::kCopying:
			id = kResCopyStatusBitmap;
			break;

		case TFSContext::kCreatingLink:
		case TFSContext::kMoving:
			id = kResMoveStatusBitmap;
			break;

		case TFSContext::kEmptyingTrash:
		case TFSContext::kMovingToTrash:
		case TFSContext::kRemoving:
			id = kResTrashStatusBitmap;
			break;

//		default:
//			TRESPASS();
	}
	
	if (id)
		GetTrackerResources() -> GetBitmapResource(B_MESSAGE_TYPE, id, &mBitmap);
}

FSStatusWindow::StatusView::~StatusView() {
	mProgressView.RemoveSelf();
	
	mStopButton.RemoveSelf();
	mPauseButton.RemoveSelf();
	mSkipButton.RemoveSelf();
	mExpandStringView.RemoveSelf();
	mExpandButton.RemoveSelf();

	delete mBitmap;
}

void
FSStatusWindow::StatusView::CustomPulse() {

	if (IsHidden()  &&  ShouldShow())
		assert_cast<ContainerView *>(Parent()) -> Pack();

	mProgressView.CustomPulse();

	CustomButton::button_type type;
	
	if (mContext.IsPauseRequested())
		type = CustomButton::kPlay;
	else
		type = CustomButton::kPause;
	
	if (mPauseButton.Sign() != type)
		mPauseButton.SetSign(type);

	ModifiersChanged();

	if (mExpanded) {
		if (system_time() - mLastDetailsUpdated > kDetailsUpdateDelay) {
			mLastDetailsUpdated = system_time();
			
			BRect rect = Bounds();
			rect.top = mProgressView.Frame().bottom + 1;
			Invalidate(rect);
		}
	}
}

void
FSStatusWindow::StatusView::GetPreferredSize(float *in_width, float *in_height) {
	*in_width = (Bounds().Width() < kStatusViewMinimalWidth) ? kStatusViewMinimalWidth : Bounds().Width();

	float w, h;
	mProgressView.GetPreferredSize(&w, &h);

	float button_height = sFontHeight * kButtonHeightMultiplier;

	*in_height = (h > button_height) ? h : button_height;

	if (mExpanded) {
		*in_height += sFontHeight + 10;
		if (mContext.mProgressInfo.IsTotalSizeProgressEnabled()) {
			*in_height += sFontHeight * 2;
		}		
	}
}

void
FSStatusWindow::StatusView::FrameResized(float, float) {
	ASSERT(Window() -> IsLocked());

	mLeftVisibleRect = Bounds();
	mLeftVisibleRect.right = kProgressViewXOffsetFromLeft - 1;
	mLeftVisibleRect.bottom = mProgressView.Frame().bottom;
	
	mRightVisibleRect = Bounds();
//	mRightVisibleRect.left = mRightVisibleRect.right - kProgressViewXOffsetFromRight + 1;
	mRightVisibleRect.left = mRightVisibleRect.right - mStopButton.Bounds().right * (3.2 + 0.6);
	mRightVisibleRect.bottom = mLeftVisibleRect.bottom;

	mDetailsRect = Bounds();
	mDetailsRect.top = mProgressView.Frame().bottom + 1;
	mDetailsRect.InsetBy(3, 3);

	Pack();
	
	int32 index = Parent() -> CountChildren() - 1;
	if (index >= 0  &&  Parent() -> ChildAt(index) == this)
		assert_cast<ContainerView *>(Parent()) -> RedrawDragger();
}

void		// time is in usec
FSStatusWindow::StatusView::CreateTimeString(char *buf, bigtime_t time, bool fractions) {

	int32 i = time / 1000000;
	
	if (i > 60) {										// more then a minute
		
		int32 hour, min, sec, j;
		hour = i / (60*60);
		j = i % (60*60);
		min = j / 60;
		sec = j % 60;
		
		sprintf(buf, "%.2ld:%.2ld:%.2ld", hour, min, sec);

	} else {											// less then a minute
	
		sprintf(buf, (fractions) ? "%.1f s" : "%.0f s", (float)time / 1000000);
	}
}

void
FSStatusWindow::StatusView::DrawEstimatedTimeString(float ypos) {

	BPoint point;
	point.x = mDetailsRect.left + 3;
	point.y = ypos;

	SetLowColor(kStringLowColor);
	SetHighColor(kStringHighColor);

	DrawString("Time remaining:", point);

	char buf[256];
	
	int32 i = mContext.EstimatedTimeLeft();
	
	if (mContext.IsCalculating())
		return;

	if (i == 0  ||  i > 60 * 60) {						// no idea or more then an hour
		
		strcpy(buf, "?");
	} else {
		
		CreateTimeString(buf, (bigtime_t)i * 1000000);
	}		

	DEBUG_ONLY(if (i != 0) printf("Estimated time left: %ld sec\n", i));

//	point.x = mDetailsRect.right - StringWidth(buf) - 3;	// when right aligned
	point.x = mDetailsRect.left + 100;
	DrawString(buf, point);
}

void
FSStatusWindow::StatusView::ModifiersChanged() {
	Window() -> DisableUpdates();
	
	uint32 modif = modifiers();

	bool old_state = mSkipButton.IsEnabled();
	
	if	(	(mContext.IsAnswerPossible(TFSContext::fSkipOperation)  &&
				mContext.LastPrimaryOperation() == TFSContext::kCalculatingItemsAndSize)
		||	(mContext.IsAnswerPossible(TFSContext::fSkipDirectory) &&
				(modif & kSkipDirectoryKey))
		||	(mContext.IsAnswerPossible(TFSContext::fSkipEntry) &&
				(modif & kSkipEntryKey))
		) {
	
		mSkipButton.SetEnabled(true);

	} else {// if ((modif & (kSkipEntryKey | kSkipDirectoryKey)) == 0) {
	
		mSkipButton.SetEnabled(false);
	}

	if (old_state != mSkipButton.IsEnabled())
		mSkipButton.Invalidate();

	Window() -> EnableUpdates();
}

void
FSStatusWindow::StatusView::Draw(BRect in_rect) {

	SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	SetHighColor(0, 0, 0);
	
	FillRect(in_rect, B_SOLID_LOW);
	
	if (in_rect.Intersects(mLeftVisibleRect)) {
		FillRect(mLeftVisibleRect, B_SOLID_LOW);
		
		BRect rect = mLeftVisibleRect;
		rect.bottom = mExpandButton.Frame().top - 1;			// bitmap rect

		if (in_rect.Intersects(rect)  &&  mBitmap != 0) {
		
			BRect bitmap_bounds = mBitmap -> Bounds();
			
			DrawBitmap(mBitmap, BPoint(	(rect.Width() - bitmap_bounds.Width()) / 2,
										(rect.Height() - bitmap_bounds.Height()) / 2)
								);
		}
		
	}
	
	if (mExpanded) {
		BRect rect(mLeftVisibleRect);
		rect.top = rect.bottom;
		rect.bottom = mDetailsRect.top + 10;
		rect.left = mDetailsRect.left;

		float radius = 6;

		SetLowColor(tint_color(LowColor(), 1.07));
//		SetLowColor(tint_color(LowColor(), B_LIGHTEN_2_TINT));
		FillRoundRect(mDetailsRect, radius, radius, B_SOLID_LOW);

		FillRoundRect(rect, radius, radius, B_SOLID_LOW);
		
		Sync();	// XXX remove if occasional draw bug still exists
		
		float y = mDetailsRect.top + sFontHeightStruct.ascent + 3;

		DrawEstimatedTimeString(y);
		
		if (mContext.mProgressInfo.IsTotalSizeProgressEnabled()) {

			char buf[128];

			DrawString("Time elapsed:", BPoint(mDetailsRect.right - 140, y));
			CreateTimeString(buf, mContext.ElapsedTime());
			DrawString(buf, BPoint(mDetailsRect.right - 3 - StringWidth(buf), y));

			y += sFontHeight;

			if (mContext.WasMultithreadedCopy()) {
				DrawString("Reader waiting:", BPoint(mDetailsRect.right - 140, y));
				CreateTimeString(buf, mContext.ReaderThreadWaiting(), true);
				DrawString(buf, BPoint(mDetailsRect.right - 3 - StringWidth(buf), y));
			}

			float ruler = mDetailsRect.left + 100;
			
			DrawString("Copy speed:", BPoint(mDetailsRect.left + 3, y));

			sprintf(buf, "%.2f MB/s", (float)mContext.mProgressInfo.mCurrentSize / (mContext.ElapsedTime()));
			DrawString(buf, BPoint(ruler, y));
			
			y += sFontHeight;

			if (mContext.WasMultithreadedCopy()) {
				DrawString("Writer waiting:", BPoint(mDetailsRect.right - 140, y));
				CreateTimeString(buf, mContext.WriterThreadWaiting(), true);
				DrawString(buf, BPoint(mDetailsRect.right - 3 - StringWidth(buf), y));
			}
						
			DrawString("Queue size:", BPoint(mDetailsRect.left + 3, y));
			TFSContext::GetSizeString(buf, mContext.TotalMemoryUsage(), 0);
			DrawString(buf, BPoint(ruler, y));
		}
	}
}

void
FSStatusWindow::StatusView::AllAttached() {

	BFont font;										// set sFontHeight
	GetFont(&font);
	
	font.GetHeight(&sFontHeightStruct);
	
	sFontHeight = sFontHeightStruct.ascent + sFontHeightStruct.descent;
	
	mStopButton.SetTarget(this);					// set button targets to be us
	mPauseButton.SetTarget(this);
	mSkipButton.SetTarget(this);
	mExpandButton.SetTarget(this);

	mStopButton.SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));			// set view colors
	mPauseButton.SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mSkipButton.SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mExpandButton.SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mExpandStringView.SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	mExpandStringView.SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	mExpandStringView.MoveTo(12, 0);
	mExpandStringView.ResizeTo(kProgressViewXOffsetFromLeft - (10 + 4), sFontHeight);

	BRect bounds = Bounds();
	FrameResized(bounds.Width(), bounds.Height());	// set up visiblerects

	Pack();
}

void
FSStatusWindow::StatusView::Pack() {

	Window() -> DisableUpdates();
													// put ProgressView to it's position
	mProgressView.MoveTo(kProgressViewXOffsetFromLeft, 0);
	BRect bounds = Bounds();
	float w, h;
	mProgressView.GetPreferredSize(&w, &h);
	mProgressView.ResizeTo(bounds.Width() - mRightVisibleRect.Width() - kProgressViewXOffsetFromLeft, h);

	BRect rect = mRightVisibleRect;					// render buttons
	rect.InsetBy(8, 5);

	float button_height = sFontHeight * kButtonHeightMultiplier;
	float y_pos = rect.top + (rect.Height() - button_height) / 2;
	float button_width = button_height;
	float x_pos = rect.left + (rect.Width() - button_width * (3 + 0.2)) / 2;
	
	mPauseButton.ResizeTo(button_width, button_height);
	mPauseButton.MoveTo(x_pos, y_pos);

	x_pos += button_width * 1.1;

	mStopButton.ResizeTo(button_width, button_height);
	mStopButton.MoveTo(x_pos, y_pos);
	
	x_pos += button_width * 1.1;

	mSkipButton.ResizeTo(button_width, button_height);
	mSkipButton.MoveTo(x_pos, y_pos);

	mExpandButton.MoveTo(4, mLeftVisibleRect.bottom - (sFontHeightStruct.ascent + 1));
	mExpandButton.ResizeTo(kProgressViewXOffsetFromLeft - 6, sFontHeightStruct.ascent + 1);
	
	ResizeToPreferred();

	Invalidate(mLeftVisibleRect);
	
	Window() -> EnableUpdates();
}

void
FSStatusWindow::StatusView::MessageReceived(BMessage *in_msg) {

	switch (in_msg -> what) {
		case TFSContext::kSkipOperation: {
			uint32 modif = modifiers();
	
			if		(modif & kSkipDirectoryKey)		mContext.SkipDirectory();
			else if	(modif & kSkipEntryKey)			mContext.SkipEntry();
			else										mContext.SkipOperation();

			break;
		}
		
		case kExpandMessage: {
		
			mExpanded = ! mExpanded;
			
			FSStatusWindow *win = dynamic_cast<FSStatusWindow *>(Window());
			if (win)										// if we are in the statuswindow
				win -> Pack();
			else											// then we are replicants
				assert_cast<ContainerView *>(Parent()) -> Pack();
				
			break;
		}
		
		case TFSContext::kCancel:
			mProgressView.mStatusBar.SetText("Cancelling...");
			mContext.Cancel();
			break;
	
		case TFSContext::kPause:
			if (mContext.HasDialogWindow() == false) {
				if (mContext.Pause())
					mPauseButton.SetSign(CustomButton::kPlay);
				else
					mPauseButton.SetSign(CustomButton::kPause);
					
				mProgressView.SetOperationString();
			}
			break;

		default:
			inherited::MessageReceived(in_msg);
	}
}






FSStatusWindow::StatusView::ProgressView::ProgressView(TFSContext &in_context) :
						inherited(BRect(0, 0, 10, 10), "Progress View", B_FOLLOW_LEFT_RIGHT, B_WILL_DRAW + B_FRAME_EVENTS),
						mContext(in_context), mMode(TFSContext::kInvalidOperation),
						mStatusBar(BRect(), "StatusBar"), mVerbose(sDefaultVerboseState), mSyncLossDetected(false) {
	
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	
	AddChild(&mStatusBar);
	
	mStatusBar.SetResizingMode(B_FOLLOW_LEFT_RIGHT);
	mStatusBar.SetBarHeight(kStatusBarThickness);
	
	SetMode();
}

FSStatusWindow::StatusView::ProgressView::~ProgressView() {
	mStatusBar.RemoveSelf();
}

void
FSStatusWindow::StatusView::ProgressView::AllAttached() {
	ASSERT(dynamic_cast<StatusView *>(Parent()) != 0);
	
	BRect rect = Bounds();
	
	float w, h;
	mStatusBar.GetPreferredSize(&w, &h);
	
	mStatusBar.ResizeTo(rect.Width(), h);
	mStatusBar.MoveTo(rect.left, rect.top);
	mStatusBar.SetMaxValue(1);
}

const char *
FSStatusWindow::StatusView::ProgressView::FilterOperationForDisplay(TFSContext::operation op) {

	if (op != TFSContext::kCalculatingItemsAndSize) {			// allow kCalculatingItemsAndSize to override any operation string

		switch ((int32)mContext.RootOperation()) {			// disallow any operation string override for the operations below
	
			case TFSContext::kEmptyingTrash:
				op = TFSContext::kEmptyingTrash;
				break;
		}
	}

	return TFSContext::AsString(op);
}

void
FSStatusWindow::StatusView::ProgressView::SetOperationString() {
		
	BString str(FilterOperationForDisplay(mContext.LastPrimaryOperation()));
	
	if (mContext.IsPauseRequested()) {
		if (mContext.IsPaused())
			str.Append(" (Paused)");
		else
			str.Append(" (Pausing...)");
	}
	
	mStatusBar.SetText(str.String());
	
	mContext.ClearOperationStringDirty();
}

void
FSStatusWindow::StatusView::ProgressView::SetMode() {

	mMode = mContext.LastPrimaryOperation();

	rgb_color color;
	
	if (mContext.mProgressInfo.IsTotalSizeProgressEnabled())
		color = kStatusBarCopyColor;
	else
		color = kStatusBarOtherColor;

	if (mContext.mProgressInfo.IsTotalEnabled() == false)	// if total values are not in sync anymore
		color = tint_color(color, B_DISABLED_MARK_TINT);	// titn the progress bar color

	mStatusBar.SetBarColor(color);

	SetOperationString();
}

void
FSStatusWindow::StatusView::ProgressView::CustomPulse() {
	TFSContext::ProgressInfo &info = mContext.mProgressInfo;

	// follow operation changes of the context
	if (mMode != mContext.LastPrimaryOperation())
		SetMode();

	if (info.IsTotalEnabled() == false  &&  mSyncLossDetected == false) {
		mSyncLossDetected = true;
		SetMode();
	}
	
	if (info.IsDirty() == false)						// no update since last check
		return;

	DrawCurrentEntry();									// XXX optimize?
	
	char buf[128];
	char *ptr = buf;
	if (info.IsTotalSizeProgressEnabled()) {
	
		if (mContext.LastPrimaryOperation() == TFSContext::kCalculatingItemsAndSize  &&  info.TotalSizeProgress() <= 0) {
			// we are in the phase of the first preflight scan
			ptr += TFSContext::GetSizeString(ptr, info.mTotalSize, 0);
		} else if (info.IsTotalEnabled()  ||  info.mTotalSize > info.mCurrentSize) {
		
			ptr += TFSContext::GetSizeString(ptr, info.mCurrentSize, info.mTotalSize);
			
			if (info.IsTotalEnabled() == false)
				ptr += sprintf(ptr, " (?)");
		} else {
		
			ptr += TFSContext::GetSizeString(ptr, info.mCurrentSize, 0);
		}
#if B_BEOS_VERSION_DANO
		mStatusBar.SetTo(info.TotalSizeProgress(), NULL, buf);
#else
		mStatusBar.fCurrent = info.TotalSizeProgress();			// XXX
		mStatusBar._Draw(mStatusBar.Bounds(), true);
#endif
	} else {
	
		if (mContext.LastPrimaryOperation() == TFSContext::kCalculatingItemsAndSize  &&  info.EntryProgress() <= 0) {
			// we are in the phase of the first preflight scan
			ptr += sprintf(ptr, "%ld", info.mTotalEntryCount);
		} else if (info.IsTotalEnabled()  ||  info.mTotalEntryCount > info.mCurrentEntryCount) {
		
			ptr += sprintf(ptr, "%ld / %ld", info.mCurrentEntryCount + 1, info.mTotalEntryCount);
			if (info.IsTotalEnabled() == false)
				ptr += sprintf(ptr, " (?)");
		} else {
		
			ptr += sprintf(ptr, "%ld", info.mCurrentEntryCount + 1);
		}
		
#ifdef B_BEOS_VERSION_DANO
		mStatusBar.SetTo(info.EntryProgress(), NULL, buf);
#else
		mStatusBar.fCurrent = info.EntryProgress();				// XXX
		mStatusBar._Draw(mStatusBar.Bounds(), true);
#endif
	}
#if !B_BEOS_VERSION_DANO
	mStatusBar.SetTrailingText(buf);
#endif
	
	info.ClearDirty();
	
	float w, h;
	GetPreferredSize(&w, &h);
	
	BRect bounds = Bounds();
	
	if (mContext.IsOperationStringDirty())
		SetOperationString();
	
	if (w > bounds.Width()  ||  h > bounds.Height()) {			// if we need more space call owner window's Pack()
		FSStatusWindow *win = dynamic_cast<FSStatusWindow *>(Window());
		if (win) {
														// but only if we are in the statuswindow
			win -> Pack();
		} else {										// then we are replicants
		
			assert_cast<ContainerView *>(Parent() -> Parent()) -> Pack();
		}
	}
}


void
FSStatusWindow::StatusView::ProgressView::GetPreferredSize(float *in_width, float *in_height) {

	BRect bounds = Bounds();
	*in_width = bounds.Width();

	float w, h;
	mStatusBar.GetPreferredSize(&w, &h);

	*in_height = h;
	*in_height += sFontHeight * 1.2;

	if (IsVerbose())
		*in_height += sFontHeight * 3 * 1.5;

	if (*in_height < bounds.Height())
		*in_height = bounds.Height();				// only growth is allowed
}

void
FSStatusWindow::StatusView::ProgressView::DrawCurrentEntry() {

//	if (mContext.DidOperationBegin() == false)	// XXX ?
//		return;

	BRect rect = Bounds();
	rect.top = mStatusBar.Frame().bottom + 1;

	BRect clear_rect(rect);
	clear_rect.bottom = rect.top + sFontHeight;
	SetLowColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	FillRect(clear_rect, B_SOLID_LOW);

	BPoint point;
	point.y = rect.top + sFontHeightStruct.ascent;

	SetHighColor(kStringHighColor);
	SetLowColor(kStringLowColor);

	BString str(mContext.CurrentEntryName());

	TFSContext::ProgressInfo &info = mContext.mProgressInfo;
	if (info.IsTotalSizeProgressEnabled()) {
		char buf[64];
		char *ptr = buf;

		if (mContext.LastPrimaryOperation() == TFSContext::kCalculatingItemsAndSize  &&  info.EntryProgress() <= 0) {
			// we are in the phase of the first preflight scan
			ptr += sprintf(buf, "%ld", info.mTotalEntryCount);
		} else if (info.IsTotalEnabled()  ||  info.mTotalEntryCount > info.mCurrentEntryCount) {
		
			ptr += sprintf(ptr, "%ld / %ld", info.mCurrentEntryCount + 1, info.mTotalEntryCount);
			if (info.IsTotalEnabled() == false)
				ptr += sprintf(ptr, " (?)");
		} else {
				
			ptr += sprintf(buf, "%ld", info.mCurrentEntryCount + 1);
		}

		point.x = rect.right - StringWidth(buf);
		DrawString(buf, point);

		if (info.HasFileSizeProgress())
			point.x -= StringWidth(" (100%)");

		TruncateString(&str, B_TRUNCATE_END, point.x - 4);
		
		if (info.HasFileSizeProgress()  &&  info.FileSizeProgress() > 0.001) {
			int32 len = str.Length();
			sprintf(str.LockBuffer(len + 10) + len, " (%.0f%%)", info.FileSizeProgress() * 100.);
			str.UnlockBuffer();
		}
	}
				
	point.x = rect.left;
	
	DrawString(str.String(), point);
}

void
FSStatusWindow::StatusView::ProgressView::Draw(BRect in_rect) {

	BRect rect = Bounds();
	rect.top = mStatusBar.Frame().bottom + 1;
	
	if (rect.Intersects(in_rect))
		DrawCurrentEntry();
}

void
FSStatusWindow::StatusView::ProgressView::FrameResized(float, float) {
	DrawCurrentEntry();
}


void
FSStatusWindow::StatusView::CustomButton::Draw(BRect rect) {

	if (Value() != 0) {
		SetViewColor(0, 0, 0);
		SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	} else {
		SetHighColor(0, 0, 0);
		SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	}
		
	inherited::Draw(rect);

	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	BRect bounds = Bounds();
	rect = bounds;

	if (IsFocusChanging() == false) {
		switch (mType) {
		
			case kStop: {
				static const float kStopsignSize = 0.40;
				
				float size = rect.Width() * kStopsignSize;
				if (rect.Height() * kStopsignSize < size)
					size = rect.Height() * kStopsignSize;
				rect.left = rect.top = 0;
				rect.right = rect.bottom = size;
				rect.OffsetBy((bounds.right - rect.right) / 2, (bounds.bottom - rect.bottom) / 2);
				FillRect(rect);
				break;
			}
			case kPause: {
		
				BRect tmp = bounds;
				tmp.InsetBy(tmp.Width() * 0.27, tmp.Height() * 0.28);
				rect = tmp;
				rect.right = rect.left + tmp.Width() * 0.32;
				FillRect(rect);
		
				rect = tmp;
				rect.left = rect.right - tmp.Width() * 0.32;
				FillRect(rect);
				break;
			}
			case kPlay:
			
				rect.InsetBy(rect.Width() * 0.3, rect.Height() * 0.27);
				FillTriangle(rect.LeftTop(), rect.LeftBottom(), BPoint(rect.right, rect.top + rect.Height() / 2), rect);
				break;
					
			case kSkipRight:
			
				if (modifiers() & kSkipDirectoryKey) {
					rect.InsetBy(rect.Width() * 0.25, rect.Height() * 0.25);
					FillTriangle(	rect.LeftBottom(), rect.RightBottom(),
									BPoint(rect.left + rect.Width() / 2, rect.bottom - rect.Height() * 0.75), rect);
					rect.bottom -= rect.Height() * 0.75 + 1;
					FillRect(rect);
				} else {
					rect.InsetBy(rect.Width() * 0.25, rect.Height() * 0.25);
					FillTriangle(	rect.LeftTop(), rect.LeftBottom(),
									BPoint(rect.left + rect.Width() * 0.75, rect.top + rect.Height() / 2), rect);
					rect.left += rect.Width() * 0.75 + 1;
					FillRect(rect);
				}
				break;
	
			default: TRESPASS();
		}
		
	}
	
	if (IsFocus()) {
		SetHighColor(ui_color(B_KEYBOARD_NAVIGATION_COLOR));
	} else {
		if (Value() != 0)
			SetHighColor(0, 0, 0);
		else
			SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_LIGHTEN_1_TINT));
	}		
	rect = bounds;
	rect.InsetBy(5, 5);
	StrokeLine(rect.LeftBottom(), rect.RightBottom());
}

}	// namespace BPrivate




