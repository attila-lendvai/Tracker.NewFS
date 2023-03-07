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

#ifndef _TRACKERFSCONTEXT_H
#define _TRACKERFSCONTEXT_H

#include "Defines.h"
#include <StopWatch.h>
#include <MessageRunner.h>
#include <StatusBar.h>
#include <vector>

#include <boost/call_traits.hpp>

#include "FSContext.h"
#include "PoseList.h"

namespace BPrivate {

class FSDialogWindow;

class TFSContext : public fs::FSContext {
	typedef	FSContext			inherited;
	typedef vector<BPoint>		point_list_t;

	struct PauseStopWatch_ResumeIf {
		typedef bool (TFSContext::*function_type)() const;
		
		BStopWatch &	mStopWatch;
		TFSContext *	mObject;
		function_type	mFunction;
		
		PauseStopWatch_ResumeIf(BStopWatch &in_watch, TFSContext *obj, function_type func) :
							mStopWatch(in_watch), mObject(obj), mFunction(func) {

			mStopWatch.Suspend();
		}
		~PauseStopWatch_ResumeIf() {
			if ((mObject ->* mFunction)())
				mStopWatch.Resume();
		}
	};

	void	initialize();
	
public:
	TFSContext();
	TFSContext(BObjectList<entry_ref> *);
	TFSContext(const PoseList &);
	TFSContext(const BMessage &);
	TFSContext(const entry_ref &);
	~TFSContext();
	
	fs::EntryIterator *	NewIterator()		{ return mEntryList.NewIterator(); }
	point_list_t &		PointList()			{ mPointList.reserve(mEntryList.size()); return mPointList; }
	fs::EntryList &		EntryList()			{ return mEntryList; }

	int32		EncapsulatedEntryCount()	{ return mEntryList.size(); }

_IMPEXP_TRACKER		status_t	CalculateItemsAndSize(ProgressInfo &ininfo);
_IMPEXP_TRACKER		status_t	CalculateItemsAndSize(fs::EntryIterator &i, ProgressInfo &info) { return inherited::CalculateItemsAndSize(i, info); }
_IMPEXP_TRACKER		status_t	CopyTo(BDirectory &target_dir, bool async);
_IMPEXP_TRACKER		status_t	Duplicate(bool async);
_IMPEXP_TRACKER		status_t	CreateLinkTo(BDirectory &target_dir, bool relative, bool async);
_IMPEXP_TRACKER		status_t	MoveTo(BDirectory &target_dir, bool async);
_IMPEXP_TRACKER		status_t	MoveToTrash(bool async);
_IMPEXP_TRACKER		status_t	RestoreFromTrash(bool async);
_IMPEXP_TRACKER		status_t	Remove(bool in_ask_user, bool async);

_IMPEXP_TRACKER		void		SetInteractive(bool in_state)					{ mInteractive = in_state; }
_IMPEXP_TRACKER		bool		IsInteractive() const							{ return mInteractive; }

_IMPEXP_TRACKER		void		DisableProgressInfo()							{ mProgressInfoEnabled = false; }

_IMPEXP_TRACKER		bool		Pause();
_IMPEXP_TRACKER		bool		IsPauseRequested() const						{ return mPause; }
_IMPEXP_TRACKER		bool		IsPaused() const								{ return mActuallyPaused; }
_IMPEXP_TRACKER		bool		IsAutoPaused() const							{ return mAutoPaused; }
_IMPEXP_TRACKER		void		ContinueAutoPaused() 							{ mShouldAutoPause = false; HardResume(); }
			
_IMPEXP_TRACKER		void		SoftResume();
_IMPEXP_TRACKER		void		HardResume()									{ mPause = false; SoftResume(); }
_IMPEXP_TRACKER		int32		EstimatedTimeLeft();
_IMPEXP_TRACKER		bigtime_t	ElapsedTime() const								{ return mElapsedStopWatch.ElapsedTime(); }
_IMPEXP_TRACKER		bigtime_t	RealElapsedTime() const							{ ASSERT(mStartTime != -1);	return system_time() - mStartTime; }

_IMPEXP_TRACKER		void		Cancel()										{ mCancel = true; HardResume(); }
_IMPEXP_TRACKER		void		SkipOperation()									{ mSkipOperation = true; SoftResume(); }
_IMPEXP_TRACKER		void		SkipEntry()										{ mSkipEntry = true; SoftResume(); }
_IMPEXP_TRACKER		void		SkipDirectory()									{ mSkipDirectory = true; SoftResume(); }
_IMPEXP_TRACKER		void		PrintOperationStackToStream()					{ puts(mOperationStack.AsString()); }
_IMPEXP_TRACKER		bool		IsAnswerPossible(answer_flags iflag)			{ return inherited::IsAnswerPossible(iflag); }
_IMPEXP_TRACKER		bool		IsCalculating()	const							{ return mOperationStack.Contains(kCalculatingItemsAndSize); }
_IMPEXP_TRACKER		bool		IsEffectiveFileCopyRunning() const				{ return mProgressInfo.HasFileSizeProgress(); }
_IMPEXP_TRACKER		const char *CurrentEntryName() const						{ return inherited::CurrentEntryName(); }
_IMPEXP_TRACKER		bool		HasDialogWindow() const							{ return mDialogWindow != 0; }
_IMPEXP_TRACKER		BWindow	*	DialogWindow() const;

_IMPEXP_TRACKER		bool		IsOperationStringDirty() const					{ return mOperationStringDirty; }
_IMPEXP_TRACKER		void		ClearOperationStringDirty()						{ mOperationStringDirty = false; }

_IMPEXP_TRACKER		void		EffectiveCopyBegins()							{ mOverheadStopWatch.Suspend(); }
_IMPEXP_TRACKER		void		EffectiveCopyEnds()								{ mOverheadStopWatch.Resume(); }

_IMPEXP_TRACKER		bool		DidOperationBegin()								{ return mOperationBegun; }
_IMPEXP_TRACKER		operation	RootOperation() const							{ return mOperationStack.RootOperation(); }
_IMPEXP_TRACKER		operation	CurrentOperation() const						{ return mOperationStack.Current(); }
_IMPEXP_TRACKER		operation	LastPrimaryOperation() const					{ return mOperationStack.LastPrimaryOperation(); }
_IMPEXP_TRACKER		const char *LastPrimaryOperationAsString()					{ return OperationStack::AsString(LastPrimaryOperation()); }
_IMPEXP_TRACKER		const char *OperationStackAsString()						{ return mOperationStack.AsString(); }
_IMPEXP_TRACKER		int32		OperationStackSize() const						{ return mOperationStack.CountItems(); }
			
_IMPEXP_TRACKER		void		CheckCancel();

	static	status_t	SetPoseLocation(ino_t in_dest_dir_inode, BNode &in_dest_node, const BPoint &in_point);
	static	status_t	SetPoseLocation(BEntry &in_entry, const BPoint &in_point);
	static	bool		GetPoseLocation(const BNode &node, BPoint &point);
	static	status_t	GetTrackerSettingsDir(BPath &, bool autoCreate = true);
	static	void		CreateSpecialDirs();
	
private:
	// Overridden functions
	void		InitProgressIndicator();
	bool		ErrorHandler(status_t iStatus);
	command		Interaction(interaction icode);
			
	void		NextEntryCreated(node_ref &, BNode &);
	void		DirectoryTrashed(node_ref &);
	void		AboutToDeleteOrTrash(BEntry &);

	void		PreparingOperation();
	void		OperationBegins();

	bool		IsConnectedToStatusWindow() const				{ return mConnectedToStatusWindow; }


	FSDialogWindow *					mDialogWindow;
	bigtime_t							mStartTime;
	BStopWatch							mElapsedStopWatch;
	BStopWatch							mOverheadStopWatch;
	fs::EntryList						mEntryList;				// the list of entries we will operate on
	fs::EntryList::STLEntryIterator		mEntryIterator;			// an iterator for the above list
	point_list_t						mPointList;
	point_list_t::iterator				mPointListPos;
	thread_id							mWorkingThread;
	float								mPreviousTimeGuess;		// to avoid time guess jumping around, make an average of the last two
	bool								mConnectedToStatusWindow;
	bool								mInteractive;
	bool								mProgressInfoEnabled;
	bool								mOperationBegun;
	bool								mWasOverheadSWRunning;
	bool								mOperationStringDirty;
	bool								mShouldAutoPause;
	
	bool								mPause;
	bool								mActuallyPaused;			// pause is only a request, buffer is first emptied. this flag shows wether it is actually paused
	bool								mAutoPaused;
	bool								mCancel;
	bool								mSkipOperation;
	bool								mSkipEntry;
	bool								mSkipDirectory;
};

}	// namespace BPrivate

#endif // _TRACKERFSCONTEXT_H

