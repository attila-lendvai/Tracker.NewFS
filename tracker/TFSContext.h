#if !defined(_TRACKERFSCONTEXT_H)
#define _TRACKERFSCONTEXT_H

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

#include <StopWatch.h>
#include <MessageRunner.h>

#define private public			// XXX HUGE hack...
#include <StatusBar.h>
#undef private

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
	
	fs::EntryIterator *	NewIterator()									{ return mEntryList.NewIterator(); }
	point_list_t &		PointList()										{ mPointList.reserve(mEntryList.size()); return mPointList; }
	fs::EntryList &		EntryList()										{ return mEntryList; }

			int32		EncapsulatedEntryCount()						{ return mEntryList.size(); }

			status_t	CalculateItemsAndSize(ProgressInfo &ininfo);
			status_t	CalculateItemsAndSize(fs::EntryIterator &i, ProgressInfo &info) { return inherited::CalculateItemsAndSize(i, info); }
			status_t	CopyTo(BDirectory &target_dir, bool async);
			status_t	Duplicate(bool async);
			status_t	CreateLinkTo(BDirectory &target_dir, bool relative, bool async);
			status_t	MoveTo(BDirectory &target_dir, bool async);
			status_t	MoveToTrash(bool async);
			status_t	RestoreFromTrash(bool async);
			status_t	Remove(bool in_ask_user, bool async);

			void		SetInteractive(bool in_state)					{ mInteractive = in_state; }
			bool		IsInteractive() const							{ return mInteractive; }

			void		DisableProgressInfo()							{ mProgressInfoEnabled = false; }

			bool		Pause();
			bool		IsPauseRequested() const						{ return mPause; }
			bool		IsPaused() const								{ return mActuallyPaused; }
			bool		IsAutoPaused() const							{ return mAutoPaused; }
			void		ContinueAutoPaused() 							{ mShouldAutoPause = false; HardResume(); }
			
			void		SoftResume();
			void		HardResume()									{ mPause = false; SoftResume(); }
			int32		EstimatedTimeLeft();
			bigtime_t	ElapsedTime() const								{ return mElapsedStopWatch.ElapsedTime(); }
			bigtime_t	RealElapsedTime() const							{ ASSERT(mStartTime != -1);	return system_time() - mStartTime; }

			void		Cancel()										{ mCancel = true; HardResume(); }
			void		SkipOperation()									{ mSkipOperation = true; SoftResume(); }
			void		SkipEntry()										{ mSkipEntry = true; SoftResume(); }
			void		SkipDirectory()									{ mSkipDirectory = true; SoftResume(); }
			void		PrintOperationStackToStream()					{ puts(mOperationStack.AsString()); }
			bool		IsAnswerPossible(answer_flags iflag)			{ return inherited::IsAnswerPossible(iflag); }
			bool		IsCalculating()	const							{ return mOperationStack.Contains(kCalculatingItemsAndSize); }
			bool		IsEffectiveFileCopyRunning() const				{ return mProgressInfo.HasFileSizeProgress(); }
			const char *CurrentEntryName() const						{ return inherited::CurrentEntryName(); }
			bool		HasDialogWindow() const							{ return mDialogWindow != 0; }
			BWindow	*	DialogWindow() const;

			bool		IsOperationStringDirty() const					{ return mOperationStringDirty; }
			void		ClearOperationStringDirty()						{ mOperationStringDirty = false; }

			void		EffectiveCopyBegins()							{ mOverheadStopWatch.Suspend(); }
			void		EffectiveCopyEnds()								{ mOverheadStopWatch.Resume(); }

			bool		DidOperationBegin()								{ return mOperationBegun; }
			operation	RootOperation() const							{ return mOperationStack.RootOperation(); }
			operation	CurrentOperation() const						{ return mOperationStack.Current(); }
			operation	LastPrimaryOperation() const					{ return mOperationStack.LastPrimaryOperation(); }
			const char *LastPrimaryOperationAsString()					{ return OperationStack::AsString(LastPrimaryOperation()); }
			const char *OperationStackAsString()						{ return mOperationStack.AsString(); }
			int32		OperationStackSize() const						{ return mOperationStack.CountItems(); }
			
			void		CheckCancel();

	static	status_t	SetPoseLocation(ino_t in_dest_dir_inode, BNode &in_dest_node, const BPoint &in_point);
	static	status_t	SetPoseLocation(BEntry &in_entry, const BPoint &in_point);
	static	bool		GetPoseLocation(const BNode &node, BPoint &point);
	static	status_t	GetTrackerSettingsDir(BPath &, bool autoCreate = true);
	static	void		CreateSpecialDirs();
//	static	status_t	CopyAttributesAndStats(BNode &, BNode &);	delete
	
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

