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

#if !defined(_FSDIALOGWINDOW_H)
#define _FSDIALOGWINDOW_H

#include <Window.h>
#include <TextView.h>
#include <RadioButton.h>
#include <Box.h>
#include <Button.h>
#include <ListView.h>
#include <TextControl.h>
#include <CheckBox.h>
#include <Autolock.h>

#include "TFSContext.h"
#include "LanguageTheme.h"

namespace BPrivate {

class FSDialogWindow;
class TFSContext;

class FSDialogWindow : public BWindow {
	typedef BWindow		inherited;

public:
	enum {
		kContinue = 124653562,
		kCancel,
		kRetry,
		kGo,
		kDefaultAnswerCB,
		kAnswerSelected,
		kDontAskKey,
		kTextControl
	};

	enum type {
		kError,
		kInteraction
	};

	class DialogView : public BView {
	public:
		DialogView() : BView(BRect(), "SomeDialogView", B_FOLLOW_NONE, 0) { }
		
		virtual	void		Pack(bool deep) = 0;
	};

	void	AnswerSelectionChanged(TFSContext::command cmd)		{ mMainView.AnswerSelectionChanged(cmd); }

private:

	friend class TFSContextDialogView : public DialogView {
		typedef DialogView	inherited;
			
		void	initialize();

		friend class CustomRadioButton : public BRadioButton {
				typedef BRadioButton inherited;
			
			public:
				CustomRadioButton(TFSContext &, TFSContext::command cmd);
				void		KeyDown(const char *, int32);
				void		MouseDown(BPoint point);
//				void		MouseUp(BPoint point)					{ Parent() -> MouseUp(ConvertToParent(point)); }

				TFSContext::command	mCommand;
			private:
				bigtime_t	mLastClickTime;
		};

		friend class CustomBBox;
		
		friend class RadioButtonView : public BView {
				friend class TFSContextDialogView;
				typedef BView inherited;		

			public:
				RadioButtonView(TFSContextDialogView *par) : inherited(BRect(), "RadioButtonView", B_FOLLOW_LEFT_RIGHT, B_NAVIGABLE_JUMP),
									mExpanded(sDefaultExpandedState), mHasExtendedButton(false), mHasNonExtendedButton(false),
									mParent(par) {
										SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
									}
				~RadioButtonView()								{ sDefaultExpandedState = mExpanded; }
				void		ResizeToPreferred();				
				void		AddChild(BView *);
	CustomRadioButton *	ActiveButton();
				void		KeyDown(const char *, int32);
				void		SetExpanded(bool state);
				void		SwitchExpanded() 						{ SetExpanded( ! mExpanded); }
				bool		IsExpanded() const						{ return mExpanded; }
				bool		HasExtendedButton() const				{ return mHasExtendedButton; }
				bool		HasNonExtendedButton() const			{ return mHasNonExtendedButton; }
				void		MouseUp(BPoint point) {
//							int32 buttons = Window() -> CurrentMessage() -> FindInt32("buttons");
//							if (buttons & B_TERTIARY_MOUSE_BUTTON)
//								SwitchExpanded();
//							else
								Parent() -> MouseUp(ConvertToParent(point));
						}
				void		MessageReceived(BMessage *);
				
			private:
				bool			mExpanded;
				bool			mHasExtendedButton;
				bool			mHasNonExtendedButton;
				TFSContextDialogView *	mParent;

				static	bool		sDefaultExpandedState;
		};
		
		friend class CustomBBox : public BBox {
			typedef BBox inherited;
			public:
				CustomBBox() : inherited(BRect(), B_FOLLOW_NONE, B_WILL_DRAW | B_FRAME_EVENTS | B_DRAW_ON_CHILDREN),
								mExpandButtonRect(0, 0, 10000, 10000) {}

		RadioButtonView *GetRadioButtonView() const {
							for (int32 i = 0;  i < CountChildren();  ++i) {
								TFSContextDialogView::RadioButtonView *view = dynamic_cast<TFSContextDialogView::RadioButtonView *>(ChildAt(i));
								if (view != 0)
									return view;
							}
							TRESPASS();
							return static_cast<TFSContextDialogView::RadioButtonView *>(0);
						}
				void		ResizeToPreferred() {
							TFSContextDialogView::RadioButtonView *view = GetRadioButtonView();
							view -> ResizeToPreferred();
							BRect rect = view -> Bounds();
							ResizeTo(rect.Width() + 8, rect.Height() + 8);
							view -> ResizeTo(rect.Width(), rect.Height());
							view -> MoveTo(4, 4);
						}
				void		DrawAfterChildren(BRect);
				void 	MouseUp(BPoint);
				void		FrameResized(float, float);
			private:
					BRect				mExpandButtonRect;
		};
		
		class CustomDefAnswCheckBox : public BCheckBox {
			typedef BCheckBox inherited;
			public:
				CustomDefAnswCheckBox() : inherited(BRect(), "DefaultAnswerCB", LOCALE("Don't ask again for this session"),
													new BMessage(kDefaultAnswerCB), B_FOLLOW_NONE) {}
		};

		class CustomTextControl : public BTextControl {
			typedef BTextControl inherited;
			public:
				CustomTextControl() : inherited(BRect(), "TextControl", 0, 0, 0, B_FOLLOW_LEFT_RIGHT,
												B_WILL_DRAW | B_NAVIGABLE | B_NAVIGABLE_JUMP) {
					SetModificationMessage(new BMessage(kTextControl));
				}
		};

		class CustomTextView : public BTextView {
			typedef BTextView inherited;		

			public:
				CustomTextView() : inherited(BRect(), "TextView", BRect(), B_FOLLOW_LEFT_RIGHT, B_FRAME_EVENTS | B_WILL_DRAW) {}

				bool	CanEndLine(int32 pos) {
							if (ByteAt(pos) == '\\')
								return true;
							
							return inherited::CanEndLine(pos);
						}
				void	ResizeToPreferred();
		};

	public:
		TFSContextDialogView(TFSContext &, TFSContext::interaction);
		TFSContextDialogView(TFSContext &, status_t);
		~TFSContextDialogView();

		void		Pack(bool deep);
		void		AllAttached();
		void		SetInfoText();
		void		SetTextControlText(char *text)	{ mTextControl.SetText(text); }
		void		AnswerSelectionChanged(TFSContext::command cmd);
		bool		SelectAnswer(TFSContext::command cmd);
		void		SwitchDontAsk()	{
					mDefaultAnswerCB.SetValue(mDefaultAnswerCB.Value() == B_CONTROL_ON ? B_CONTROL_OFF : B_CONTROL_ON);
					mDefaultAnswerCB.Invoke();
				}
		TFSContext::interaction		InteractionCode()		{ return mInteractionCode; };

	private:
		TFSContext &				mContext;
		type						mType;
		TFSContext::interaction		mInteractionCode;
		status_t						mErrorCode;
		RadioButtonView				mRadioButtonView;
		CustomBBox				mButtonBox;
		CustomDefAnswCheckBox	mDefaultAnswerCB;
		CustomTextView				mTextView;
		CustomTextControl			mTextControl;
	};

	class MainView : public BView {
		typedef BView	inherited;
		friend class FSDialogWindow;
	
	public:
		MainView();
		~MainView();
		

		void		Pack(bool deep);
		void		SetDialogView(DialogView *view);
		void		AllAttached();
		void		Draw(BRect iRect);
		void		SelectAnswer(TFSContext::command cmd) {
					BAutolock l(Window());
					
					if (l.IsLocked()  &&  assert_cast<TFSContextDialogView *>(mDialogView) -> SelectAnswer(cmd) == true)
						SetContinueEnabled(true);
				}
		void		SetTextControlText(char *text) {
					ASSERT(mDialogView != 0);

					TFSContextDialogView *v = dynamic_cast<TFSContextDialogView *>(mDialogView);
					v -> SetTextControlText(text);
				}
		void	AnswerSelectionChanged(TFSContext::command cmd) {
				TFSContextDialogView *view = dynamic_cast<TFSContextDialogView *>(mDialogView);
				if (view)
					view -> AnswerSelectionChanged(cmd);
			}
		void	SwitchDontAsk() {
				ASSERT(mDialogView);
				TFSContextDialogView *view = dynamic_cast<TFSContextDialogView *>(mDialogView);
				if (view)
					view -> SwitchDontAsk();
			}
		void	SetContinueEnabled(bool state) {
				mContinueButton.SetEnabled(state);
			}
		void	SetRetryButtonEnabled(bool state) {
					if (state == false)
						mRetryButton.Hide();
				}

	private:
		BButton			mContinueButton,
						mCancelButton,
						mRetryButton;
		DialogView *		mDialogView;
	};

	void	initialize();

public:
	friend class TFSContextDialogView::RadioButtonView;
	
	FSDialogWindow(TFSContext::command *, bool *, FSDialogWindow **, TFSContext &, TFSContext::interaction);
	FSDialogWindow(TFSContext::command *, bool *, FSDialogWindow **, TFSContext &, status_t);
	~FSDialogWindow();

			void			Go();
			bool			HasReply()							{ return mReply != TFSContext::kInvalidCommand; }
	TFSContext::command	Reply()							{ ASSERT(HasReply()); return mReply; }

			void			SetDialogView(DialogView *view)		{ mMainView.SetDialogView(view); }
			void			SetContext(TFSContext &context, TFSContext::interaction);
			void			SetContext(TFSContext &context, status_t);
			void			SetTextControlText(char *text)		{ mMainView.SetTextControlText(text); }
		TFSContext &	Context()							{ return mContext; }

			void			SelectAnswer(TFSContext::command cmd)		{ mMainView.SelectAnswer(cmd); }
			void			MessageReceived(BMessage *);
//			void			DispatchMessage(BMessage *, BHandler *);
			void			Pack(bool deep);
			void			SetType(type);
			void			Cancel();
			void			Quit();

				
			bool			QuitRequested();

private:
	static BPoint				sWindowLeftTop;

	FSDialogWindow **		mContainer;
	const char *				mTextControlText;
	TFSContext::command *	mReplyPtr;
	bool *					mDefaultAnswerPtr;		// a flag to be filled in by the state of the defalt checkbox at exit
	TFSContext::command	mReply;
	TFSContext &			mContext;
	MainView				mMainView;
	type					mType;
};


}	// namespace BPrivate

#endif // _FSDIALOGWINDOW_H

