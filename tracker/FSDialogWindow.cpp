/* Open Tracker License

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
All rights reserved. */

#include "FSDialogWindow.h"
#include "LanguageTheme.h"

#include <Debug.h>
#include <Screen.h>
#include <Autolock.h>
#include <Alert.h>

#include <memory>

namespace BPrivate {

using namespace fs;

//TODO:
// do something about resizing

static const float			kColoredRectWidth			= 35;
static const float			kMinDialogWindowHeight		= 100;
static const rgb_color		kColoredRectColor			= {164, 181, 197, 0};
//#define 					kColoredRectColor			tint_color(ViewColor(), B_DARKEN_1_TINT)
static const float			kRadioButtonViewWidth		= 200;	// low limit
static const float			kRadioButtonLeftOffset		= 20;
static const float			kMaxTextViewWidth			= 400;

static const float			kTextViewHeight				= 90;	// workaround for the BTextView::TextHeight() bug

//#define FS_SHOW_ERROR_CODE 1		// print the error code number in the error string
#define FS_SHOW_OPERATION_STACK 1		// print the whole operation stack in braces

bool		FSDialogWindow::TFSContextDialogView::RadioButtonView::sDefaultExpandedState		= false;
BPoint	FSDialogWindow::sWindowLeftTop(-1, -1);

void
FSDialogWindow::initialize() {

	mReply = TFSContext::kInvalidCommand;
	*mDefaultAnswerPtr = false;
	mTextControlText = 0;
	
	Minimize(true);
	AddChild(&mMainView);
	SetSizeLimits(20, 1000, 20, 1000);

	AddShortcut(B_ESCAPE, 0, new BMessage(kCancel));	// XXX useless?!
	AddShortcut(B_SPACE, B_COMMAND_KEY, new BMessage(kDontAskKey));

// XXX HERE: without at least one Show() the BAutolocks in Add() and Remove() will end up in a deadlock?! BeOS bug.
	Show();
	Hide();
}


FSDialogWindow::FSDialogWindow(TFSContext::command *replyptr, bool *defptr, FSDialogWindow **iptr, TFSContext &context,
								 TFSContext::interaction iact) : inherited(BRect(0, 0, 100, 100), "BuG!",
											B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, // B_FLOATING_APP_WINDOW_FEEL,
											B_NOT_RESIZABLE | B_NOT_ZOOMABLE, B_ALL_WORKSPACES),
									mContainer(iptr),
									mReplyPtr(replyptr),
									mDefaultAnswerPtr(defptr),
									mContext(context),
									mMainView(),
									mType(kInteraction) {
	initialize();

	if (mContext.TargetNewName()  &&  mContext.SourceNewName())
		strncpy(mContext.TargetNewName(), mContext.SourceNewName(), B_FILE_NAME_LENGTH - 1);

	SetType(kInteraction);
	SetContext(context, iact);

	BAutolock l(this);

	Pack(true);
}

FSDialogWindow::FSDialogWindow(TFSContext::command *replyptr, bool *defptr, FSDialogWindow **iptr, TFSContext &context, status_t rc) :
								 inherited(BRect(0, 0, 100, 100), "BuG!",
											B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, // B_FLOATING_APP_WINDOW_FEEL,
											B_NOT_RESIZABLE | B_NOT_ZOOMABLE, B_ALL_WORKSPACES),
									mContainer(iptr),
									mReplyPtr(replyptr),
									mDefaultAnswerPtr(defptr),
									mContext(context),
									mMainView(),
									mType(kError) {
	initialize();

	SetType(kError);
	SetContext(context, rc);

	BAutolock l(this);

	Pack(true);
}

FSDialogWindow::~FSDialogWindow() {
	mMainView.RemoveSelf();
	sWindowLeftTop = Frame().LeftTop();
	
	*mContainer = 0;
}

void
FSDialogWindow::SetType(type t) {
	mType = t;
	
	if (mType == kError) {
		SetTitle(LOCALE("Tracker Error"));
	} else if (mType == kInteraction) {
		SetTitle(LOCALE("Tracker Question"));
	} else {
		TRESPASS();
	}
}


void
FSDialogWindow::SetContext(TFSContext &context, TFSContext::interaction iact) {
	BAutolock l(this);
	
	ASSERT(mMainView.mDialogView == 0);
	mMainView.SetDialogView(new TFSContextDialogView(context, iact));
	mMainView.SetRetryButtonEnabled(mContext.IsAnswerPossible(TFSContext::fRetryOperation));
}

void
FSDialogWindow::SetContext(TFSContext &context, status_t rc) {
	BAutolock l(this);

	ASSERT(mMainView.mDialogView == 0);
	mMainView.SetDialogView(new TFSContextDialogView(context, rc));
	mMainView.SetRetryButtonEnabled(mContext.IsAnswerPossible(TFSContext::fRetryOperation));
}

void
FSDialogWindow::Go() {
	Minimize(false);
	Show();
}

void
FSDialogWindow::MessageReceived(BMessage *msg) {

	switch (msg -> what) {
		case kRetry:
			mReply = TFSContext::kRetryOperation;
			Quit();
			break;
			
		case kTextControl: {
			BControl *ptr;
			if (msg -> FindPointer("source", reinterpret_cast<void **>(&ptr)) == B_OK) {
				BTextControl *source = assert_cast<BTextControl *>(ptr);
				mTextControlText = source -> Text();
			}
			break;
		}
		case kDontAskKey:
			mMainView.SwitchDontAsk();
			break;
			
		case kCancel:
			Cancel();
			Quit();
			break;

		case kDefaultAnswerCB: {
			BCheckBox *box;
			if (msg -> FindPointer("source", reinterpret_cast<void **>(&box)) == B_OK)
				*mDefaultAnswerPtr = (box -> Value() == B_CONTROL_ON);
			break;
		}
		case kAnswerSelected: {
			int32 cmd;
			if (msg -> FindInt32("command", &cmd) != B_OK)
				break;
			
			mReply = static_cast<TFSContext::command>(cmd);
			mMainView.AnswerSelectionChanged(mReply);
			break;
		}
		case kContinue:
			if (HasReply())
				Quit();
			break;

		default:
			inherited::MessageReceived(msg);
	}
}


void
FSDialogWindow::Pack(bool deep) {

	DisableUpdates();
//	BeginViewTransaction();	// the more/less gadget is drawn bad with it
	
//	mMainView.MoveTo(0, 0);
	mMainView.Pack(deep);
	BRect rect = mMainView.Bounds();
	ResizeTo(rect.right - 1, rect.bottom - 1);
	
	if (deep) {
		if (sWindowLeftTop.x == -1) {
			rect = Bounds();
			sWindowLeftTop = BAlert::AlertPosition(rect.right, rect.bottom);
		}
		MoveTo(sWindowLeftTop);
	}

//	EndViewTransaction();
	EnableUpdates();
}


void
FSDialogWindow::Quit() {
	
	*mReplyPtr = mReply;
	*mContainer = 0;

	if (mReply == TFSContext::kSuppliedNewNameForSource) {
		ASSERT(mTextControlText != 0);
		ASSERT(mContext.SourceNewName() != 0);
		strncpy(mContext.SourceNewName(), mTextControlText, B_FILE_NAME_LENGTH - 1);
		mContext.SourceNewName()[B_FILE_NAME_LENGTH - 1] = 0;
	} else if (mReply == TFSContext::kSuppliedNewNameForTarget) {
		ASSERT(mTextControlText != 0);
		ASSERT(mContext.TargetNewName() != 0);
		strncpy(mContext.TargetNewName(), mTextControlText, B_FILE_NAME_LENGTH - 1);
		mContext.TargetNewName()[B_FILE_NAME_LENGTH - 1] = 0;
	}

	mContext.HardResume();
	
	inherited::Quit();
}

void
FSDialogWindow::Cancel() {
	mReply = TFSContext::kCancel;
	mContext.Cancel();
}

//void
//FSDialogWindow::DispatchMessage(BMessage *msg, BHandler *handler) {
//	if (msg -> what == B_KEY_DOWN) {
//		int32 key = msg -> FindInt32("key");
//		if (key == B_ESCAPE)
//			Quit();
//	}
//
//	inherited::DispatchMessage(msg, handler);
//}

bool
FSDialogWindow::QuitRequested() {
	Cancel();
	return true;
}






FSDialogWindow::MainView::MainView() : inherited(BRect(), "MainView", B_FOLLOW_NONE, B_WILL_DRAW),
										mContinueButton(BRect(), "ContinueButton", LOCALE("Continue"), new BMessage(kContinue),
											B_FOLLOW_NONE, B_WILL_DRAW),
										mCancelButton(BRect(), "CancelButton", LOCALE("Stop"), new BMessage(kCancel)),
										mRetryButton(BRect(), "RetryButton", LOCALE("Retry"), new BMessage(kRetry)),
										mDialogView(0) {

	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	AddChild(&mCancelButton);
	AddChild(&mRetryButton);
	AddChild(&mContinueButton);
}

FSDialogWindow::MainView::~MainView() {
	mContinueButton.RemoveSelf();
	mCancelButton.RemoveSelf();
	mRetryButton.RemoveSelf();
}

void
FSDialogWindow::MainView::AllAttached() {
	inherited::AllAttached();

	mContinueButton.ResizeToPreferred();
	mContinueButton.SetEnabled(false);
	mContinueButton.MakeDefault(true);
	mCancelButton.ResizeToPreferred();
	mRetryButton.ResizeToPreferred();
}


void
FSDialogWindow::MainView::Draw(BRect iRect) {
	
	BRect rect = Bounds();
	
	rect.right = kColoredRectWidth;
	SetHighColor(kColoredRectColor);
	if (iRect.Intersects(rect)) {
		FillRect(rect);
		SetHighColor(tint_color(kColoredRectColor, B_DARKEN_4_TINT));
		StrokeLine(rect.RightTop(), rect.RightBottom());
	}
}

void
FSDialogWindow::MainView::SetDialogView(DialogView *view) {

	ASSERT(mDialogView == 0);
	ASSERT(Looper() -> IsLocked());
	
	mDialogView = view;
	AddChild(view);
}

void
FSDialogWindow::MainView::Pack(bool deep) {
	BRect rect(0, 0, 0, 0);

	float tw, th = 0;
	
	tw = mContinueButton.Bounds().right + mCancelButton.Bounds().right + mRetryButton.Bounds().right;
	tw *= 1.3;
	tw += kColoredRectWidth;
	
	if (mDialogView) {
		mDialogView -> MoveTo(kColoredRectWidth + 1, 0);
		if (mDialogView -> Bounds().Width() < tw)
			mDialogView -> ResizeTo(tw, 0);
		mDialogView -> Pack(deep);
		rect = mDialogView -> Frame();
	}

	if (tw < rect.right)
		tw = rect.right;
	th += rect.bottom;
	
	mContinueButton.ResizeToPreferred();

	BRect buttonrect = mContinueButton.Bounds();
	
	th += buttonrect.bottom * 1.2;
	buttonrect.right *= 3;				// at least 3 times the button width
	if (tw < buttonrect.right)
		tw = buttonrect.right;

	rect.InsetBy(rect.Width() * 0.07, 0);

	mCancelButton.MoveTo(	rect.left,
							rect.bottom + mCancelButton.Bounds().bottom * 0.1);
	
	mContinueButton.MakeDefault(false);
	buttonrect = mContinueButton.Bounds();
	mContinueButton.MoveTo(	rect.right - buttonrect.right,
							rect.bottom + buttonrect.bottom * 0.1);
	mContinueButton.MakeDefault(true);

	buttonrect = mRetryButton.Bounds();
	mRetryButton.MoveTo(mContinueButton.Frame().left - buttonrect.right * 1.2,
						rect.bottom + buttonrect.bottom * 0.1);

	ResizeTo(tw, th);
}



FSDialogWindow::TFSContextDialogView::TFSContextDialogView(TFSContext &context, TFSContext::interaction iact) :
											inherited(),
											mContext(context), mType(kInteraction),
											mRadioButtonView(this),
											mButtonBox(),
											mDefaultAnswerCB(),
											mTextView(),
											mTextControl() {
	mInteractionCode = iact;
	
	initialize();
}

FSDialogWindow::TFSContextDialogView::TFSContextDialogView(TFSContext &context, status_t rc) :
											inherited(),
											mContext(context), mType(kError),
											mRadioButtonView(this),
											mButtonBox(),
											mDefaultAnswerCB(),
											mTextView(),
											mTextControl() {
	mErrorCode = rc;

	initialize();
}

void
FSDialogWindow::TFSContextDialogView::initialize() {
	AddChild(&mTextView);
	AddChild(&mButtonBox);
	mButtonBox.AddChild(&mRadioButtonView);
	AddChild(&mTextControl);
	AddChild(&mDefaultAnswerCB);
	
	mTextControl.Hide();

	mTextView.MakeEditable(false);
	mTextView.MakeSelectable(false);
	mTextView.SetStylable(true);

	if (mType == kInteraction  &&  mContext.IsExtendedInteraction(mInteractionCode) == false)
		mDefaultAnswerCB.Hide();

	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	SetInfoText();
}

FSDialogWindow::TFSContextDialogView::~TFSContextDialogView() {
	mRadioButtonView	.RemoveSelf();
	mButtonBox			.RemoveSelf();
	mDefaultAnswerCB	.RemoveSelf();
	mTextView			.RemoveSelf();
	mTextControl		.RemoveSelf();
}


void
FSDialogWindow::TFSContextDialogView::SetInfoText() {
	static const int32 kMaxRunStructs = 30;
	
	// Text styles
	
	struct InfoTextStyle {
		const BFont		**font;
		rgb_color		color;
	};
	
	static const InfoTextStyle sDefault			= { &be_plain_font,		{0, 0, 0, 0}};
//	static const InfoTextStyle sDefaultBold		= { &be_bold_font,		{0, 0, 0, 0}};
	static const InfoTextStyle sStatic			= { &be_plain_font,		{20, 20, 20, 0}};
	static const InfoTextStyle sRootOperation	= { &be_bold_font,		{0, 100, 50, 0}};
	static const InfoTextStyle sEntry			= { &be_bold_font,		{0, 50, 100, 0}};
	static const InfoTextStyle sError			= { &be_bold_font,		{100, 0, 0, 0}};
	static const InfoTextStyle sInteraction		= { &be_bold_font,		{0, 80, 0, 0}};
	static const InfoTextStyle sCurrentOperation= { &be_bold_font,		{0, 0, 100, 0}};
	
#define SET_STYLE(x)							\
	run = &array -> runs[i];					\
	run -> font 	= *s##x.font;				\
	run -> offset	= str.Length();				\
	run -> color	= s##x.color;				\
	++i;										\

// !!! only after a SET_STYLE, otherwise it will set the previous style !!!
#define SCALE_FONT(sizeprop)					\
	run -> font.SetSize(run -> font.Size() * sizeprop);
#define SET_SHEAR								\
	run -> font.SetShear(100);					
//	run -> font.SetSpacing(B_CHAR_SPACING);

	// The function
	
	auto_ptr<text_run_array> array(reinterpret_cast<text_run_array *>(
									new char[sizeof(text_run_array) + sizeof(text_run) * kMaxRunStructs])); 
	BString str;
	int32 i = 0;
	register text_run *run;

	SET_STYLE(Static);
//	SET_SHEAR;
	SCALE_FONT(1.0);
	str += LOCALE("While");
	str += "\n";
	
	SET_STYLE(RootOperation);
	str += mContext.AsString(mContext.RootOperation());
	
	if (mContext.HasTargetDir(mContext.RootOperation())) {
		SET_STYLE(Default);
		
		str += " (";
		str += LOCALE(mContext.TargetDirPrefix(mContext.RootOperation()));
		str += " ";
		
		BPath path;
		mContext.GetRootTargetDir(path);
		str += path.Path();
		str += ")\n";
	} else
		str += "\n";		

	SET_STYLE(Default);
	SCALE_FONT(0.5);
	str += "\n";

	if (mType == kError  ||  mContext.IsExtendedInteraction(mInteractionCode) == true) {
		const EntryRef &ref(*mContext.CurrentEntry());
		BEntry entry(ref);
		BPath path;
		entry.GetPath(&path);

		SET_STYLE(Static);
		SCALE_FONT(1.0);
//		SET_SHEAR;
		
		if (entry.IsFile())
			str += LOCALE("The file ");
		else if (entry.IsSymLink())
			str += LOCALE("The link ");
		else if (entry.IsDirectory())
			str += LOCALE("The folder ");
		else
			TRESPASS();

		SET_STYLE(Default);
		SCALE_FONT(0.9);
		str += "\n";

		SET_STYLE(Entry);
		str += mContext.CurrentEntryName();
		
		SET_STYLE(Default);
		SCALE_FONT(0.9);
		str += "  (";
		str += LOCALE("from");
		str += " ";
		
		{
			BString tmpstr(path.Path());
			tmpstr.RemoveLast(path.Leaf());
			str += tmpstr;
			str += ")\n";
		}
	
		SET_STYLE(Default);
		SCALE_FONT(0.5);
		str += "\n";

		if (mContext.OperationStackSize() > 1) {
			SET_STYLE(Static);
//			SET_SHEAR;
			str += LOCALE("Current operation");
			str += "\n";
		
			SET_STYLE(CurrentOperation);
			str += mContext.AsString(mContext.CurrentOperation());
		
			SET_STYLE(Default);
			SCALE_FONT(0.9);

#if FS_SHOW_OPERATION_STACK
			str += " (";
			str += mContext.OperationStackAsString();
			str += ")\n";
#endif
			SET_STYLE(Default);
			SCALE_FONT(0.5);
			str += "\n";
		}
	}
	
	SET_STYLE(Default);
//	SET_SHEAR;
	str += LOCALE("Information");
	str += "\n";
	
	if (mType == kError) {
	
//		SET_STYLE(Default);
//		SET_SHEAR;
//		str += "The returned error code:\n";
		
		SET_STYLE(Error);
		SCALE_FONT(1.2);
		str += strerror(mErrorCode);

#if FS_SHOW_ERROR_CODE
		str += " (";
		char buf[128];
		sprintf(buf, "0x%lx", (int32)mErrorCode);
		str += buf;
		str += ")";
#endif
		
	} else if (mType == kInteraction) {
		
		SET_STYLE(Interaction);
		SCALE_FONT(1.0);
		switch ((int)mInteractionCode) {
			
			case TFSContext::kFileAlreadyExists:
			case TFSContext::kDirectoryAlreadyExists:
			case TFSContext::kTargetAlreadyExists: {
				
				str += LOCALE(mContext.AsString(mInteractionCode));
				
				if (mContext.CurrentEntry() != 0  &&  mContext.TargetEntry() != 0) {
					BEntry source(*mContext.CurrentEntry());
					BEntry target(*mContext.TargetEntry());
					if (source.InitCheck() == B_OK  &&  target.InitCheck() == B_OK) {
						time_t source_mod, target_mod;
						source.GetModificationTime(&source_mod);
						target.GetModificationTime(&target_mod);
						str += " (";
						str += LOCALE("source");
						str += " ";
						if (source_mod > target_mod)
							str += LOCALE("is newer");
						else if (source_mod < target_mod)
							str += LOCALE("is older");
						else
							str += LOCALE("was modified at the same time as target");
						if (source.IsFile()  &&  target.IsFile()) {
							off_t source_size, target_size;
							BFile source_node(&source, B_READ_ONLY), target_node(&target, B_READ_ONLY);
							if (source_node.InitCheck() == B_OK  &&  target_node.InitCheck() == B_OK) {
								source_node.GetSize(&source_size);
								target_node.GetSize(&target_size);
								str += " ";
								str += LOCALE("and");
								str += " ";
								if (source_size > target_size)
									str += LOCALE("bigger");
								else if (source_size < target_size)
									str += LOCALE("smaller");
								else
									str += LOCALE("has the same size");
							}
						}
						str += ")";
					}
				} else {
					TRESPASS();
				}
				break;				
			}
			case TFSContext::kNotEnoughFreeSpace:
			case TFSContext::kNotEnoughFreeSpaceForThisFile: {
				
				str += LOCALE(mContext.AsString(mInteractionCode));

				// XXX todo: make it async and cancel if takes too long, or only set when done
				TFSContext::ProgressInfo info;
				TFSContext *context = new TFSContext();
				context -> SetInteractive(false);
				BDirectory dir;
				TFSContext::GetTrashDir(dir, mContext.TargetDevice());
				auto_ptr<DirEntryIterator> dei(new DirEntryIterator());
				dei -> SetTo(dir);
				
				context -> CalculateItemsAndSize(*dei.get(), info);
				if (info.IsTotalEnabled()) {
					if (info.mTotalSize != 0) {
						char buf[128];
						TFSContext::GetSizeString(buf, info.mTotalSize);
						char buffer[1024];
						sprintf(buffer, LOCALE("(%s in Trash)"), buf);
						str += buffer;
					} else {
						str += LOCALE("(The Trash is empty)");
					}
				}
				break;
			}
			
			case TFSContext::kAboutToDelete: {
				const char *format = LOCALE("Are you sure you want to delete "
					"%s? This operation cannot be reverted!");
				
				char name[1024];
				if (mContext.EntryList().size() == 1)
					sprintf(name, "\"%s\"", mContext.EntryList().front().Name());
				else
					sprintf(name, LOCALE("the selected entries"));
				
				char buffer[2048];
				sprintf(buffer, format, name);
				str += buffer;
				break;	
			}
			
			default:
				str += LOCALE(mContext.AsString(mInteractionCode));
				break;
		}		
	} else {
		TRESPASS();
	}

//	SET_STYLE(Default);
//	SET_SHEAR;
//	str += "\nMake your choice:";

	ASSERT(i < kMaxRunStructs);

	array -> count = i;
	mTextView.SetText(str.String(), array.get());
#undef SET_STYLE
#undef SCALE_FONT
}

void
FSDialogWindow::TFSContextDialogView::AllAttached() {
	inherited::AllAttached();

	mDefaultAnswerCB.ResizeToPreferred();
	mDefaultAnswerCB.SetEnabled(false);
	
	mRadioButtonView.MakeFocus();
	
	mTextView.SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	if (mType == kInteraction) {
		const TFSContext::command *cmds = mContext.PossibleAnswersFor(mInteractionCode);
		if (cmds[0] != TFSContext::kInvalidCommand) {
			for (int32 i = 0;  i < TFSContext::kMaxAnswersForInteraction - 1;  ++i) {
				if (cmds[i] == TFSContext::kInvalidCommand)
					break;
				
				switch (cmds[i]) {
					case TFSContext::kSuppliedNewNameForSource:
					case TFSContext::kSuppliedNewNameForTarget:
							if (mContext.SourceNewName() == 0  ||  mContext.TargetNewName() == 0)
								continue;	// skip these possible answers if no buffer is supplied before Interaction() call
						break;
				
					case TFSContext::kReplaceIfNewer: {
// XXX: insert proper skipping code here to hide this answer when teh root operation is creating dir, or creating link and the name is occupied
// (to create a unique name by default, but is that a good idea?)
//						continue;

						#if 0	// this is not a good idea to hide it, because then the user can't select this answer and check the default
								// answer checkbox. (only if s/he skips until the first newer source)

							// skip this answer if the files were modified the same time
							BEntry source_entry(*mContext.CurrentEntry(), false);
							BEntry target_entry(*mContext.TargetEntry());
							time_t source_time, target_time;
							if (source_entry.GetModificationTime(&source_time) == B_OK  &&
												target_entry.GetModificationTime(&target_time) == B_OK  &&
												source_time <= target_time)
								continue;
						#endif
						
						break;
					}
				
					case TFSContext::kContinueFile: {
							if (mContext.TargetEntry() == 0)
								continue;
							
							BFile target_file(mContext.TargetEntry(), B_READ_ONLY);
							BFile source_file(*mContext.CurrentEntry(), B_READ_ONLY);
							if (target_file.InitCheck() != B_OK  ||  source_file.InitCheck() != B_OK)
								continue;
							
							off_t target_size, source_size;
							source_file.GetSize(&source_size);
							target_file.GetSize(&target_size);
							
							if (source_size <= target_size)
								continue;
								
						break;
					}
					
					case TFSContext::kSkipEntry:
							if (mContext.IsAnswerPossible(TFSContext::fSkipEntry) == false)
								continue;
						break;

					case TFSContext::kSkipOperation:
							if (mContext.IsAnswerPossible(TFSContext::fSkipOperation) == false)
								continue;
						break;

					case TFSContext::kRetryEntry:
							if (mContext.IsAnswerPossible(TFSContext::fRetryEntry) == false)
								continue;
						break;
						
					case TFSContext::kSkipDirectory:
							if (mContext.IsAnswerPossible(TFSContext::fSkipDirectory) == false)
								continue;
						break;

					case TFSContext::kIgnore:
							if (mContext.IsAnswerPossible(TFSContext::fIgnore) == false)
								continue;
						break;
						
					default:
						break;
				}
				mRadioButtonView.AddChild(new CustomRadioButton(mContext, cmds[i]));
			}
		}
		
		if (mRadioButtonView.CountChildren() == 0)
			mButtonBox.Hide();
			
	} else {	// then this is an error window
	
		if (mContext.IsAnswerPossible(TFSContext::fSkipEntry))
			mRadioButtonView.AddChild(new CustomRadioButton(mContext, TFSContext::kSkipEntry));
		if (mContext.IsAnswerPossible(TFSContext::fSkipOperation))
			mRadioButtonView.AddChild(new CustomRadioButton(mContext, TFSContext::kSkipOperation));
		if (mContext.IsAnswerPossible(TFSContext::fRetryEntry))
			mRadioButtonView.AddChild(new CustomRadioButton(mContext, TFSContext::kRetryEntry));
		if (mContext.IsAnswerPossible(TFSContext::fSkipDirectory))
			mRadioButtonView.AddChild(new CustomRadioButton(mContext, TFSContext::kSkipDirectory));
		if (mContext.IsAnswerPossible(TFSContext::fIgnore))
			mRadioButtonView.AddChild(new CustomRadioButton(mContext, TFSContext::kIgnore));
	}

	assert_cast<FSDialogWindow *>(Window()) -> Pack(true);

	// autoselect the first item
	CustomRadioButton *button = assert_cast<CustomRadioButton *>(mRadioButtonView.ChildAt(0));
	if (button) {
		button->SetValue(B_CONTROL_ON);
		button->Invoke();
	}
}


void
FSDialogWindow::TFSContextDialogView::Pack(bool deep) {

	float tw = Bounds().right;
	BRect rect;

	mButtonBox.ResizeToPreferred();
	rect = mButtonBox.Bounds();

	rect.right += 10;
	if (tw < rect.right)
		tw = rect.right;

	if (deep) {
		mTextView.MoveTo(5, 5);
		mTextView.ResizeToPreferred();
		rect = mTextView.Bounds();

		rect.right += 10;
		if (tw < rect.right)
			tw = rect.right;
	
		mButtonBox.MoveTo(5, mTextView.Frame().bottom + 6);
	} else {
		mButtonBox.Invalidate();
	}
	
	CustomRadioButton *button;
	bool isReallyHidden = true;

	for (int i = 0;  (button = assert_cast<CustomRadioButton *>(mRadioButtonView.ChildAt(i))) != 0;  ++i) {
		if (button -> Value() == B_CONTROL_ON) {
			if (button -> mCommand == TFSContext::kSuppliedNewNameForSource ||
				button -> mCommand == TFSContext::kSuppliedNewNameForTarget) {

				if (mTextControl.IsHidden())
					mTextControl.Show();

				mTextControl.ResizeToPreferred();
				mTextControl.ResizeTo(tw - 10, mTextControl.Bounds().bottom);
				mTextControl.MoveTo(5, mButtonBox.Frame().bottom + 4);
				
				isReallyHidden = false;
			} else {
				if (mTextControl.IsHidden() == false) {
					mTextControl.Hide();
					isReallyHidden = true;
				}
			}
		}
	}

	mDefaultAnswerCB.ResizeToPreferred();
	mDefaultAnswerCB.MoveTo((tw - mDefaultAnswerCB.Bounds().right) / 2,
							(isReallyHidden) ? mButtonBox.Frame().bottom + 4 :
														mTextControl.Frame().bottom + 4);
	
	if (tw - 10  >  mTextView.Bounds().right) {
		mTextView.ResizeTo(tw - 10, mTextView.Bounds().bottom);
		mTextView.SetTextRect(mTextView.Bounds());
	}
	if (tw - 10  >  mButtonBox.Bounds().right)
		mButtonBox.ResizeTo(tw - 10, mButtonBox.Bounds().bottom);
		
	ResizeTo(tw, mDefaultAnswerCB.Frame().bottom);
}

//void
//FSDialogWindow::TFSContextDialogView::Draw(BRect rect) {
//}

void
FSDialogWindow::TFSContextDialogView::CustomTextView::ResizeToPreferred() {

	BRegion region;
	BRect frame;
	float width = BScreen(Window()).Frame().right * 0.33;
	
	if (width > kMaxTextViewWidth)
		width = kMaxTextViewWidth;
	
	if (Bounds().Width() < width) {
		ResizeTo(width, 200);
		SetTextRect(Bounds());
	}

	GetTextRegion(0, TextLength(), &region);
	frame = region.Frame();
	if (frame.IsValid())
		ResizeTo(width, frame.Height());
	else
		ResizeTo(width, kTextViewHeight);

	SetTextRect(Bounds());

	GetTextRegion(0, TextLength(), &region);
	frame = region.Frame();
	if (frame.IsValid())
		ResizeTo(frame.right, frame.bottom);

	SetTextRect(Bounds());
	
	return;
}

FSDialogWindow::TFSContextDialogView::CustomRadioButton::CustomRadioButton(TFSContext &context, TFSContext::command cmd) : 
															inherited(BRect(), "AnswerList", "", 0), mCommand(cmd), mLastClickTime(0) {

	switch (static_cast<int32>(mCommand)) {
		case TFSContext::kSkipOperation: {
			
			BString str(LOCALE("Skip operation"));
			if (context.SkipOperationTarget() != context.kInvalidOperation) {
				str += " (";
				str += context.AsString(context.SkipOperationTarget());
				str += ")";
			}
			
			SetLabel(str.String());
			break;
		}
		
		default:
			SetLabel(context.AsString(mCommand));
	}

	BMessage *msg = new BMessage(kAnswerSelected);
	msg -> AddInt32("command", mCommand);
	SetMessage(msg);
}

void
FSDialogWindow::TFSContextDialogView::CustomRadioButton::MouseDown(BPoint point) {
	bigtime_t clickspeed;
	get_click_speed(&clickspeed);
	
	if (system_time() - mLastClickTime <= clickspeed) {
		Invoke();
		Window() -> PostMessage(kContinue);
	}
	
	mLastClickTime = system_time();
	
	inherited::MouseDown(point);
}


void
FSDialogWindow::TFSContextDialogView::CustomBBox::DrawAfterChildren(BRect rect) {
							
	TFSContextDialogView::RadioButtonView *bview = GetRadioButtonView();
	if (bview -> HasExtendedButton()  &&  bview -> HasNonExtendedButton()) {
		if (mExpandButtonRect.Intersects(rect)) {
			const char *string = (bview -> IsExpanded()) ? LOCALE("Less"B_UTF8_ELLIPSIS) : LOCALE("More"B_UTF8_ELLIPSIS);
		
			BFont font = be_plain_font;
			font.SetSize(11);
			font.SetShear(100);
			SetFont(&font);
			
			font_height fh;
			font.GetHeight(&fh);
			
			BRect bounds = Bounds();
			BPoint pos;
			float string_width = StringWidth(string);
//			pos.y = bounds.bottom - fh.descent;	// should be this one, but looks ugly
			pos.y = bounds.bottom - 1;
			pos.x = (bounds.right - string_width) * 0.95;
			SetLowColor(ViewColor());
			SetHighColor(189, 0, 0);

			mExpandButtonRect.left = pos.x - 4;
			mExpandButtonRect.top = pos.y - fh.ascent;
			mExpandButtonRect.right = pos.x + string_width + 2;
			mExpandButtonRect.bottom = bounds.bottom;
			FillRect(mExpandButtonRect, B_SOLID_LOW);
			DrawString(string, pos);
		}
	}
}

void
FSDialogWindow::TFSContextDialogView::CustomBBox::FrameResized(float width, float height) {
	mExpandButtonRect.left = mExpandButtonRect.top = 0;
	mExpandButtonRect.right = mExpandButtonRect.bottom = 10000;
	inherited::FrameResized(width, height);
}

void
FSDialogWindow::TFSContextDialogView::CustomBBox::MouseUp(BPoint point) {

	BRect rect(mExpandButtonRect);
	rect.InsetBy(-rect.Width() * 0.25, -rect.Height() * 0.25);	// make it easier to push
	
	if (rect.Contains(point))
		GetRadioButtonView() -> SwitchExpanded();
}

void
FSDialogWindow::TFSContextDialogView::CustomRadioButton::KeyDown(const char *bytes, int32 count) {
	if (count == 1) {
		switch (*bytes) {
			case B_DOWN_ARROW:
			case B_UP_ARROW:
			case B_RIGHT_ARROW:
			case B_LEFT_ARROW:
				assert_cast<RadioButtonView *>(Parent()) -> KeyDown(bytes, count);
			default:
				inherited::KeyDown(bytes, count);
		}
	} else {
		inherited::KeyDown(bytes, count);
	}
}

void
FSDialogWindow::TFSContextDialogView::RadioButtonView::MessageReceived(BMessage *msg) {
	switch (msg -> what) {
		case B_MOUSE_WHEEL_CHANGED: {
			float y = 0;
			msg -> FindFloat("be:wheel_delta_y", &y);

		#if 0
			BMessageQueue *queue = Window() -> MessageQueue();	// empty the message queue
			BMessage *msg;
			while ((msg = queue -> FindMessage(B_MOUSE_WHEEL_CHANGED, 0)) != 0)
				queue -> RemoveMessage(msg);
		#endif
			
			char c = y > 0 ? B_DOWN_ARROW : B_UP_ARROW;
			KeyDown(&c, 1);
			break;
		}
		default:
			inherited::MessageReceived(msg);
	}
}

void
FSDialogWindow::TFSContextDialogView::RadioButtonView::KeyDown(const char *bytes, int32 count) {
	if (count == 1  &&  CountChildren() > 0) {

		CustomRadioButton *active_button = ActiveButton();
		
		char key  = *bytes;

		if (key == B_PAGE_DOWN) {
			active_button = 0;
			key = B_UP_ARROW;
		} else if (key == B_PAGE_UP) {
			active_button = 0;
			key = B_DOWN_ARROW;
		}
		
		if (key == B_DOWN_ARROW  ||  key == B_UP_ARROW) {
			bool down = key == B_DOWN_ARROW;
			if (active_button == 0) {
				active_button = assert_cast<CustomRadioButton *>(ChildAt(down ? 0 : CountChildren() - 1));
				while (active_button -> IsHidden()) {
					active_button = dynamic_cast<CustomRadioButton *>(down ?	active_button -> NextSibling() :
																			active_button -> PreviousSibling());
				}
				if (active_button) {
					active_button -> SetValue(B_CONTROL_ON);
					active_button -> Invoke();
				}
			} else  {
				CustomRadioButton *next_button = active_button;
				for(;;) {
					if ((next_button = dynamic_cast<CustomRadioButton *>(down ?	next_button -> NextSibling() :
																				next_button -> PreviousSibling())) == 0)
						break;
					if (next_button -> IsHidden() == false)
						break;
				}
				if (next_button) {
					active_button -> SetValue(B_CONTROL_OFF);
					next_button -> SetValue(B_CONTROL_ON);
					next_button -> Invoke();
				} else if (down  &&  mHasExtendedButton  &&  mExpanded == false) {
					SwitchExpanded();
				}
			}
		} else if (key == B_RIGHT_ARROW) {
			SetExpanded(true);
		} else if (key == B_LEFT_ARROW) {
			SetExpanded(false);
		} else {
			inherited::KeyDown(bytes, count);
		}
	} else {
		inherited::KeyDown(bytes, count);
	}
}

FSDialogWindow::TFSContextDialogView::CustomRadioButton *
FSDialogWindow::TFSContextDialogView::RadioButtonView::ActiveButton() {
	int32 count = CountChildren();
	for (int32 i = 0;  i < count;  ++i) {
		CustomRadioButton *button = dynamic_cast<CustomRadioButton *>(ChildAt(i));
		if (button  &&  button -> Value() == B_CONTROL_ON)
			return button;
	}
	return 0;
}

void
FSDialogWindow::TFSContextDialogView::RadioButtonView::SetExpanded(bool state) {
	if (mExpanded != state) {
		mExpanded = state;
		assert_cast<FSDialogWindow *>(Window()) -> Pack(false);
		if (state == false) {
			CustomRadioButton *active_button = ActiveButton();
			if (active_button  &&  active_button -> IsHidden()) {
				char c = B_UP_ARROW;
				KeyDown(&c, 1);
			}
		}
	}
}

void
FSDialogWindow::TFSContextDialogView::RadioButtonView::AddChild(BView *view) {
	CustomRadioButton *button = assert_cast<CustomRadioButton *>(view);
	
	inherited::AddChild(view);

	if (mParent -> mType == kInteraction  &&  TFSContext::IsExtendedCommand(mParent -> InteractionCode(), button -> mCommand)) {
		if (mExpanded == false)
			view -> Hide();
		mHasExtendedButton = true;
	} else {
		mHasNonExtendedButton = true;
	}
}

void
FSDialogWindow::TFSContextDialogView::RadioButtonView::ResizeToPreferred() {
	ASSERT(Window() != 0);

	float w = Bounds().Width(), h = 0;

	for (int32 i = 0;  i < CountChildren();  ++i) {
		CustomRadioButton *button = assert_cast<CustomRadioButton *>(ChildAt(i));
		
		float bw, bh;

		if (mParent -> mType != kInteraction  ||  TFSContext::IsExtendedCommand(mParent -> InteractionCode(), button -> mCommand) == false
				||  mExpanded  ||  mHasNonExtendedButton == false) {
					
			if (Window() -> IsHidden() == false  &&  button -> IsHidden())
				button -> Show();
			button -> GetPreferredSize(&bw, &bh);
			button -> ResizeTo(w - kRadioButtonLeftOffset - 60, bh);	// -60 is to simpllify the handling of the More.../Less... button
			button -> MoveTo(kRadioButtonLeftOffset, h);
			h += ceilf(bh) + 1;
		} else {
			
			if (Window() -> IsHidden() == false  &&  button -> IsHidden() == false)
				button -> Hide();
		}
	}

	h -= 1;
	
	ResizeTo((w > kRadioButtonViewWidth) ? w : kRadioButtonViewWidth, h);
}

bool
FSDialogWindow::TFSContextDialogView::SelectAnswer(TFSContext::command cmd) {

	CustomRadioButton *button;
	
	for (int i = 0;  (button = assert_cast<CustomRadioButton *>(mRadioButtonView.ChildAt(i))) != 0;  ++i) {
		if (button -> mCommand == cmd) {
			button -> SetValue(B_CONTROL_ON);
			button -> Invoke();
			return true;
		}
	}
	return false;
}

void
FSDialogWindow::TFSContextDialogView::AnswerSelectionChanged(TFSContext::command cmd) {

	FSDialogWindow *win = assert_cast<FSDialogWindow *>(Window());

	assert_cast<MainView *>(Parent()) -> SetContinueEnabled(true);

	mDefaultAnswerCB.SetEnabled(mContext.IsDefaultableCommand(cmd));

	if (cmd == TFSContext::kSuppliedNewNameForSource  ||  cmd == TFSContext::kSuppliedNewNameForTarget) {
		if (mTextControl.IsHidden())
			mTextControl.Show();
			
		if (cmd == TFSContext::kSuppliedNewNameForSource)
			win -> SetTextControlText(win -> Context().SourceNewName());
		else if (cmd == TFSContext::kSuppliedNewNameForTarget)
			win -> SetTextControlText(win -> Context().TargetNewName());
			
		win -> Pack(false);
	} else {
		if (mTextControl.IsHidden() == false)
			mTextControl.Hide();
		win -> Pack(false);
	}
}

}	// namespace BPrivate
