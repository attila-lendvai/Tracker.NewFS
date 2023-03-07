#if !defined(_STATUSWINDOW_H)
#define _STATUSWINDOW_H

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

#include "DialogPane.h"

#include <Window.h>
#include <Button.h>
#include <StopWatch.h>
#include <Dragger.h>
#include <MessageRunner.h>

#if B_BEOS_VERSION_DANO		// XXX clean it up once...
	#include <StatusBar.h>
#else
	#define private public			// XXX HUGE hack...
	#include <StatusBar.h>
	#undef private
#endif

#include <vector>

#define FS_USE_ELAPSED_TIME 1	// if defined the status window popoup delay will depend on the time actually
							// spend with the operation not the absolute time since it began (for example if
							// a dialog window is open this timer is paused)
namespace BPrivate {

static const bigtime_t	kPopUpDelay						= 600000;	// operations that does not take at least kPopUpDelay long will not open the status window

class FSStatusWindow : public BWindow {
friend FSStatusWindow &gStatusWindow();
	class ContextView;
	typedef BWindow						inherited;

	class ContainerView : public BView {
		typedef BView inherited;
		friend class FSStatusWindow;
		
		static const char * const kClassName = "FSStatusWindow::ContainerView";
		
		void initialize();
	public:
		ContainerView();
		~ContainerView();
		ContainerView(BMessage *);
		
		static BArchivable *Instantiate(BMessage *);
				status_t	Archive(BMessage *, bool) const;
				
				void		Add(TFSContext &);
				void		Remove(const TFSContext &);
				bool		ShouldPause(const TFSContext &);
				void		Draw(BRect);
				void		AllAttached();
				void		MessageReceived(BMessage *);
				void		ModifiersChanged();
				void		Pack();				// render things again
				bool		AttemptToQuit();	// cancel pending operations

				void		CustomPulse();
				void		SetPulseRate(bigtime_t rate);
				void		RedrawDragger(int32 offset = 0);
								
		static	bool		HasReplicantInstance()	{ return sReplicantInstance != 0; }
		static ContainerView &ReplicantInstance()	{ return *sReplicantInstance; }

	private:
		static ContainerView *sReplicantInstance;

		#if FS_USE_ELAPSED_TIME
		bigtime_t			mInitTime;				// the system_time() when the last status view was added
		#endif
		BMessageRunner *	mPulseMessageRunner;	// this is required because it's not wise to depend on DesktopWindow::PulseRate
		bool				mPackNeeded;
		bool				mReplicant;
	};
	

	class StatusView : public BView {
		typedef BView		inherited;
		friend class ContainerView;
		
		enum {
			kExpandMessage = 23456134
		};
		
		class CustomStringView : public BStringView {
				typedef BStringView inherited;
			public:
				CustomStringView(const char *name) : inherited(BRect(), name, name) {}
				void		MouseDown(BPoint point)				{ Parent() -> MouseDown(point); }
				void		MouseUp(BPoint point)				{ Parent() -> MouseUp(point); }
		};
		
		class CustomButton : public BButton {
			typedef BButton inherited;

		public:
			enum button_type {
				kStop = 1,
				kPause,
				kPlay,
				kSkipRight
			};

			CustomButton(button_type in_type, uint32 in_what) : inherited(BRect(), "", "", new BMessage(in_what),
											B_FOLLOW_LEFT + B_FOLLOW_TOP, B_WILL_DRAW | B_NAVIGABLE), mType(in_type) {}
			void	Draw(BRect);
			void	SetSign(button_type in_type)	{ mType = in_type; Invalidate(Bounds()); }
		button_type	Sign() const					{ return mType; }

		private:
			button_type		mType;
		};

		class ProgressView : public BView {	// the view with the progress bars
			friend class StatusView;
		public:
			ProgressView(TFSContext &);
			~ProgressView();
		
					void		AllAttached();
					void		GetPreferredSize(float *, float *);
					void		Draw(BRect);
					void		DrawCurrentEntry();
					void		FrameResized(float, float);
					void		CustomPulse();
					void		SetMode();
					const char *FilterOperationForDisplay(TFSContext::operation op);

					void		SetOperationString();
					
					void		SetVerbose(bool in_state)	{ mVerbose = in_state; }
					bool		IsVerbose() const			{ return mVerbose; }
		
		private:
			static bool							sDefaultVerboseState;

			TFSContext &						mContext;
			TFSContext::operation				mMode;
			BStatusBar							mStatusBar;
//			float								mCountStringPosition,
//												mTotalSizeStringPosition,
//												mSizeStringPosition;
			bool								mVerbose;
			bool								mSyncLossDetected;
		};

	public:
		StatusView(TFSContext &);
		~StatusView();
		
				void		AllAttached();
				void		GetPreferredSize(float *, float *);
				void		Draw(BRect);
				void		MouseDown(BPoint point)		{ assert_cast<CustomDragger *>(Parent() -> Parent()) -> MouseDown(point); }
				void		FrameResized(float, float);
				void		MessageReceived(BMessage *);

				void		ModifiersChanged();
				void		CreateTimeString(char *buf, bigtime_t time, bool fractions = false);
				void		DrawEstimatedTimeString(float);
				bigtime_t	EstimatedTimeLeft()							{ return mContext.EstimatedTimeLeft(); }
				void		CustomPulse();
				void		Pack();
				TFSContext &FSContext()									{ return mContext; }
				bool		ShouldShow() const							{ return (mContext.RealElapsedTime()  >  (kPopUpDelay * 1.2)); }
				
				void		Cancel()									{ mContext.Cancel(); }
				
				bool		operator==(const TFSContext &in_context)	{ return &mContext == &in_context; }
		
	private:
		static float			sFontHeight;
		static font_height		sFontHeightStruct;
		
		bigtime_t				mLastDetailsUpdated;
		TFSContext &			mContext;
		ProgressView			mProgressView;
		PaneSwitch				mExpandButton;
		CustomStringView		mExpandStringView;
		CustomButton			mStopButton,
								mPauseButton,
								mSkipButton;
		BRect					mLeftVisibleRect,
								mRightVisibleRect,
								mDetailsRect;
		BBitmap *				mBitmap;
		bool						mExpanded;
	};

	class CustomDragger : public BDragger {
		typedef BDragger inherited;
		static const char * const kClassName = "FSStatusWindow::CustomDragger";
	public:
			CustomDragger(BView &view) : inherited(BRect(), &view, B_FOLLOW_ALL,
											B_WILL_DRAW + B_DRAW_ON_CHILDREN) {}
			void		Draw(BRect)						{ if (IsVisibilityChanging()) assert_cast<ContainerView *>(Target()) -> RedrawDragger(); }
			void		DrawAfterChildren(BRect rect)	{ inherited::Draw(rect); }
			status_t	Archive(BMessage *msg, bool deep = true);
	};

public:
	FSStatusWindow();
	~FSStatusWindow();

			void		Add(TFSContext &);
			void		Remove(const TFSContext &);
			bool		ShouldPause(const TFSContext &);
			void		DispatchMessage(BMessage *, BHandler *);
			void		Pack();
		ContainerView &	Container()					{ ASSERT(IsLocked()); return mContainer; }
				

			DEBUG_ONLY(
				void		Show();
			)
			bool		QuitRequested();

			bool		AttemptToQuit()				{ return mContainer.AttemptToQuit(); }

private:
	static FSStatusWindow *	sSelf;
	static int32			sInitialized;

	ContainerView 			mContainer;
	CustomDragger			mDragger;
};


inline FSStatusWindow &
gStatusWindow() {
	if (atomic_or(&FSStatusWindow :: sInitialized, 1) == 0) {
		FSStatusWindow :: sSelf = new FSStatusWindow();
	}
	return *FSStatusWindow :: sSelf;
}

}	// namespace BPrivate

#endif // _STATUSWINDOW_H

