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

#include "TFSContext.h"
#include "FSUtils.h"
#include "Attributes.h"
#include "Commands.h"
#include "Tracker.h"
#include "ThreadMagic.h"
#include "Bitmaps.h"
#include "FSStatusWindow.h"
#include "OverrideAlert.h"
#include "FSDialogWindow.h"

#include <Application.h>
#include <Autolock.h>
#include <fs_attr.h>
#include <Roster.h>
#include <Alert.h>
#include <Beep.h>
#include <errno.h>
#include <math.h>

// Various info
//	- ContainerView does pulse managemant on it's own by a BMessageRunner. It calls StatusView::Pulse which
//	  calls ContainerView::Pulse. This is required to be independent from the holding window, that might
//    be the desktop window also if replaicand bug fixed in beos.

// Current limitations
//	- Does not yet checks for well known entries
//	- Estimated time is redrawn when window is resized (Draw() seems to be called with Bounds() instead of the rect to be updated)
//	- See list in the beginning of FSContext.cpp

// TODO

// BUGS
//  - StatusView redraw at the right, below the buttons is bad on heavy system load. Dunno why.

#define		FS_PRINT_ESTIMATION_INFO		0

namespace BPrivate {

using namespace fs;

void
TFSContext::initialize() {
	mDialogWindow	= 0;
	
	mConnectedToStatusWindow	= true;
	mInteractive				= true;
	mProgressInfoEnabled		= true;
	mOperationBegun				= false;
	mWasOverheadSWRunning		= false;
	mOperationStringDirty		= true;	

	mPause				= false;
	mActuallyPaused		= false;
	mAutoPaused		= false;
	mShouldAutoPause	= true;
	mCancel			= false;
	mSkipOperation		= false;
	mSkipDirectory		= false;
	mSkipEntry			= false;
	
	mWorkingThread		= 0;
	mPreviousTimeGuess	= 0;
	mStartTime			= -1;
}

TFSContext::TFSContext() :
				mElapsedStopWatch("", true),
				mOverheadStopWatch("", true),
				mEntryIterator(mEntryList) {

	initialize();
}

// these ctors collect a list of entries from the given parameters
TFSContext::TFSContext(BObjectList<entry_ref> *inlist) :
				mElapsedStopWatch("", true),
				mOverheadStopWatch("", true),
				mEntryList(inlist),
				mEntryIterator(mEntryList) {

	initialize();
}

TFSContext::TFSContext(const PoseList &inlist) :
				mElapsedStopWatch("", true),
				mOverheadStopWatch("", true),
				mEntryList(inlist),
				mEntryIterator(mEntryList) {

	initialize();
}

TFSContext::TFSContext(const BMessage &in_msg) :
				mElapsedStopWatch("", true),
				mOverheadStopWatch("", true),
				mEntryList(in_msg),
				mEntryIterator(mEntryList) {

	initialize();
}

TFSContext::TFSContext(const entry_ref &in_ref) :
				mElapsedStopWatch("", true),
				mOverheadStopWatch("", true),
				mEntryList(in_ref),
				mEntryIterator(mEntryList) {
				
	initialize();
}

TFSContext::~TFSContext() {
	if (IsConnectedToStatusWindow())
		gStatusWindow().Remove(*this);
		
	if (mDialogWindow  &&  mDialogWindow -> Lock())
		mDialogWindow -> Quit();
}


status_t
TFSContext::CalculateItemsAndSize(ProgressInfo &ininfo) {
	return inherited::CalculateItemsAndSize(mEntryIterator, ininfo);	// XXX without inherited gcc can't bind the inherited function ?!
}

status_t
TFSContext::CopyTo(BDirectory &target_dir, bool async) {
	return async ?
		inherited::CopyTo(NewIterator(), target_dir, true) :
		inherited::CopyTo(mEntryIterator, target_dir);
}

status_t
TFSContext::Duplicate(bool async) {
	return async ?
		inherited::Duplicate(NewIterator(), true) :
		inherited::Duplicate(mEntryIterator);
}

status_t
TFSContext::CreateLinkTo(BDirectory &target_dir, bool relative, bool async) {
	return async ?
		inherited::CreateLinkTo(NewIterator(), target_dir, relative, true) :
		inherited::CreateLinkTo(mEntryIterator, target_dir, relative);
}

status_t
TFSContext::MoveTo(BDirectory &target_dir, bool async) {
	return async ?
		inherited::MoveTo(NewIterator(), target_dir, true) :
		inherited::MoveTo(mEntryIterator, target_dir);
}

status_t
TFSContext::MoveToTrash(bool async) {
	return async ?
		inherited::MoveToTrashAsync(NewIterator(), true) :
		inherited::MoveToTrash(mEntryIterator);
}

status_t
TFSContext::RestoreFromTrash(bool async) {
	return async ?
		inherited::RestoreFromTrash(NewIterator(), true) :
		inherited::RestoreFromTrash(mEntryIterator);
}

status_t
TFSContext::Remove(bool in_ask_user, bool async) {
	return async ?
		inherited::Remove(NewIterator(), in_ask_user, true) :
		inherited::Remove(mEntryIterator, in_ask_user);
}

void
TFSContext::PreparingOperation() {
	mEntryIterator.Rewind();
	mPointListPos = mPointList.begin();
	mStartTime = system_time();
}

void
TFSContext::OperationBegins() {
	mElapsedStopWatch.Reset();
	mOverheadStopWatch.Reset();
	mOperationBegun = true;
}

int32
TFSContext::EstimatedTimeLeft() {

	static const int32 kMinimalEntryCountForEstimation = 20;

	if (mProgressInfo.IsTotalEnabled() == false  ||  DidOperationBegin() == false  ||
		mElapsedStopWatch.ElapsedTime() < 3000000)
		return 0;

	ASSERT(mProgressInfo.EntryProgress() <= 1		&&  mProgressInfo.EntryProgress() >= 0);
	ASSERT(mProgressInfo.TotalSizeProgress() <= 1	&&  mProgressInfo.TotalSizeProgress() >= 0);

	float max		= (float)(mProgressInfo.mTotalEntryCount * 2);

	float current1	= (float)mProgressInfo.mCurrentEntryCount;
	float weight1 = mProgressInfo.TotalSizeProgress();

	float current2;
	float weight2;

	if (mProgressInfo.IsTotalSizeProgressEnabled()) {
	
		current2 = (float)mProgressInfo.mCurrentSize * mProgressInfo.mTotalEntryCount / mProgressInfo.mTotalSize;
		weight1 = mProgressInfo.TotalSizeProgress();
		weight2 = mProgressInfo.EntryProgress();
		float f = (weight1 + weight2) / 2;
		weight1 /= f;
		weight2 /= f;
	
		weight1 = powf(weight1, 0.4);
		weight2 = powf(weight2, 0.4);
	
		f = (weight1 + weight2) / 2;
		weight1 /= f;
		weight2 /= f;
	
		if (mProgressInfo.mTotalEntryCount < kMinimalEntryCountForEstimation) {	// to few files, don't bother
			current1 = 0;
			weight1 = 0;
			weight2 = 2;
		}
	} else {
		current2 = 0;
		weight2 = 0;
		weight1 = 2;
	}
	
	float time = mElapsedStopWatch.ElapsedTime() / 1000000;
	float guess = time / (current1 * weight1 + current2 * weight2) * max - time;

#if FS_PRINT_ESTIMATION_INFO
	printf("elaps: %.1f,\toverh: %.1f,\tprogr: %.2f, %.2f,\tguess: %.1f * %.1f + %.1f * %.1f  = %.1f/%.1f -> \t%.2f\n",
			(float)(mElapsedStopWatch.ElapsedTime() / 1000000),
			(float)(mOverheadStopWatch.ElapsedTime() / 1000000),
			mProgressInfo.EntryProgress(),
			mProgressInfo.TotalSizeProgress(),
			current1, weight1, current2, weight2,
			current1 * weight1 + current2 * weight2, max, guess);
#endif		

	if (guess > 10000  ||  guess < 0)
		return 0;							// no idea

	if (mPreviousTimeGuess == 0)
		mPreviousTimeGuess = guess;
		
	time = (mPreviousTimeGuess + guess) / 2;
	mPreviousTimeGuess = guess;
	
	return (int32)time;
}

bool
TFSContext::Pause() {

	mShouldAutoPause = false;
	mAutoPaused = false;
	
	if (mPause) {
	
		HardResume();
	} else {
	
		mPause = true;
	}
	
	mOperationStringDirty = true;
	return mPause;
}

void
TFSContext::SoftResume() {

//	ReallocateBuffer(mBufferSizeBackup);	// delme
	
	mElapsedStopWatch.Resume();
	if (IsEffectiveFileCopyRunning() == false)
		mOverheadStopWatch.Resume();

	if (mWorkingThread != 0) {
		mActuallyPaused = false;
		resume_thread(mWorkingThread);
	}

	mOperationStringDirty = true;
}

void
TFSContext::CheckCancel() {

	if (mCancel) {
	
		mCancel = false;
		FS_CONTROL_THROW(kCancel);
	}
	
	if (mSkipOperation) {
		mSkipOperation = false;
		
		if (IsAnswerPossible(fSkipOperation)) {
			FS_CONTROL_THROW(kSkipOperation);
		}
	}
	
	if (mSkipDirectory) {
		mSkipDirectory = false;
		
		if (IsAnswerPossible(fSkipDirectory)) {
			FS_CONTROL_THROW(kSkipDirectory);
		}
	}

	if (mSkipEntry) {
		mSkipEntry = false;
		
		if (IsAnswerPossible(fSkipEntry)) {
			FS_CONTROL_THROW(kSkipEntry);
		}
	}

	if (mPause  &&  IsBufferReleasable() == true) {
	
		mElapsedStopWatch.Suspend();
		mOverheadStopWatch.Suspend();

		ReleaseBuffer();

		mWorkingThread = find_thread(0);
		mActuallyPaused = true;
		mOperationStringDirty = true;
		suspend_thread(mWorkingThread);		// StatusView will call Pause() that will wake the thread up if required

		CheckCancel();						// check again to quickly catch a cancel or skipop if it was blocked by a pause
	} else if (mShouldAutoPause  &&  mAutoPaused == false) {
		// should not check again for autopause if it was resumed once
		
		if (CurrentOperation() == kCalculatingItemsAndSize  ||  RootOperation() != kCopying) {
			if (mProgressInfo.mTotalEntryCount <= 10)
				return;
		}
		
		if (gStatusWindow().ShouldPause(*this)) {
			mPause = true;
			mAutoPaused = true;
			mOperationStringDirty = true;
			
			CheckCancel();		// XXX maybe not usefull here?
		}
	}
}


status_t
TFSContext::SetPoseLocation(ino_t in_dest_dir_inode, BNode &in_dest_node, const BPoint &in_point) {

	PoseInfo poseInfo;
	poseInfo.fInvisible = false;
	poseInfo.fInitedDirectory = in_dest_dir_inode;
	poseInfo.fLocation = in_point;

	status_t rc = in_dest_node.WriteAttr(kAttrPoseInfo, B_RAW_TYPE, 0, &poseInfo, sizeof(poseInfo));

	if (rc == sizeof(poseInfo))
		return B_OK;
	
	return rc;
}

void
TFSContext::InitProgressIndicator() {
	if (mProgressInfoEnabled) {
		mConnectedToStatusWindow = true;
		gStatusWindow().Add(*this);
	}
	
//	snooze(50000000);	// XXX
}


bool
TFSContext::ErrorHandler(status_t in_err) {

	if (mCancel  ||  IsInteractive() == false) {
		FS_CONTROL_THROW(kCancel);
	}

	PauseStopWatch				p1(mElapsedStopWatch);
	PauseStopWatch_ResumeIf 	p2(mOverheadStopWatch, this, &TFSContext::IsEffectiveFileCopyRunning);
	
	command cmd = DefaultErrorAnswer(in_err);

	if (cmd != 0) {

		if		(cmd == kRetryOperation)	return true;
		else if	(cmd == kIgnore)			return false;

		TRESPASS();
		beep();
		FS_CONTROL_THROW(kCancel);
	}

#if 1	// XXX remove when fat attribute bug fixed

	if (CurrentOperation() == kReadingAttribute  &&  in_err == B_ENTRY_NOT_FOUND) {
	
		FS_CONTROL_THROW(kSkipOperation);
	}
#endif

	bool def;

	try {
	
		mDialogWindow = new FSDialogWindow(&cmd, &def, &mDialogWindow, *this, in_err);
		mDialogWindow -> Go();
	
		do {
		
			mPause = true;
			CheckCancel();			// fall asleep until we are resumed by the dialog window
			
		} while (mDialogWindow != 0);
		
	} catch (FSException e) {
	
		if (mDialogWindow != 0  &&  mDialogWindow -> Lock()) {
			mDialogWindow -> Quit();
			mDialogWindow = 0;
		}
		throw;
	}

	if (def  &&  IsDefaultableCommand(cmd))
		SetDefaultErrorAnswer(cmd, in_err);

	if (IsThrowableCommand(cmd)) {
		FS_CONTROL_THROW(cmd);
	}
	
	if (cmd == kRetryOperation)
		return true;
	else if (cmd == kIgnore)
		return false;
	else
		TRESPASS();
	
	return true;
}

FSContext::command
TFSContext::Interaction(interaction in_code) {

	if (mCancel  ||  IsInteractive() == false) {
		FS_CONTROL_THROW(kCancel);
	}

	command cmd = DefaultInteractionAnswer(in_code);
	if (cmd != kInvalidCommand)
		return cmd;

	PauseStopWatch				p1(mElapsedStopWatch);
	PauseStopWatch_ResumeIf 	p2(mOverheadStopWatch, this, &TFSContext::IsEffectiveFileCopyRunning);

	bool def;
	try {

		mDialogWindow = new FSDialogWindow(&cmd, &def, &mDialogWindow, *this, in_code);
		
		if ((cmd = LastSelectedInteractionAnswer(in_code)) != kInvalidCommand)
			mDialogWindow -> SelectAnswer(cmd);
		else
			mDialogWindow -> SelectAnswer(DefaultInteractionAnswer(in_code));

		mDialogWindow -> Go();
	
		do {
		
			mPause = true;
			CheckCancel();			// fall asleep until we are resumed by the dialog window
			
		} while (mDialogWindow != 0);

	} catch (FSException e) {
	
		if (mDialogWindow != 0  &&  mDialogWindow -> Lock()) {
			mDialogWindow -> Quit();
			mDialogWindow = 0;
		}
		throw;
	}
	
	if (def  &&  IsDefaultableCommand(cmd))
		SetDefaultInteractionAnswer(in_code, cmd);
	
	SetLastSelectedInteractionAnswer(in_code, cmd);		// store the given answer to be the default selected next time
	
	if (IsThrowableCommand(cmd)) {
		FS_CONTROL_THROW(cmd);
	}
	
	return cmd;
}

BWindow	*
TFSContext::DialogWindow() const {
	return mDialogWindow;
}

void
TFSContext::NextEntryCreated(node_ref &in_dir_ref, BNode &in_node) {

	if (mPointListPos < mPointList.end()) {		// if we have position info then write it out
		PoseInfo pose_info;
		
		pose_info.fInvisible = false;
		pose_info.fInitedDirectory = in_dir_ref.node;
		pose_info.fLocation = *mPointListPos;
		
		in_node.WriteAttr(kAttrPoseInfo, B_RAW_TYPE, 0, &pose_info, sizeof(PoseInfo));
		
		++mPointListPos;
	}
}

void
TFSContext::DirectoryTrashed(node_ref &in_node) {

	BMessage message(kCloseWindowAndChildren);
	message.AddData("node_ref", B_RAW_TYPE, &in_node, sizeof(node_ref));
	be_app->PostMessage(&message);
}

void
TFSContext::AboutToDeleteOrTrash(BEntry &entry) {

	if (entry.IsDirectory()) {							// unmount if volume
		BDirectory dir;
		
		status_t rc;
		FS_OPERATION(dir.SetTo(&entry));

		if (dir.IsRootDirectory()) {
			struct stat st;
			FS_OPERATION(entry.GetStat(&st));
			
			BVolume	boot;
			FS_OPERATION(BVolumeRoster().GetBootVolume(&boot));
			
			if (boot.Device() != st.st_dev) {
				
				BMessage message(kUnmountVolume);
				message.AddInt32("device_id", st.st_dev);
				be_app->PostMessage(&message);

				if (IsAnswerPossible(fSkipEntry)) {
					FS_CONTROL_THROW(kSkipEntry);
				} else {
					return;
				}
			} else {
				for (;;)
					Interaction(kCannotUnmountBootVolume);
			}
		}
	}
}


status_t 
TFSContext::GetTrackerSettingsDir(BPath &path, bool autoCreate) {

	status_t rc = find_directory(B_USER_SETTINGS_DIRECTORY, &path, autoCreate);
	if (rc != B_OK)
		return rc;
		
	path.Append("Tracker");

	return mkdir(path.Path(), 0777) ? B_OK : errno;
}


void
TFSContext::CreateSpecialDirs() {

	BVolume volume;
	BVolumeRoster roster;

	while (roster.GetNextVolume(&volume) == B_OK) {
		
		if (volume.IsReadOnly() || !volume.IsPersistent())
			continue;
		
		BPath path;
		find_directory(B_DESKTOP_DIRECTORY, &path, true, &volume);
		find_directory(B_TRASH_DIRECTORY, &path, true, &volume);

		BDirectory trashDir;
		if (GetTrashDir(trashDir, volume.Device()) == B_OK) {
			size_t size;
			const void* data;
			if ((data = GetTrackerResources()->LoadResource('ICON', kResTrashIcon, &size)) != 0)
				trashDir.WriteAttr(kAttrLargeIcon, B_COLOR_8_BIT_TYPE, 0, data, size);

			if ((data = GetTrackerResources()->LoadResource('MICN', kResTrashIcon, &size)) != 0)
				trashDir.WriteAttr(kAttrMiniIcon, B_COLOR_8_BIT_TYPE, 0, data, size);
		}
	}
}


status_t
TFSContext::SetPoseLocation(BEntry &entry, const BPoint &point) {

	status_t rc;

	BNode node(&entry);
	FS_STATIC_OPERATION(node.InitCheck());
	
	BDirectory parent;
	FS_STATIC_OPERATION(entry.GetParent(&parent));
	
	node_ref destNodeRef;
	FS_STATIC_OPERATION(parent.GetNodeRef(&destNodeRef));
	
	return SetPoseLocation(destNodeRef.node, node, point);
}

bool
TFSContext::GetPoseLocation(const BNode &node, BPoint &point) {

	PoseInfo poseInfo;
	if (ReadAttr(node, kAttrPoseInfo, kAttrPoseInfoForeign,
			B_RAW_TYPE, 0, &poseInfo, sizeof(poseInfo), &PoseInfo::EndianSwap)
			== kReadAttrFailed)
		return false;
	
	if (poseInfo.fInitedDirectory == -1LL)
		return false;

	point = poseInfo.fLocation;

	return true;
}



}	// namespace BPrivate
