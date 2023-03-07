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


#include <memory>
#include <File.h>
#include <SymLink.h>
#include <VolumeRoster.h>
#include <fs_info.h>
#include <NodeInfo.h>
#include <Debug.h>
#include <Locker.h>
#include <Autolock.h>
#include <fs_attr.h>
#include <sys/stat.h>
#include <ctype.h>
#include <Drivers.h>

#if _BUILDING_tracker
  #include "Attributes.h"
  #include "MimeTypes.h"
  #include <Screen.h>
#endif

#define FS_CONFIG_MULTITHREADED		1
#define FS_RELATIVE_LINKS_KEEP_TARGET_WHEN_MOVED 1	// when moving a relative link on one volume from dir to dir it will keep it's target
//#define FS_PRINT_BUFFER_INFO		1	// will print the memory buffer allocations
//#define FS_SAME_DEVICE_OPT		1		// this will call a sync on the target node when a file is copied on the same phisical device (should speed things up)
//#define FS_USE_SET_FILE_SIZE		1		// will call BFile::SetSize() before copy; prpbably faster, but never checked
											// but this way in an extreme case you may end up in a properly sized file not properly copied!
// 	See header file for more config stuff!!!

#include "FSContext.h"
#include "FSUtils.h"
#include "LanguageTheme.h"
#include "Undo.h"

// from FSUtils.cpp
enum {
	kNotConfirmed,
	kConfirmedHomeMove,
	kConfirmedAll
};

#if FS_CONFIG_MULTITHREADED
	#include "ThreadMagic.h"
	using namespace TM_NAMESPACE;
#endif

#include <stdio.h>

#include <functional>
#include <algorithm>

// Known bugs:
// - Dropping a file on a popoupmenu that comes up if you leave the mouse over an icon initiates a move even
//   if cross-device. It should copy instead. Should be fixed at some higher level then this
// - MoveTo menu automatically calls FSContext::Copy across volumes
// - according to areawatch seems to leak memory when one-disk mode copy is cancelled... (?!?!?! probably some
//   memory management sideeffect, malloc_debug does not report it)
// - malloc_debug reports sometimes memory freed twice and written to after freed also in the above case (seems to be false alert!??!?!)
// - when moving an entry and there's a dir-dir name collision and we end up sweeping together the two dirs instead of
//   moving, then the pose position is mistakenly set for the source as if it were moved (should not be set, couse it's contents
//   are swept together)
// - Estimated time left is sometimes garbage if theres a dialog at the beginning (still exists?)
// - if an entry that has no pose info yet is dropped to an icon poseview then the position is not set properly? or something...
// - pressing cancel while a cancel dialog (delete/keep as is) is already up hard-cancels the operation
// - no wellknownentrylist, at least it's not used
// - CreateLink should not show replaceifnewer and take the link source as a real source if it's actually creating a link,
//   but it's also used for copying a link, so... rework it.
// - restoring an item from the trash copies even on the same volume (?) (only when dropping the icon by the mouse?)
// - writing to samba shares does not work
// - on media error the floppy driver reports some B_ERROR but writes portion of the buffer. so the file will be bigger at the
//    end due to these unreported writes. on error nothing should be written... on the other hand sometimes no error code is
//    reported from beos only a zero while reading fails.
// - duplicate starts up a writer thread; it should be single threaded


// ToDo:
// - Write total size for emptying trash?
// - Automatic Trash management, clean up once in a while, delete old files
// - Add shortcuts for dialog window answers, like r to replace
// - double cancel should force cacelling when finishing a file before cancelling while sweeping together two dirs
// - test multithreaded copy throwing, when the thrown exception is transfered from the writer thread to the main copy thread
// - identify untyped files that come from fat disks
// - rewrite renaming in widgetattrtext.cpp so that it shows a smarter dialog window if the name is already taken
// - the already exists interaction should print file/dir/link instead of entry in all cases, maybe should contain more info,
//    maybe behind a more button
// - decide: should a relative link keep it's target when moved or simply be moved as is?
// - Check if replacing dir A then dir A should not be replaced by something from dir A
// - store current chunk size for drives, to start with that in the copy of the next file (?)
// - make a new NamedPaneSwitch class
// - compile time switch for quick pause/release mem pause modes
// - think about unifying the way possible answers are handled and handle all of them with FS_ENABLE_POSSIBLE_ANSWER ???
// - rethink when and how to auto rename entries when the target name is occupied (like creating link from "foobar"
//    and automatically naming it "foobar link" if the foobar name is used)

// Possible improvements:
// - Build a tree of the source filesystem tree in memory, and while doing the preflight check and store file counts and sizes 
//   for each dir (required to remain in sync when skipping a dir) and check for conflicts, prompt for answer and store
//   the answers. There should be a dry-run flag and when it's set each operation should only count the entries and check for
//   the need of user interaction. After that the operation should be run on normally, executing the previously given answers.
//   The rest of the system should not change (noone guarantees that there'll be no more conflict) but it should
//   first check for the stored answers at the current item in the tree and ask the user only if there's no already stored answer.
//   Store entry modification time and ignore answers given in the dry-run if entry was modified
// - Make a seperate daemon that receives commands in a BMessage. Create a small wrapper class that handles
//   communication from the remote app.
// - Extend FSContext so that at Interaction() the operation does not stop but rather continues with the next
//   entry and at the end restarts with the problematic operations/entries according to the user feedback.
// - FSContext should read and write more files at once when it's working on one device (too much work and
//   the risk of more bugs for a rather small gain)

// Assumptions, rules, etc.:
// - General concept of FSContext: a context object that stores the current state, with several functions that
//   call each other in a recursive manner. Recursion depth dependent variables are backed up on the stack with
//   FS_BACKUP_VARIABLE_AND_SET(), and are automatically restored when leaving scope. Special control
//   of recursion depth is achived with throwing exceptions and catching them at the right level. (I know you've
//   read in several books that using exceptions for controlling program flow is evil... but mail me if you know any
//   other way, which is also clean. And don't tell me that C++ is handycapped... i know... :)
// - Keep in mind that the precondition lists at the beginning of the functions are not guaranteed to be complete
// - FSContext must be allocated on the heap, and will be freed after the invoked operation is finished; after
//   the last operation leaves the operation stack. You may not access any member variables after that point.
// - After a SourceDirSetter left scope mSourceDir is Unset(). The obvious solution would be to set mSourceDir
//   to the previous dir in SourceDirSetter dtor, but it's unnecessary and would require some rework. Take good
//   care not to use mSourceDir after a SourceDirSetter left scope somewhere (even called functions count) and
//   before SourceDirSetter::SetSourceDir() is not yet called again.
// - The public API never throws, only returns a status_t. The private API throws in case of an error. Take good
//   care not to have any FS_OPERATION() outside of a try block!
// - If an API function takes an input pointer parameter then it takes ownership too. Each function that takes an
//   EntryIterator * will delete the iterator when it's done. Only such functions can be async. If a function
//   takes an output pointer parameter then it means that this pointer may be NULL.
// - The public API returns kCancel if the user cancelled. It could be filtered out in every catch if required.
// - It's illegal to return from any public API function before at least one FS_SET_OPERATION() (otherwise who will
//   delete the FSContext?) PopOperation() "delete this" if it's popping the last operation.
// - When InitProgressIndicator is called the root operation must already be set.
// - EffectiveOperationBegins/Ends is called to porvide some way to monitor overhead. In case of TFSContext a
//   BStopWatch (mOverheadStopWatch) is measuring the time spent doing something for the operation but not
//   effective file copy. This overhead/entry information may help for better estimation of the time left.

namespace fs {

static const bigtime_t	kProgressUpdateRate				= 250000;			// should be in sync with FSStatusWindow
static const size_t		kCopyBufferInitialSize			= 64 * 1024;
static const int32		kMemsizeDividerForCopyBuffer	= 8;				// mMaxBufferSize = memory size / kMemsizeDividerForCopyBuffer
static const off_t		kMultiThreadedFileSizeLimit		= 1 * 1024 * 1024;	// files bigger than this will be copied with two threads
static const size_t		kMinBufferSize					= 32 * 1024;
static const off_t		kMinChunkSize					= 16 * 1024;		// read/write chunks while copying, must be >=16k
static const int32		kMaxChunkSizeDivider			= 32;				// max_mem / kMaxChunkSizeDivider is the max size of the chunk in multithreaded copy
static const bigtime_t	kShrinkBufferAfterThisDelay		= 2000000;			// delay after buffer may shrink
static const int32		kBuffersInTwoDeviceCopyMode		= 4;				// must be bigger or equal to 2
static const int32		kReaderSemCount					= kBuffersInTwoDeviceCopyMode - 1;

#if FS_SAME_DEVICE_OPT
static const int32		kSyncLowerLimit					= 128 * 1024;		// files smaller then this won't be flushed when they are written
#endif

uint64						FSContext::sMaxMemorySize;
int32						FSContext::sEmptyTrashRunning = 0;
int32						FSContext::sInitialized = 0;
BLocker						FSContext::sLocker;
FSContext::node_ref_list_t	FSContext::sTrashDirList;
FSContext::node_ref_list_t	FSContext::sDesktopDirList;
FSContext::node_ref_list_t	FSContext::sPrintersDirList;
FSContext::node_ref_list_t	FSContext::sHomeDirList;
FSContext::node_ref_list_t	FSContext::sSystemDirList;
FSContext::node_ref_list_t	FSContext::sBeOSDirList;
FSContext::command			FSContext::sLastSelectedInteractionAnswers[kTotalInteractions];


const char *FSContext::sCommandStringTable[kTotalCommands] = {
	#define FSCMD(a, b, c)			b,
	#define FSCMD_INITED(a, b, c, d)	b,
	#define FSCMD_HIDDEN(a, b)
	#include "FSCommands.tbl"
};

const char *FSContext::sOperationStringTable[kTotalOperations] = {
	#define FSOP(name, string, suffix)	string,
	#include "FSOperations.tbl"
};

const char *FSContext::sTargetDirPrefixTable[kTotalOperations] = {
	#define FSOP(name, string, suffix)	suffix,
	#include "FSOperations.tbl"
};

const FSContext::InteractionDescription		FSContext::sInteractionsTable[kTotalInteractions] = {
	#define		INT_0(name, string, showfile) \
				 	{string, showfile, \
				 		{kInvalidCommand}, \
				 		{false, false, false, false, false, false, false, false, false} \
				 	},
	#define		INT_1(name, string, showfile, a, ax) \
				 	{string, showfile, \
				 		{a, kInvalidCommand}, \
				 		{ax, false, false, false, false, false, false, false, false} \
				 	},
	#define		INT_2(name, string, showfile, a, ax, b, bx) \
				 	{string, showfile, \
				 		{a, b, kInvalidCommand}, \
				 		{ax, bx, false, false, false, false, false, false, false} \
				 	},
	#define		INT_3(name, string, showfile, a, ax, b, bx, c, cx) \
				 	{string, showfile, \
				 		{a, b, c, kInvalidCommand}, \
				 		{ax, bx, cx, false, false, false, false, false, false} \
				 	},
	#define		INT_4(name, string, showfile, a, ax, b, bx, c, cx, d, dx) \
				 	{string, showfile, \
				 		{a, b, c, d, kInvalidCommand}, \
				 		{ax, bx, cx, dx, false, false, false, false, false} \
				 	},
	#define		INT_5(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex) \
				 	{string, showfile, \
				 		{a, b, c, d, e, kInvalidCommand}, \
				 		{ax, bx, cx, dx, ex, false, false, false, false} \
				 	},
	#define		INT_6(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx) \
				 	{string, showfile, \
				 		{a, b, c, d, e, f, kInvalidCommand}, \
				 		{ax, bx, cx, dx, ex, fx, false, false, false} \
				 	},
	#define		INT_7(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx, g, gx) \
				 	{string, showfile, \
				 		{a, b, c, d, e, f, g, kInvalidCommand}, \
				 		{ax, bx, cx, dx, ex, fx, gx, false, false} \
				 	},
	#define		INT_8(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx, g, gx, h, hx) \
				 	{string, showfile, \
				 		{a, b, c, d, e, f, g, h, kInvalidCommand}, \
				 		{ax, bx, cx, dx, ex, fx, gx, hx, false} \
				 	},
	#define		INT_9(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx, g, gx, h, hx, i, ix) \
				 	{string, showfile, \
				 		{a, b, c, d, e, f, g, h, i, kInvalidCommand}, \
				 		{ax, bx, cx, dx, ex, fx, gx, hx, ix, false} \
				 	},
	#define		INT_10(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx, g, gx, h, hx, i, ix, j, jx) \
				 	{string, showfile, \
				 		{a, b, c, d, e, f, g, h, i, j, kInvalidCommand}, \
				 		{ax, bx, cx, dx, ex, fx, gx, hx, ix, jx, false} \
				 	},
	#define		INT_11(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx, g, gx, h, hx, i, ix, j, jx, k, kx) \
				 	{string, showfile, \
				 		{a, b, c, d, e, f, g, h, i, j, k, kInvalidCommand}, \
				 		{ax, bx, cx, dx, ex, fx, gx, hx, ix, jx, kx, false} \
				 	},
	#define		INT_12(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx, g, gx, h, hx, i, ix, j, jx, k, kx, l, lx) \
				 	{string, showfile, \
				 		{a, b, c, d, e, f, g, h, i, j, k, l, kInvalidCommand}, \
				 		{ax, bx, cx, dx, ex, fx, gx, hx, ix, jx, kx, lx, false} \
				 	},
	#define		INT_13(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx, g, gx, h, hx, i, ix, j, jx, k, kx, l, lx, m, mx) \
				 	{string, showfile, \
				 		{a, b, c, d, e, f, g, h, i, j, k, l, m, kInvalidCommand}, \
				 		{ax, bx, cx, dx, ex, fx, gx, hx, ix, jx, kx, lx, mx, false} \
				 	},
	#include		"FSInteractions.tbl"
};

// STL extensions
template <typename T>
struct deleter : public unary_function<T, void> {
	void operator()(T *x)		{ delete x; }
};

#if FULL_ERROR_EXCEPTION

FSException::FSException(const FSException &other) {
	mStatus = other.mStatus;
	mWhat = other.mWhat;
	mDeleteWhat = other.mDeleteWhat;
	const_cast<FSException &>(other).mDeleteWhat = false;	// take ownership of error string
}

FSException::FSException() : mDeleteWhat(false) {
	mStatus = B_ERROR;
	mWhat = "General FSContext error";
}

FSException::FSException(const FSContext::command iCommand) : mStatus(iCommand), mDeleteWhat(false) {
}

FSException::FSException(const status_t iStatus) : mStatus(iStatus), mDeleteWhat(false) {
	mWhat = strerror(iStatus);
}

FSException::FSException(const char *iStr) : mDeleteWhat(false) {
	mStatus = B_ERROR;
	mWhat = iStr;
}

FSException::FSException(const char *iStr, const status_t iStatus) : mStatus(iStatus), mDeleteWhat(true) {
	BString str(iStr);
	str += strerror(iStatus);
	mWhat = new char[str.Length() + 1];
	strcpy(const_cast<char *>(mWhat), str.String());
}

FSException::FSException(const char *iStr1, const char *iStr2, const char *iStr3,
		const status_t iStatus)  : mStatus(iStatus), mDeleteWhat(true) {
	BString str(iStr1);
	str += iStr2;
	str += iStr3;
	str += strerror(iStatus);
	mWhat = new char[str.Length() + 1];
	strcpy(const_cast<char *>(mWhat), str.String());
}

FSException::~FSException() {
	if (mDeleteWhat)
		delete [] mWhat;
}
		
const char *
FSException::what() const {
	return mWhat;
}

#endif // FULL_ERROR_EXCEPTION

bool FSContext::IsExtendedCommand(interaction iact, command icmd) FS_NOTHROW {
	ASSERT(iact >= 0  &&  iact < kTotalInteractions);
	ASSERT(icmd >= 0  &&  icmd < kTotalCommands);
	for (int i = 0;  i < kMaxAnswersForInteraction;  ++i) {
		if (sInteractionsTable[iact].mPossibleAnswers[i] == icmd)
			return sInteractionsTable[iact].mAnswerIsExtended[i];
	}
	if (icmd == kRetryEntry)
		return true;
		
	return false;
}


const char *
FSContext::OperationStack::AsString() FS_NOTHROW {
	mStackString = "";
	
	for (int32 i = 0;  i < CountItems();  i++) {
		if (i != 0)
			mStackString.Append(" -> ");
		mStackString.Append(AsString( mElements[i] ));
	}
	
	return mStackString.String();
}


const char *
FSContext::AsString(command incmd) FS_NOTHROW {

	ASSERT(incmd < kTotalCommands  &&  sCommandStringTable[incmd] != 0);
	
	
	if (incmd >= kTotalCommands)
		return "";

	return LOCALE(sCommandStringTable[incmd]);
}

const char *
FSContext::OperationStack::AsString(operation inop) FS_NOTHROW {

	ASSERT(inop < kTotalOperations);	// safe to fail
	
	if (inop >= kTotalOperations)
		return "";

	return LOCALE(FSContext::sOperationStringTable[inop]);
}

bool
FSContext::PushOperation(operation iop) FS_NOTHROW {
	DEBUG_ONLY(if (iop == kInitializing) ASSERT(mOperationStack.CountItems() > 0));
	if (mOperationStack.CountItems() == 0) {
		mCurrentEntry = 0;			// in case of an early error in some init phase
		SetCurrentEntryType(0);		// mark that there's no info available yet
		mOperationStack.Push(iop);
	} else {
		if (mOperationStack.Current() != iop)
			mOperationStack.Push(iop);
		else
			return false;			// didn't pushed
	}

	return true;
}

void
FSContext::PopOperation() FS_NOTHROW {
	ASSERT(mOperationStack.CountItems() > 0);
	mOperationStack.Pop();
	if (mOperationStack.CountItems() == 0)
		delete this;
}

const char *
FSContext::AsString(mode_t type) FS_NOTHROW {
	if (type == 0)
		return LOCALE("entry");
	else if (S_ISDIR(type))
		return LOCALE("directory");
	else if (S_ISREG(type))
		return LOCALE("file");
	else if (S_ISLNK(type))
		return LOCALE("link");
	else
		return LOCALE("nonregular entry");
}

bool
FSContext::ShouldCopy(copy_flags iflag) FS_NOTHROW	{
	return ((iflag & mCopyFlags) != 0);
}

FSContext::FSContext() FS_NOTHROW :
							mShrinkBufferSuggestionTime(0),
							mBuffer(0),
							mBufferSize(0),
							mCurrentEntry(0),
							mPossibleAnswers(fDefaultPossibleAnswers),
							mSkipOperationTargetOperation(kInvalidOperation),
							mSourceDevice(-1),
							mThrowThisInMainThread(kInvalidCommand),
							mWriterThreadRunning(false),
							mWriterThreadID(0),
							mChunkList(0),
							mWasMultithreaded(false),
							#if FS_MONITOR_THREAD_WAITINGS
								mWriterThreadWaiting("", true),
								mReaderThreadWaiting("", true),
							#endif
							mCopyLinksInstead(false),
							mDirectoryCreatedByUs(false),
							mThrowThisAfterFileCopyFinished(kInvalidCommand),
							mFileCreatedByUs(false),
							mCopyFlags(fDefaultCopyFlags),
							mSourceNewName(0),
							mTargetNewName(0),
							mTargetEntry(0),
							mBufferReleasable(true),
							mSameDevice(false)
							#if _BUILDING_tracker
								, mSkipTrackerPoseInfoAttibute(false)
							#endif
							{

#if DEBUG
		if (((int)this & 0xf0000000)  !=  0x80000000)
			DEBUGGER("FSContext must be allocated on the heap and will be freed after the last operation left the stack");
#endif

	#if FS_MONITOR_THREAD_WAITINGS
		mWriterThreadWaiting.Reset();
		mWriterThreadWaiting.Suspend();
		
		mReaderThreadWaiting.Reset();
		mReaderThreadWaiting.Suspend();
	#endif
	
	if (atomic_or(&sInitialized, 1) == 0) {
		for (int i = 0;  i < kTotalInteractions;  ++i)
			sLastSelectedInteractionAnswers[i] = kInvalidCommand;
		
		system_info si;
		get_system_info(&si);
		sMaxMemorySize = (uint64)si.max_pages * B_PAGE_SIZE;
	}
	
	for (int i = 0;  i < kTotalInteractions;  ++i)
		mDefaultInteractionAnswers[i] = kInvalidCommand;
	
	mMaxBufferSize = sMaxMemorySize / kMemsizeDividerForCopyBuffer;
	
	if (gTrackerSettings.UndoEnabled())
		gUndoHistory.PrepareUndoContext(this);
}

FSContext::~FSContext() FS_NOTHROW
{
	if (gTrackerSettings.UndoEnabled())
		gUndoHistory.CommitUndoContext(this);
	
	delete [] mBuffer;
	delete mChunkList;

	for (int32 i = 0;  mWriterThreadRunning == true;  ++i) {	// wait for the writer thread to realize that we are quitting...
		snooze(100000);
		ASSERT(i < 10);

		#if 0
		// Killing the thread is not a good idea, some dialog window may be open,
		// that would write to released stack space and such things
		if (i > 10) {
			kill_thread(mWriterThreadID);
			DEBUGGER("Writer thread didn't exit in time");
			break;
		}
		#endif
	}
}

void
FSContext::SuggestBufferSize(size_t requested_size) FS_NOTHROW {
//	if (mBufferSize >= requested_size)									// if buffer is already big enough
//		return;

	ASSERT(requested_size > 0);

	requested_size += (16 * 1024 - 1);
	requested_size -= requested_size % (16 * 1024);						// round it to 16k boundary
	
	if (requested_size > mBufferSize) {
	
		mShrinkBufferSuggestionTime = 0;								// reset timer

		size_t tmp = (mBufferSize << 1) - (mBufferSize >> 1);				// mBufferSize * 1.5
		size_t new_size = (requested_size > tmp) ? requested_size : tmp;	// rise buffer size at least by *1.5
		
		if (new_size > mMaxBufferSize)										// limit to mMaxBufferSize
			new_size = mMaxBufferSize;
			
		system_info info;
		get_system_info(&info);
		
		uint64 free_mem = (uint64)(info.max_pages - info.used_pages) * B_PAGE_SIZE;
		
		// if two third of the free mem is less then the amount we want to rise the buffer with
		if (mBufferSize == 0  ||  (mBufferSize < new_size  &&  free_mem * 2 / 3 > (size_t)(new_size - mBufferSize)))
			SetBufferSize(new_size);

	} else {

		if (mShrinkBufferSuggestionTime == 0)
			mShrinkBufferSuggestionTime = system_time();
		else if (system_time() - mShrinkBufferSuggestionTime  >  kShrinkBufferAfterThisDelay) {
			SetBufferSize(requested_size > mBufferSize >> 1  ?  requested_size  :  mBufferSize >> 1);
			mShrinkBufferSuggestionTime = system_time();
		}
	}
}

void
FSContext::SetBufferSize(size_t newsize) FS_THROW_FSEXCEPTION {

	if ((newsize != 0  &&  newsize < mBufferSize  &&  newsize < kMinBufferSize)  ||  newsize == mBufferSize)
		return;

	delete [] mBuffer;
	mBuffer = 0;
	mBufferSize = 0;

	if (newsize != 0) {
		try {
			mBuffer = new uint8[newsize];
		} catch (...) {
			TRESPASS();
			
			FS_ERROR_THROW("Cancelling due to out of memory error", B_NO_MEMORY);	// XXX what else? interaction?
		}
		
		mBufferSize = newsize;
		#if FS_PRINT_BUFFER_INFO
			printf("Buffer size: %ld\n", BufferSize());
		#endif
	}
}

uint8 *
FSContext::Buffer() FS_NOTHROW {
	if (mBuffer == 0) {
		mBuffer = new uint8[kCopyBufferInitialSize];
		mBufferSize = kCopyBufferInitialSize;
	}
	return mBuffer;
}


int
date_sorter_comp(const void *_first, const void *_second) {
	TrashedEntryRef **first = (TrashedEntryRef **)_first;
	TrashedEntryRef **second = (TrashedEntryRef **)_second;

	return (*first) -> TrashedDate() - (*second) -> TrashedDate();
}


void
FSContext::MakeSpaceFromTrash(off_t size) FS_NOTHROW {

	status_t rc;
	BDirectory trash;
	FS_OPERATION(GetTrashDir(trash, TargetDevice()));
	
	DirEntryIterator dei;
	FS_OPERATION(dei.SetTo(trash));
	
	int32 i;
	int32 count = dei.CountEntries();
	TrashedEntryRef **list = new (TrashedEntryRef *)[count];
	
	for (i = 0;  i < count;  ++i) {
		list[i] = new TrashedEntryRef();
		if (dei.GetNext(*list[i]) == false) {
			delete list[i];
			break;
		}
	}
	
	count = i;
	
	size = (off_t)((float)size * 1.3);		// 30 % plus space if possible to make the fs feel good
	
	if (i != 0) {
			
		qsort(list, count, sizeof(void *), &date_sorter_comp);
		
		for (i = 0;  i < count;  ++i) {
		
			BEntry entry;
			FS_OPERATION(entry.SetTo(*list[i]));
			{
				FS_SET_OPERATION(kRemoving);
				FS_SKIPABLE_OPERATION(Remove(entry));
			}
			if (mTargetVolume.FreeBytes() > size)
				break;
		}
	}
	
	for (i = 0;  i < count;  ++i)
		delete list[i];
	
	delete [] list;
}

void
FSContext::CheckFreeSpaceOnTarget(off_t size, interaction icode) FS_THROW_FSEXCEPTION {

	ASSERT(mTargetVolume.InitCheck() == B_OK);
	
	FS_SET_OPERATION(kCheckingFreeSpace);
	FS_ADD_POSSIBLE_ANSWER(fRetryOperation);
	
	while (mTargetVolume.IsReadOnly()  &&  Interaction(kTargetIsReadOnly) == kRetryOperation);

	ASSERT(mTargetVolume.InitCheck() == B_OK);
	
	for (;;) {
		if (mTargetVolume.FreeBytes() > size)
			break;
		
		FS_ADD_POSSIBLE_ANSWER(fIgnore);
		
		command cmd = Interaction(icode);
		
		if (cmd == kRetryOperation)
			continue;
		else if (cmd == kIgnore)
			break;
		else if (cmd == kMakeSpaceFromTrash)
			MakeSpaceFromTrash(size);
	}
}

void
FSContext::SetDefaultErrorAnswer(command ianswer, status_t ierror) FS_NOTHROW {
	if (mOperationStack.CountItems() <= 0  ||  ianswer == kRetryOperation  ||  ianswer ==  kRetryEntry)
		return;
		
	error_answer item(mOperationStack.Current(), ierror, ianswer);
	mDefaultErrorAnswers.Push(item);
}

FSContext::command
FSContext::DefaultErrorAnswer(status_t ierr) FS_THROW_FSEXCEPTION {

	for (int32 i = 0;  i < mDefaultErrorAnswers.CountItems();  i++) {
		if (mDefaultErrorAnswers[i].Equals(mOperationStack.Current(), ierr)) {
			if (mDefaultErrorAnswers[i].answer == kRetryOperation  &&  IsAnswerPossible(fRetryOperation)) {
				return kRetryOperation;
			} else {
				bool ok = true;	// check if the stored answer is possible in this context
				switch ((int32)mDefaultErrorAnswers[i].answer) {
					case kSkipEntry:			ok = IsAnswerPossible(fSkipEntry);			break;
					case kSkipOperation:		ok = IsAnswerPossible(fSkipOperation);		break;
					case kRetryEntry:			ok = IsAnswerPossible(fRetryEntry);			break;
					case kRetryOperation:		ok = IsAnswerPossible(fRetryOperation);		break;
					case kSkipDirectory:		ok = IsAnswerPossible(fSkipDirectory);		break;
				}
				if (ok)
					FS_CONTROL_THROW(mDefaultErrorAnswers[i].answer);
			}
		}
	}

	return (command)0;
}


FSContext::command
FSContext::DefaultInteractionAnswer(interaction iact) FS_THROW_FSEXCEPTION {
	command cmd = mDefaultInteractionAnswers[iact];
	
	if (cmd == kSkipEntry) {
		FS_CONTROL_THROW(kSkipEntry);
	} else if (cmd == kCancel) {
		FS_CONTROL_THROW(kCancel);
	}
	
	// XXX static default array which is saved across reboots and configurable. check kCancel, too ?
	return cmd;
}

void
FSContext::ResetProgressIndicator() FS_NOTHROW {
	mProgressInfo.Clear();
}

void
FSContext::ProgressInfo::PrintToStream() {
	printf("mTotalSize:\t%Ld\nmCurrentSize:\t%Ld\nTotalSizeProgress:\t%.2f\n", mTotalSize, mCurrentSize, TotalSizeProgress());
}

void
EntryRef::SetTo(dev_t idev, ino_t idir, const char *iname) FS_NOTHROW {
	mRef.device = idev;
	mRef.directory = idir;

	int32 len = strlen(iname) + 1;

	if (len > mSize) {
		if (mDeleteName)
			delete [] Name();

		mDeleteName = true;

		try {
			mRef.name = new char[len];
			mSize = len;
		} catch (...) {
			FS_ERROR_THROW("In EntryRef::SetTo()", B_NO_MEMORY);
		}
	}
	
	strcpy(mRef.name, iname);
}

EntryRef::EntryRef(dev_t dev, ino_t dir, const char *name) FS_NOTHROW
{
	initialize();
	SetTo(dev, dir, name);
}

EntryRef::EntryRef(const entry_ref &ref) FS_NOTHROW
{
	initialize();
	SetTo(ref.device, ref.directory, ref.name);
}

EntryRef::~EntryRef()
{
	if (mDeleteName)
		delete [] Name();
	
	mRef.name = 0;				// calm down entry_ref dtor
}


void	// Postcond:	entry is unset
FSContext::AccumulateItemsAndSize(BEntry &entry, bool recursive) FS_THROW_FSEXCEPTION
{
	status_t rc;
	
	if (mProgressInfo.IsTotalEnabled() == false)
		return;												// then we should not waste time here...

	SingleEntryIterator sei;
	FS_OPERATION(sei.SetTo(entry));
	
	AccumulateItemsAndSize(sei, recursive);
}


void
FSContext::AccumulateItemsAndSize(EntryIterator &i, bool recursive) FS_THROW_FSEXCEPTION
{
	if (mProgressInfo.IsTotalEnabled() == false)
		return;												// then we should not waste time here...

	FS_SET_OPERATION(kCalculatingItemsAndSize);
	FS_ADD_POSSIBLE_ANSWER(fSkipOperation);

	try {
	
		CalculateItemsAndSizeRecursive(i, mProgressInfo, recursive);	// and then go on deeper
		
	} catch (FSException e) {
	
		i.Rewind();
		mProgressInfo.DisableTotals();
		
		FS_FILTER_EXCEPTION(kSkipOperation, NOP);
	}
	
	i.Rewind();
}

void
FSContext::CalculateItemsAndSizeRecursive(EntryIterator &i, ProgressInfo &progress_info, bool recursive) FS_THROW_FSEXCEPTION
{
	status_t rc = B_OK;

	EntryRef ref;
	while (i.GetNext(ref)) {
	
		CheckCancel();
	
		FS_SET_CURRENT_ENTRY(ref);

		BEntry entry;
		struct stat st;
		{
			FS_SET_OPERATION(kInspecting);
			
			FS_OPERATION(entry.SetTo(ref));
			FS_OPERATION(entry.GetStat(&st));
		}
		
		if (S_ISDIR(st.st_mode)) {
		
			progress_info.NewDirectory();
			
			if (recursive) {
				DirEntryIterator dei;
				i.RegisterNested(dei);
				
				FS_OPERATION(dei.SetTo(entry));
				
				CalculateItemsAndSizeRecursive(dei, progress_info);
			}
		} else if (S_ISREG(st.st_mode)) {
		
			progress_info.NewFile(st.st_size);
			
		} else if (S_ISLNK(st.st_mode)) {
		
			progress_info.NewLink();
		}
	}
}

int32
FSContext::GetSizeString(char *in_ptr, float size1, float size2)
{
	static const char *strings[] = { "TB", "GB", "MB", "kB", "B"};

	float num1 = 0, num2 = 0;
	uint32 i;
	
	char *ptr = in_ptr;
	
	bool print_first = true;
	if (size2 == 0) {
		size2 = size1;								// size2 is the base for calculations
		print_first = false;
	}
	
	float divider = 1024.*1024.*1024.*1024.;
	for (i = 0;  divider >= 1;  ++i) {
		num1 = size1 / divider;
		if ((num2 = size2 / divider) >= 1  ||  num2 == 0)
			break;
			
		divider /= 1024;
	}
	
	const char *format;
	if (print_first) {
		if (num1 == 0)
			format = "%.0f / ";
		else if (num1 < 1)
			format = "%.2f / ";
		else if (num1 < 10)
			format = "%.1f / ";
		else
			format = "%.0f / ";
	
		ptr += sprintf(ptr, format,	num1);
	}

	if (num2 == 0)
		format = "0";
	else if (num2 < 10)
		format = "%.1f %s";
	else
		format = "%.0f %s";

	// XXX check it... that string[i] may behave strange

	ptr += sprintf(ptr, format,	num2, (i < sizeof(strings) / sizeof(strings[0])) ? strings[i] : "BuG!");
	
	return (int32)(ptr - in_ptr);
}


status_t
FSContext::CalculateItemsAndSize(EntryIterator &i, ProgressInfo &progress_info) FS_NOTHROW
{
	PreparingOperation();
	
	FS_SET_OPERATION(kCalculatingItemsAndSize);
	
	try {

		OperationBegins();

		CalculateItemsAndSizeRecursive(i, progress_info);
		
	} catch (FSException e) {
	
		i.Rewind();
		progress_info.DisableTotals();
		
		return e;
	}
	
	i.Rewind();
	return B_OK;
}

void		// blindly copy the link content
FSContext::RawCopyLink(const BEntry &source_entry, BDirectory &target_dir, char *target_name) FS_THROW_FSEXCEPTION
{
	status_t rc;
	
	BSymLink source_link(&source_entry);
	{
		FS_REMOVE_POSSIBLE_ANSWER(fRetryOperation);
		FS_OPERATION(source_link.InitCheck());
	}
	
	char contents[B_PATH_NAME_LENGTH];
	source_link.ReadLink(contents, B_PATH_NAME_LENGTH);
	
	FS_OPERATION(target_dir.CreateSymLink(target_name, contents, 0));
	{
		// we actually created a link, copy additionals
		BNode source_node, new_node;
		FS_OPERATION(source_node.SetTo(&source_entry));
		FS_OPERATION(new_node.SetTo(&target_dir, target_name));
		CopyAdditionals(source_node, new_node);
		
		if (gTrackerSettings.UndoEnabled()) {
			BEntry entry(&target_dir, target_name);
			entry_ref new_ref;
			entry.GetRef(&new_ref);
			entry_ref orig_ref;
			source_entry.GetRef(&orig_ref);
			
			BMessage *undo = new BMessage(kCopySelectionTo);
			undo->AddRef("new_ref", &new_ref);
			undo->AddRef("orig_ref", &orig_ref);
			gUndoHistory.AddItemToContext(undo, this);
		}
	}

	NextEntryCreated(target_dir, target_name);
}

		// Precond:		mRootSourceDirRef is set, target_name is B_FILE_NAME_LENGTH long
bool	// Postcond:	source_entry will be unset, if the return code is false then the link target was copied and the caller should not count the link in the progress info
FSContext::CopyLink(EntryIterator &i, BEntry &source_entry, BDirectory &target_dir, char *target_name) FS_THROW_FSEXCEPTION
{
	status_t rc;
	entry_ref eref;
	bool count_this_link = true;
	
	for (;;) {									// make sure the target name is free
		BEntry entry;
		
		FS_OPERATION(entry.SetTo(&target_dir, target_name));
		if (entry.Exists() == false)
			break;

		char name_buf[B_FILE_NAME_LENGTH];
		
		FS_BACKUP_VARIABLE_AND_SET(mSourceNewName, target_name);
		FS_BACKUP_VARIABLE_AND_SET(mTargetNewName, name_buf);
		FS_BACKUP_VARIABLE_AND_SET(mTargetEntry, &entry);
		
		switch ((int32)Interaction(kTargetAlreadyExists)) {
			case kReplace: {
				FS_SET_OPERATION(kRemoving);
				FS_OPERATION(entry.Remove());
				break;
			}
			case kReplaceIfNewer:
				if (RemoveIfNewer(source_entry, target_dir, target_name) == true)
					break;
					
				FS_CONTROL_THROW(kSkipEntry);
				break;
				
			case kSuppliedNewNameForSource:
				break;

			case kRawCopyLink:
				RawCopyLink(source_entry, target_dir, target_name);
				return count_this_link;
				break;

			case kSuppliedNewNameForTarget: {
				FS_SET_OPERATION(kRenaming);
				FS_ADD_POSSIBLE_ANSWER(fIgnore);
				FS_OPERATION(entry.Rename(name_buf));
				break;
			}
			case kMakeUniqueName:
				MakeUniqueName(target_dir, target_name);
				break;
				
			case kMoveTargetToTrash:
				MoveToTrash(TargetDevice(), entry);
				break;
			
			case kRetryOperation:
				continue;
				
			default:	TRESPASS();
		}
	}


	if (mCopyLinksInstead == false) {		// then try to create a link
		BSymLink source_link(&source_entry);
		{
			FS_REMOVE_POSSIBLE_ANSWER(fRetryOperation);
			FS_OPERATION(source_link.InitCheck());
		}
		
		BEntry link_target;
	
		// if a realtive link points to an entry that is also copied in this operation then create link to the
		// copied entry instead of the one the original link pointed to. (typically links like ./doc/index.html)
		FS_OPERATION(source_entry.GetRef(&eref));
		FS_OPERATION_ETC(link_target.SetTo(&eref, true), rc != B_ENTRY_NOT_FOUND, NOP); // set to the target of the link
		if (rc == B_ENTRY_NOT_FOUND) {			// the target of this link is unreachable

			RawCopyLink(source_entry, target_dir, target_name);
			return count_this_link;

		} else {
		
			if (source_link.IsAbsolute()) {
				
				RawCopyLink(source_entry, target_dir, target_name);
				return count_this_link;

			} else {
			
				BDirectory root_source;
				{
					FS_SET_OPERATION(kInspecting);
					
					FS_OPERATION(root_source.SetTo(&mRootSourceDirNodeRef));
				}
				
				if (root_source.Contains(&link_target)  ||  mSourceDir.Contains(&link_target)) {
					
					RawCopyLink(source_entry, target_dir, target_name);
					return count_this_link;
					
				} else if (CreateLink(link_target, target_dir, target_name, true) == true) {	// relative
					
					// we actually created a link, copy additionals
					BNode source_node, new_node;
					FS_OPERATION(source_node.SetTo(&source_entry));
					FS_OPERATION(new_node.SetTo(&target_dir, target_name));
					CopyAdditionals(source_node, new_node);

					NextEntryCreated(target_dir, target_name);
				}
			}
		}
	}

	if (mCopyLinksInstead == true) {	// then we are in copy mode, maybe just switched. a simple else is not enough!
	
		FS_SET_OPERATION(kCopying);

		BEntry tmp_entry;
		FS_OPERATION(source_entry.GetRef(&eref));
		FS_OPERATION_ETC(tmp_entry.SetTo(&eref, true), rc != B_ENTRY_NOT_FOUND, NOP);	// set to the link target
		if (rc == B_ENTRY_NOT_FOUND) {			// the target of this link is unreachable

			RawCopyLink(source_entry, target_dir, target_name);
			return count_this_link;

		} else {
			EntryRef tmp_ref;							// set ref to the link target
			FS_OPERATION(tmp_entry.GetRef(&eref));
			tmp_ref.SetTo(eref);
			
			SourceDirSetter sds(this);
			
			FS_SET_CURRENT_ENTRY(tmp_ref);
	
			{											// initialize, set up dirs
				FS_SET_OPERATION(kInitializing);
				FS_OPERATION(sds.SetSourceDir(tmp_ref));
	
				AccumulateItemsAndSize(tmp_entry);		// accumlate size and counts of the link target
				mProgressInfo.SkipLink();
				count_this_link = false;
			}
			
			tmp_entry.Unset();							// spare fds in recursion
			source_entry.Unset();
	
			CopyEntry(i, tmp_ref, target_dir, target_name);
		}
	}

	return count_this_link;
}

		// Precond:		TargetVolume set, target_name is B_FILE_NAME_LENGTH long
void	// Postcond:	source_entry will be unset
FSContext::CopyDirectory(EntryIterator &i, BEntry &source_entry, BDirectory &target_dir, char *target_name) FS_THROW_FSEXCEPTION {

	status_t rc;
	BDirectory new_dir;

	FS_BACKUP_VARIABLE_AND_SET(mDirectoryCreatedByUs, false);

	try {

		FS_ADD_POSSIBLE_ANSWER(fSkipDirectory);

		bool target_dir_ok = false;
		while (target_dir_ok == false) {
			interaction icode = (interaction)0;
			
			switch (rc = new_dir.SetTo(&target_dir, target_name)) {
				case B_OK:
					icode = kDirectoryAlreadyExists;
					break;
				
				case B_ENTRY_NOT_FOUND: {
					FS_SET_OPERATION(kCreatingDirectory);
					CreateDirectory(&target_dir, target_name, &new_dir);
					target_dir_ok = true;
					mDirectoryCreatedByUs = true;
					break;
				}
				
				case B_BAD_VALUE: {	// some entry but not a dir, but double check it
					BEntry test(&target_dir, target_name);
					if (test.Exists()) {
						icode = kTargetAlreadyExists;
						break;
					} // else fall trough to default to trigger the error requester!
				}
				default:
					if (ErrorHandler(rc) == false)
						target_dir_ok = true;
					continue;
			}
			
			if (icode != 0) {
				char name_buf[B_FILE_NAME_LENGTH];
				
				BEntry target_entry;
				FS_OPERATION(target_entry.SetTo(&target_dir, target_name));
				
				FS_BACKUP_VARIABLE_AND_SET(mSourceNewName, target_name);
				FS_BACKUP_VARIABLE_AND_SET(mTargetNewName, name_buf);
				FS_BACKUP_VARIABLE_AND_SET(mTargetEntry, &target_entry);
		
				switch ((int32)Interaction(icode)) {
					case kEnterBoth:
						ASSERT(icode == kDirectoryAlreadyExists);
						target_dir_ok = true;
						continue;
						
					case kReplaceIfNewer:
						if (RemoveIfNewer(source_entry, target_dir, target_name) == true)
							break;
						
						FS_CONTROL_THROW(kSkipEntry);
						break;
						
					case kSuppliedNewNameForSource:
						continue;

					case kSuppliedNewNameForTarget: {
						BEntry e;
						FS_SET_OPERATION(kRenaming);
						FS_ADD_POSSIBLE_ANSWER(fIgnore);
						FS_OPERATION(new_dir.GetEntry(&e));
						FS_OPERATION(e.Rename(name_buf));
						continue;
					}
					
					case kMoveTargetToTrash: {
							BEntry entry;
							FS_OPERATION(entry.SetTo(&target_dir, target_name));
							
							MoveToTrash(TargetDevice(), entry);
						}
						continue;
	
					case kReplace: {
						FS_SET_OPERATION(kRemoving);
						FS_SKIPABLE_OPERATION(Remove(target_dir, target_name));
						continue;
					}
					case kMakeUniqueName:
	
						MakeUniqueName(target_dir, target_name);	// substitute target with the new unique name
						break;
						
					case kRetryOperation:
						continue;
					
					default: TRESPASS();
				}
			}
		}
	
		{
			BNode source, target;
			FS_OPERATION(source.SetTo(&source_entry));
			FS_OPERATION(target.SetTo(&target_dir, target_name));
			
			CopyAdditionals(source, target);
		}

		NextEntryCreated(target_dir, target_name);

		DirEntryIterator dei;
		i.RegisterNested(dei);
		
		FS_OPERATION(dei.SetTo(source_entry));
	
		source_entry.Unset();
		
		CopyRecursive(dei, new_dir);
		
	} catch (FSException e) {				// delete dir if it was skipped
		
		// kSkipEntry rarely reach this point, but be prepared if happens
		// if we created the target dir then delete it (it's half-done)
		if (mDirectoryCreatedByUs  &&  new_dir.InitCheck() == B_OK) {
			
			FS_SET_OPERATION(kCleaningUp);

// delete if ok	if (e == kSkipDirectory  ||  e == kSkipEntry  ||  e == kCancel) {

				FS_REMOVE_POSSIBLE_ANSWER(fRetryEntry | fSkipEntry | fSkipDirectory | fRetryOperation);

				switch  (Interaction(kAboutToCleanupDirectory)) {

					case kGoOnAndDelete: {
						BEntry entry;
						FS_OPERATION(new_dir.GetEntry(&entry));
						Remove(entry, kRemovingSkippedDir);
						break;
					}
					
					case kKeepIt:
						break;
					
					default:	TRESPASS();

				}
// delete if ok	}
		}

		if (e == kSkipDirectory) {		// convert kSkipDirectory to kSkipEntry
			ASSERT(IsAnswerPossible((answer_flags)(fSkipDirectory | fSkipEntry)));

			FS_CONTROL_THROW(kSkipEntry);
		} else
			throw;
	}
}

void
FSContext::CheckCancelInCopyFile() FS_THROW_FSEXCEPTION {
	try {
	
		CheckCancel();
		
	} catch (FSException e) {
		
		DEBUG_ONLY(
			if (IsAnswerPossible(fSkipDirectory) == false  &&  e == kSkipDirectory)
				TRESPASS();
		);
		
// XXX Interaction() if the file was not newly created, rather overwritten, and we are only half-done.

		// make sure we finish the current file if the directory, in which we copy the file, or the file itself
		// was not newly created by us (because we will not delete the dir in those cases and the file should be
		// finished cleanly otherwise the dir will be skrewed up)
		if (e == kSkipDirectory  &&  (mDirectoryCreatedByUs == false  &&  mFileCreatedByUs == false)) {
			// leave a mark that we should throw kSkipDirectory after finished the current file
			mThrowThisAfterFileCopyFinished = kSkipDirectory;
			return;
		}

		throw;
	}
}

void	// Precond:		mSameDevice is properly set (by SourceDirSetter), TargetDevice is set, target_name is B_FILE_NAME_LENGTH long
FSContext::CopyFile(BEntry &source_entry, BDirectory &target_dir, char *target_name) FS_THROW_FSEXCEPTION {

	status_t rc;
	BFile source_file, target_file;
	BEntry target_entry;
	
	FS_OPERATION(source_file.SetTo(&source_entry, B_READ_ONLY));
	
	FS_BACKUP_VARIABLE_AND_SET(mFileCreatedByUs, false);
	bool appending = false, touched = false;
	off_t target_size_before_append;
	
	try {

		off_t file_size;
		FS_OPERATION(source_file.GetSize(&file_size));
		
		for (;;) {			// set target according to feedback if already exists
		
			command cmd;
			char name_buf[B_FILE_NAME_LENGTH];
			
			FS_OPERATION(target_entry.SetTo(&target_dir, target_name));
			if (target_entry.Exists() == false) {
				mFileCreatedByUs = true;
				break;
			}
			
			FS_BACKUP_VARIABLE_AND_SET(mSourceNewName, target_name);
			FS_BACKUP_VARIABLE_AND_SET(mTargetNewName, name_buf);
			FS_BACKUP_VARIABLE_AND_SET(mTargetEntry, &target_entry);
			
			if (target_entry.IsFile())
				cmd = Interaction(kFileAlreadyExists);
			else
				cmd = Interaction(kTargetAlreadyExists);
				
			if (cmd == kReplace) {
				FS_SET_OPERATION(kRemoving);
				FS_SKIPABLE_OPERATION(Remove(target_dir, target_name));
				continue;
				
			} else if (cmd == kReplaceIfNewer) {
				
				if (RemoveIfNewer(source_entry, target_dir, target_name) == true)
					continue;
				
				FS_CONTROL_THROW(kSkipEntry);
				
			} else if (cmd == kSuppliedNewNameForSource) {
				continue;
			} else if (cmd == kSuppliedNewNameForTarget) {

				FS_SET_OPERATION(kRenaming);
				FS_ADD_POSSIBLE_ANSWER(fIgnore);
				FS_OPERATION(target_entry.Rename(name_buf));
				continue;

			} else if (cmd == kMoveTargetToTrash) {
			
				MoveToTrash(TargetDevice(), target_entry);
				FS_OPERATION(target_entry.SetTo(&target_dir, target_name));
				continue;
				
			} else if (cmd == kAppend) {
			
				ASSERT(target_entry.IsFile());
				FS_OPERATION(target_file.SetTo(&target_entry, B_WRITE_ONLY | B_OPEN_AT_END));
				FS_OPERATION(target_file.GetSize(&target_size_before_append));
				appending = true;
				break;
				
			} else if (cmd == kContinueFile) {
			
				ASSERT(target_entry.IsFile());
				FS_OPERATION(target_file.SetTo(&target_entry, B_WRITE_ONLY | B_OPEN_AT_END));
				off_t stmp;
				FS_OPERATION(target_file.GetSize(&stmp));
				ASSERT(stmp < file_size);
				FS_OPERATION(source_file.Seek(stmp, SEEK_SET));
				mProgressInfo.mTotalSize -= stmp;
				break;
				
			} else if (cmd == kMakeUniqueName) {
			
				MakeUniqueName(target_dir, target_name);	// substitute target with the new unique name
			} else if (cmd == kRetryOperation) {
				
				continue;
			} else {
				
				TRESPASS();
			}
		}
		
		CheckCancelInCopyFile();
		
		if (target_file.InitCheck() != B_OK) {
			touched = true;
			FS_OPERATION(target_file.SetTo(&target_dir, target_name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE));
			
			if (gTrackerSettings.UndoEnabled()) {
				BEntry entry(&target_dir, target_name);
				entry_ref new_ref;
				entry.GetRef(&new_ref);
				entry_ref orig_ref;
				source_entry.GetRef(&orig_ref);
								
				BMessage *undo = new BMessage(kCopySelectionTo);
				undo->AddRef("new_ref", &new_ref);
				undo->AddRef("orig_ref", &orig_ref);
				gUndoHistory.AddItemToContext(undo, this);
			}
		}
		
		source_entry.Unset();		// spare some fd's
		
		NextEntryCreated(target_dir, target_name);
		
		CheckFreeSpaceOnTarget(file_size, kNotEnoughFreeSpaceForThisFile);
		
		#if FS_USE_SET_FILE_SIZE
			if (mTargetVolume.IsPersistent()  &&  mTargetVolume.IsShared() == false) {
				// XXX is it possible to open for writing twice? and if so what hapens if someone is writing in the file?
				FS_SET_OPERATION(kSettingFileSize);
				touched = true;
				if (mFileCreatedByUs) {
					FS_OPERATION(target_file.SetSize(file_size));		// force file size at the beginning
				} else {
					#if 0 // XXX
					off_t size_tmp;
					FS_OPERATION(target_file.GetSize(&size_tmp));		// XXX BUUUUUUUUUUUUUUUUUUUUUG in beos
					FS_OPERATION(target_file.SetSize(size_tmp + file_size)); // if compiled in the file size will grow again at the first call after SetSize
																		// so the file size will be size_tmp + 2*file_size iirc
					#endif
				}
			}
		#endif // FS_USE_SET_FILE_SIZE
		
		touched = true;

		FS_INIT_COPY_PROGRESS(file_size);	// it's smart and will restore old progress values if
											// it's destructed before the end of the file. required
											// partly to keep the progress info in sync
		
		if (mSameDevice) {
		
			SuggestBufferSize(file_size);
			CopyFileInnerLoop(source_file, target_file, file_size);
			
		} else if (file_size < kMultiThreadedFileSizeLimit) {
		
			CopyFileInnerLoop(source_file, target_file, file_size);
			
		} else {

			SetBufferSize(0);
			CopyFileInnerLoopTwoDevices(source_file, target_file);
		}

		#if DEBUG
		{
			off_t size;
			target_file.GetSize(&size);
			ASSERT(size == target_file.Position());
		}
		#endif

	} catch (FSException e) {
	
		if (touched) {
			
			FS_SET_OPERATION(kCleaningUp);
			FS_REMOVE_POSSIBLE_ANSWER(fRetryEntry | fSkipEntry | fSkipDirectory | fRetryOperation);
			
			if ((mFileCreatedByUs  ||  ! IsThrowableCommand(static_cast<command>(e)))  &&   // or some unknown exception
					target_entry.Exists()) {
						
				switch (Interaction(kAboutToCleanupFile)) {
					case kGoOnAndDelete:
						target_entry.Remove();
						break;
					
					case kKeepIt:
						break;
					
					default:	TRESPASS();
				}
			} else if (appending) {
				switch (Interaction(kAboutToCleanupAppending)) {
					case kResetSize:
						target_file.SetSize(target_size_before_append);
						break;
					
					case kKeepIt:
						break;
					
					default:	TRESPASS();
				}
			}
		}

		ThrowIfNecessary(mThrowThisAfterFileCopyFinished);

		throw;
	}
	
	CopyAdditionals(source_file, target_file);

	ThrowIfNecessary(mThrowThisAfterFileCopyFinished);
}

void
FSContext::CopyFileInnerLoopTwoDevices(BFile &source_file, BFile &target_file) FS_THROW_FSEXCEPTION {
	status_t rc;

	mMainThreadID = find_thread(0);

	sem_id reader_sem, writer_sem, chunk_sem;	// unfortunately we can not use BLocker for the chunk lock, because we need a safe method for cancelling: delete_sem()
	
	ASSERT(kReaderSemCount > 0);
	
	CreateSemScoped s1(reader_sem, kReaderSemCount, "filecopy: reader_sem", this);
	CreateSemScoped s2(writer_sem, 0, "filecopy: writer_sem", this);
	CreateSemScoped s3(chunk_sem, 1, "filecopy: chunk_sem", this);

	FS_OPERATION(s1.DoIt());
	FS_OPERATION(s2.DoIt());
	FS_OPERATION(s3.DoIt());

	mChunkList = new ChunkList();
	mWasMultithreaded = true;

	LaunchInNewThread("FileCopy writer thread", B_NORMAL_PRIORITY, this, &FSContext::CopyFileWriterThread, &target_file,
						mChunkList, chunk_sem, reader_sem, writer_sem);
						
	try {
	
		CopyFileReaderThread(&source_file, mChunkList, chunk_sem, reader_sem, writer_sem);
		
	} catch (...) {
	
		// XXX take a good look here again...
		
		acquire_sem(chunk_sem);
		delete_sem(chunk_sem);	// explicitly delete it first, so the writer will quit peacefully, without accessing the soon to be deleted chunk list
		chunk_sem = 0;
		
		delete mChunkList;
		mChunkList = 0;
		
		throw;
	}
	
	acquire_sem_etc(reader_sem, kReaderSemCount - 1, 0, 0);	// wait for writer to flush buffers

	acquire_sem(chunk_sem);
	delete_sem(chunk_sem);	// explicitly delete it first, so the writer will quit peacefully, without accessing the soon to be deleted chunk list
	chunk_sem = 0;

	delete mChunkList;
	mChunkList = 0;
}

void
FSContext::CopyFileReaderThread(BFile *file, ChunkList *chunks, sem_id chunk_sem, sem_id reader_sem,
									sem_id writer_sem) FS_THROW_FSEXCEPTION {

	status_t rc;
	size_t chunk_size = kMinChunkSize;
	ssize_t this_chunk;
	uint8 *buffer = 0;
	pair<uint8 *, size_t> *chunk_struct = 0;
	
	system_info si;
	get_system_info(&si);
	size_t max_chunk_size = (si.max_pages * B_PAGE_SIZE) / kMaxChunkSizeDivider;

//	FS_SET_OPERATION(kReadingFile);	// commented out intentionally, there were two threads playing with the operation stack

	try {
	
		for (;;) {
		
			ThrowIfNecessary(mThrowThisInMainThread);
			
			CheckCancelInCopyFile();
	
			bool skip_recalc = false;
			bigtime_t start_time = system_time();
			
			buffer = new uint8[chunk_size];

			#if FS_PRINT_BUFFER_INFO
				printf("Chunk size: %.2f kB\n", (float)chunk_size / 1024);
			#endif
						
			while ((this_chunk = file -> Read(buffer, chunk_size)) < 0) {

				ThrowIfNecessary(mThrowThisInMainThread);
				
				skip_recalc = true;
				if (ErrorHandler(this_chunk) == false) {
					TRESPASS();							// this is an illegal false return from ErrorHandler
				}
			}
			
			mProgressInfo.ReadProgress(this_chunk);
			
			if (skip_recalc == false  &&  this_chunk != 0)
				CalculateNewChunkSize(chunk_size, max_chunk_size, start_time);
	
			{
				#if FS_MONITOR_THREAD_WAITINGS
					RunStopWatch rsw(mReaderThreadWaiting);
				#endif

				// acquire reader sem
				// reader is some locks ahead, so it will read the next few chunks while the writer is writing the previous ones
				while ((rc = acquire_sem_etc(reader_sem, 1, B_CAN_INTERRUPT, 0)) != B_OK) {
				
					switch (rc) {
					
						case B_BAD_SEM_ID:
						case B_INTERRUPTED:
							ThrowIfNecessary(mThrowThisInMainThread);
							break;
						
				//		case B_BAD_VALUE:	// should not happen, but be bulletproof; skipping is a reasonable thing here
						default:
							FS_CONTROL_THROW(kSkipEntry);
					}
				}
			}
			
			if (this_chunk == 0)		// then we are done; the above acquire will return only if writer finished, so we can safely
				break;				// return and free up local shared variables

			chunk_struct = new pair<uint8 *, size_t>;
			chunk_struct -> first = buffer;
			buffer = 0;
			chunk_struct -> second = this_chunk;
			
			// add this chunk to the shared list
			while ((rc = acquire_sem(chunk_sem)) == B_INTERRUPTED);
			if (rc != B_OK) {
				FS_CONTROL_THROW(kSkipEntry);
			}
			
			chunks -> push_back(chunk_struct);
			chunk_struct = 0;
			
			release_sem_etc(chunk_sem, 1, B_DO_NOT_RESCHEDULE);
			
			// release writer thread to write this chunk
			release_sem_etc(writer_sem, 1, B_DO_NOT_RESCHEDULE);	// this way reschedule will happen while blocking in the io call
		}
	
	} catch (...) {
		// no need to catch FSException here, because CreateSemScoped will delete the sem and
		// that will force the writer thread to exit
	
		delete [] buffer;
		delete chunk_struct;
		
		throw;
	}
	
	delete [] buffer;
	delete chunk_struct;
}

void	// file is a pointer to skip unneccesary copy ctor in LaunchInNewThread
FSContext::CopyFileWriterThread(BFile *file, ChunkList *chunks, sem_id chunk_sem, sem_id &reader_sem,
									sem_id writer_sem) FS_NOTHROW {

	mWriterThreadRunning = true;

	pair<uint8 *, size_t> *chunk_struct = 0;

	try {
	
//		FS_SET_OPERATION(kWritingFile);	// commented out intentionally, there were two threads playing with the operation stack
		status_t rc;

		for (;;) {
			
			{
				#if FS_MONITOR_THREAD_WAITINGS
					RunStopWatch rsw(mWriterThreadWaiting);
				#endif

				// wait for the next chunk to be written
				while ((rc = acquire_sem(writer_sem)) == B_INTERRUPTED);
				if (rc != B_OK)
					goto exit;		// after acquire_sem failed we should quit, 'couse the main thread is probably already waiting for us
			}

// XXX
//printf("%ld buffers in queue\n", chunks -> size());
			
			// get next chunk form the shared list
			while ((rc = acquire_sem(chunk_sem)) == B_INTERRUPTED);
			if (rc != B_OK)
				goto exit;
			
			ASSERT(chunks -> size() > 0);
				
			chunk_struct = chunks -> front();
			chunks -> erase(chunks -> begin());

			release_sem_etc(chunk_sem, 1, B_DO_NOT_RESCHEDULE);
			
			size_t write_pos = 0;
			ssize_t this_chunk;
			
			uint8 *buffer_pos = chunk_struct -> first;

			while (write_pos < chunk_struct -> second) {
			
				while ((this_chunk = file -> Write(buffer_pos, chunk_struct -> second)) < 0) {
	
					if (ErrorHandler(this_chunk) == false)
						TRESPASS();							// this is an illegal false return from ErrorHandler
				}
				
				mProgressInfo.WriteProgress(this_chunk);
				buffer_pos += this_chunk;
				write_pos += this_chunk;
			}
			
			delete [] chunk_struct -> first;
			delete chunk_struct;
			chunk_struct = 0;
			
			release_sem_etc(reader_sem, 1, B_DO_NOT_RESCHEDULE);
		}
		
	} catch (FSException e) {
	
		ASSERT(mThrowThisInMainThread == kInvalidCommand);	// XXX test these lines if they properly move the exception to the main thread!!!
		
		// move this exception into the main thread
		mThrowThisInMainThread = static_cast<command>(e);
#if 1
		delete_sem(reader_sem);					// release reader thread
		reader_sem = 0;							// spare a kernel call
#elif 0
		send_signal(mMainThreadID, SIGCONT);	// seems to be useless, enven with B_CAN_INTERRUPT at acquire_sem()
#elif 0
		resume_thread(mMainThreadID);			// according to the book this is the same as send_signal(thid, SIGCONT); doesn't work either
#endif
	}

exit:

	delete chunk_struct;
	mWriterThreadRunning = false;
}

void
FSContext::CopyFileInnerLoop(BFile &source_file, BFile &target_file, off_t file_size) FS_THROW_FSEXCEPTION {
	off_t read_pos = 0, write_pos;
	size_t chunk_size = kMinChunkSize;
	
	SuggestBufferSize(chunk_size);
	ASSERT(chunk_size <= BufferSize());
	bool keep_on = true;
	
	while (keep_on) {
	
		CheckCancelInCopyFile();
		
		write_pos = read_pos;								// store current position
		
		// buffer should be enough for one second of data
		SuggestBufferSize((mSameDevice) ? chunk_size * (1000000 / kProgressUpdateRate) : chunk_size);
		if (chunk_size > BufferSize())
			chunk_size = BufferSize();

		uint8 *buffer_pos;
		ssize_t this_chunk;
		
		{
			FS_BACKUP_VARIABLE_AND_SET(mBufferReleasable, false);
			
			{	// fill the buffer in chunks that are read for at most kProgressUpdateRate usecs
				buffer_pos = Buffer();
				
				#if FS_SAME_DEVICE_OPT
					bool synced = false;
				#endif
				
				size_t space_left;
				// as long as there's enough space in the buffer
				while ((space_left = (size_t)(BufferSize() - (buffer_pos - Buffer()))) >= (chunk_size / 8)  ||  space_left == BufferSize()) {
					
					bool skip_recalc = false;
					bigtime_t start_time = system_time();
					
					{
						FS_SET_OPERATION(kReadingFile);
		
						CheckCancelInCopyFile();
		
						while ((this_chunk = source_file.Read(buffer_pos, (space_left > chunk_size) ? chunk_size : space_left)) < 0) {
							skip_recalc = true;
							if (ErrorHandler(this_chunk) == false)
								TRESPASS();							// this is an illegal false return from ErrorHandler
						}
						
						if (this_chunk == 0) {
							keep_on = false;
							break;
						}
						
						mProgressInfo.ReadProgress(this_chunk);
						read_pos += this_chunk;
						buffer_pos += this_chunk;
					}				
	
					if (mSameDevice == false) {					// then write out this chunk right now
	
						buffer_pos = Buffer();
						FS_SET_OPERATION(kWritingFile);
						
						while (write_pos < read_pos) {
						
							CheckCancelInCopyFile();
							
							while ((this_chunk = target_file.Write(buffer_pos,
									(read_pos - write_pos > chunk_size) ? chunk_size : read_pos - write_pos)) < 0) {
	
								skip_recalc = true;
								if (ErrorHandler(this_chunk) == false)
									TRESPASS();							// this is an illegal false return from ErrorHandler
							}
							
							mProgressInfo.WriteProgress(this_chunk);
							buffer_pos += this_chunk;
							write_pos += this_chunk;
						}
						
						buffer_pos = Buffer();							// reset buffer ptr
					}
	
					if (!skip_recalc  &&  space_left >= chunk_size)
						CalculateNewChunkSize(chunk_size, file_size, start_time);
					
					#if FS_SAME_DEVICE_OPT
						if (mSameDevice  &&  synced == false  &&  file_size > kSyncLowerLimit  &&  chunk_size < 128 * 1024) {
							synced = true;
							target_file.Sync();
							#if FS_PRINT_BUFFER_INFO
								puts("Sync()");
							#endif
						}
					#endif
					
					if (mSameDevice == false)
						break;	// only one chunk per loop, more is just useless wasting of ram
				}
			}
			
			// write out the buffer if something is left
			if (write_pos < read_pos) {
				buffer_pos = Buffer();
				FS_SET_OPERATION(kWritingFile);
				while (write_pos < read_pos) {
				
					CheckCancelInCopyFile();
					
					while ((this_chunk = target_file.Write(buffer_pos,
							(read_pos - write_pos > chunk_size) ? chunk_size : read_pos - write_pos)) < 0) {
		
						if (ErrorHandler(this_chunk) == false)
							TRESPASS();							// this is an illegal false return from ErrorHandler
					}
					
					mProgressInfo.WriteProgress(this_chunk);
					buffer_pos += this_chunk;
					write_pos += this_chunk;
					
					#if SAME_DEVICE_OPT
					if (mSameDevice  &&  file_size > kSyncLowerLimit)
						target_file.Sync();
					#endif
				}
			}
		}
	}
}

void
FSContext::CalculateNewChunkSize(size_t &chunk_size, off_t upper_limit, bigtime_t start_time) FS_NOTHROW {

	chunk_size = 256 * 1024;
	return;
	
	size_t old_chunk_size = chunk_size;
	chunk_size = (size_t) ((float)chunk_size / (system_time() - start_time) * kProgressUpdateRate);
	
	if (chunk_size > old_chunk_size * 2)			// allow limited growth only
		chunk_size = old_chunk_size * 2;
	
	if (chunk_size > upper_limit)					// limit with file_size
		chunk_size = upper_limit + (16 * 1024 - 1);
		
	if (chunk_size < kMinChunkSize)
		chunk_size = kMinChunkSize;
	else
		chunk_size -= chunk_size % (16 * 1024);		// round to 16k boundary
	
	#if FS_PRINT_BUFFER_INFO
		printf("Current chunk size: %ld\n", (int32)chunk_size);
	#endif
}

bool	// Precond: mSourceDir is set
FSContext::CopyEntry(EntryIterator &i, EntryRef &ref, BDirectory &target_dir, const char *i_target_name,
						bool first_run) FS_THROW_FSEXCEPTION {

	status_t rc;
	
	if (mSourceDir == target_dir  &&  i_target_name == 0) {
		for (;;)
			Interaction(kSourceAndTargetIsTheSame);
	}
	
	if (gTrackerSettings.UndoEnabled()) {
		gUndoHistory.SetSourceForContext(mSourceDir, this);
		gUndoHistory.SetTargetForContext(target_dir, this);
	}
	
	char target_name[B_FILE_NAME_LENGTH];
	
	// if no name is given then copy entry with it's original name, make a local copy if the input for possible MakeOriginalName calls
	(i_target_name == 0)  ?  strcpy(target_name, ref.Name())  :  strcpy(target_name, i_target_name);
	
	struct stat statbuf;
	
// get stat
	BEntry entry;
	{
		FS_SET_OPERATION(kInspecting);
		
		FS_OPERATION(entry.SetTo(&mSourceDir, ref.Name()));
		
		FS_OPERATION(entry.GetStat(&statbuf));
		SetCurrentEntryType(statbuf.st_mode);
	}

// Check type
	try {		// this try must be here because CopyEntry may be called form places other then CopyRecursive

		CheckCancel();

		if (first_run)				// it may only cause problem when we are in the first recursion
			CheckTargetDirectory(entry, target_dir, S_ISDIR(statbuf.st_mode));
	
		if (S_ISDIR(statbuf.st_mode)) {
	
			CopyDirectory(i, entry, target_dir, target_name);	
			mProgressInfo.DirectoryDone();
	
		} else if (S_ISLNK(statbuf.st_mode)) {
			
			if (CopyLink(i, entry, target_dir, target_name))
				mProgressInfo.LinkDone();
			
		} else if (S_ISREG(statbuf.st_mode)) {
			
			CopyFile(entry, target_dir, target_name);
			mProgressInfo.FileDone();
			
		} else {
	
			Interaction(kIrregularFile);
		}
	} catch (FSException e) {
	
		if (e == kSkipEntry) {			// if we skip the entry then alter the progress indicator
			if (S_ISDIR(statbuf.st_mode))
				mProgressInfo.SkipDirectory();
			else if (S_ISLNK(statbuf.st_mode))
				mProgressInfo.SkipLink();
			else if (S_ISREG(statbuf.st_mode))
				mProgressInfo.SkipFile(statbuf.st_size);
				
			return false;
		}
		
		throw;
	}
	return true;
}

void
FSContext::CopyRecursive(EntryIterator &i, BDirectory &target_dir, bool first_run) FS_THROW_FSEXCEPTION {

	status_t rc = 0;

	SourceDirSetter sds(this);	// this is required to be able to copy relative links better
								// (so that if their target is also copied then they point to
								// the copied one). and to set mSourceDir always to the proper dir.
	
	EntryRef ref;
	bool retry_entry = false;
	
	while (retry_entry  ||  i.GetNext(ref)) {
		retry_entry = false;
		
		try {
		
			FS_SET_CURRENT_ENTRY(ref);
	
			{							// initialize, set up dirs
				FS_SET_OPERATION(kInitializing);
				
				FS_OPERATION(sds.SetSourceDir(ref));
				sds.SetRootSourceDir(first_run);
			}

			CopyEntry(i, ref, target_dir, 0, first_run);
			
		} catch (FSException e) {

			FS_FILTER_EXCEPTION_2(	kSkipEntry,		mProgressInfo.SkipEntry(); mProgressInfo.DisableTotals(),
									kRetryEntry,	retry_entry = true;
													mProgressInfo.SkipEntry();
													mProgressInfo.DisableTotals();
								);
		}
	}
}

status_t
FSContext::CopyTo(EntryIterator *i, BDirectory &target_dir, bool async) FS_NOTHROW {

	if (async) {
		#if FS_CONFIG_MULTITHREADED
			LaunchInNewThread("FS copy thread", B_NORMAL_PRIORITY, this, &FSContext::CopyTo, i, target_dir, false);
		#else
			return CopyTo(i, target_dir, false);
		#endif		
	} else {
		auto_ptr<EntryIterator> _i(i);
		return CopyTo(*i, target_dir);
	}
	
	return B_OK;
}

// this one is for the New template copy: no progress indication, and not async
status_t	// Precond: target name must be a B_FILE_NAME_LENGTH sized mutable buffer!
FSContext::CopyFileTo(entry_ref &_ref, BDirectory &target_dir, char *target_name) FS_NOTHROW {

	EntryRef ref(_ref);

	FS_SET_OPERATION(kCopying);
	FS_SET_CURRENT_ENTRY(ref);
	
	try {
	
		PreparingOperation();

		status_t rc;
	
		FS_OPERATION(target_dir.InitCheck());
		FS_OPERATION(target_dir.GetNodeRef(&mRootTargetDirNodeRef));

		SourceDirSetter sds(this);
		
		BEntry entry;
		FS_OPERATION(entry.SetTo(ref));
		
		{
			FS_SET_OPERATION(kInitializing);
			
			SetTargetVolume(target_dir);
			
			FS_OPERATION(sds.SetSourceDir(ref));
			sds.SetRootSourceDir(true);
			
			struct stat stat;
			FS_OPERATION(entry.GetStat(&stat));
			CheckFreeSpaceOnTarget(stat.st_size, kNotEnoughFreeSpace);
		}
		
		OperationBegins();

		CopyFile(entry, target_dir, target_name);
		
	} catch (FSException e) {
		if (e != kSkipEntry)
			return e;
	}
	
	return B_OK;
}

status_t
FSContext::CopyTo(EntryIterator &i, BDirectory &target_dir) FS_NOTHROW {

	try {
	
		PreparingOperation();

		status_t rc;
	
		FS_OPERATION(target_dir.InitCheck());
		FS_OPERATION(target_dir.GetNodeRef(&mRootTargetDirNodeRef));
		
		FS_SET_OPERATION(kCopying);

		ResetProgressIndicator();
		mProgressInfo.EnableTotalSizeProgress();	// indicate that we will be dealing with sizes, not only entries
		InitProgressIndicator();

		{
			FS_SET_OPERATION(kInitializing);
			
			SetTargetVolume(target_dir);
			
			AccumulateItemsAndSize(i);
		}
				
		FS_ADD_POSSIBLE_ANSWER(fRetryEntry + fSkipEntry);
		
		CheckFreeSpaceOnTarget(mProgressInfo.mTotalSize, kNotEnoughFreeSpace);

		OperationBegins();

		CopyRecursive(i, target_dir, true);
		
	} catch (FSException e) {
		return e;
	}
	
	return B_OK;
}


void	// Precond: SetTargetVolume() was called, mRootSourceDir is needed for CopyLink
FSContext::MoveToRecursive(EntryIterator &i, BDirectory &target_dir, bool first_run) FS_THROW_FSEXCEPTION {

	status_t rc;
	
	SourceDirSetter sds(this);
	
	bool call_callback = true;

	EntryRef ref;
	BEntry entry;
	char name_buf[B_FILE_NAME_LENGTH];
	bool retry_entry = false;
	int32 askOnceOnly = kNotConfirmed;
	
	while (retry_entry  ||  i.GetNext(ref)) {
		retry_entry = false;
		bool count_this_entry = true;
				
		try {

			FS_SET_CURRENT_ENTRY(ref);
					
			CheckCancel();

			{
				FS_SET_OPERATION(kInitializing);
				FS_OPERATION(sds.SetSourceDir(ref));
				sds.SetRootSourceDir(first_run);
				ASSERT_WITH_MESSAGE((mSourceDir != target_dir), "FSContext::MoveTo() was called with sourcedir = destinationdir");
				if (gTrackerSettings.UndoEnabled()) {
					gUndoHistory.SetSourceForContext(mSourceDir, this);
					gUndoHistory.SetTargetForContext(target_dir, this);
				}
			}

			FS_OPERATION(entry.SetTo(ref));
			FS_OPERATION(entry.GetName(name_buf));	// get source name

			bool keep_on_trying = true;

			if (first_run)				// it may only cause problem when we are in the first recursion
				CheckTargetDirectory(entry, target_dir);

			struct stat statbuf;
			
			{
				FS_SET_OPERATION(kInspecting);
				
				FS_OPERATION(entry.GetStat(&statbuf));
				SetCurrentEntryType(statbuf.st_mode);				// totally optional, but if we have the info...
			}
			
			if (S_ISLNK(statbuf.st_mode)) {						// links special cased
				bool copy_link = false;
				
				if (ref.Device() != TargetDevice()) {					// a link is going to a different volume
					copy_link = true;								// checking here we can show the link-different-volume-move requester straight
#if FS_RELATIVE_LINKS_KEEP_TARGET_WHEN_MOVED
				} else {											// a link stays on the same volume
					BSymLink link(&entry);							// check if relative, if so keep target
					{
						FS_REMOVE_POSSIBLE_ANSWER(fRetryOperation);
						FS_OPERATION(link.InitCheck());
					}
					
					if (link.IsAbsolute() == false) {
						// XXX there's no way in the Be API yet to set the link "data", where it points. so we must call CopyLink
						// that will create a new link and copy the attributes instead of moving the link to the target dir
						// and changing where it points.
						
						copy_link = true;
					}
#endif // FS_RELATIVE_LINKS_KEEP_TARGET_WHEN_MOVED
				}
				
				if (copy_link) {
					count_this_entry = CopyLink(i, entry, target_dir, name_buf);
	
					try {										// remove source link
						FS_ADD_POSSIBLE_ANSWER(fSkipOperation);
						FS_SET_OPERATION(kRemoving);
						
						FS_OPERATION(entry.SetTo(ref));			// CopyLink unsets it's source
						FS_OPERATION_ETC(entry.Remove(), rc != B_ENTRY_NOT_FOUND, NOP);
					} catch (FSException e) {
					
						FS_FILTER_EXCEPTION(kSkipOperation, NOP);
					}
					
					keep_on_trying = false;						// this one is done, leve it alone
				}
			}
			
// if cross-volume move

			if (keep_on_trying  &&  ref.Device() != TargetDevice()) {	// target is a different volume
			
				command cmd = Interaction(kCannotMove);
				
				// we can only get here if user wants some kind of copy
				FS_SET_OPERATION(kCopying);
					SourceDirSetter sds(this);	// required for the deletion below
				
				mProgressInfo.EnableTotalSizeProgress();	// indicate that we will be dealing with sizes, not only entries
				
				{											// initialize, set up dirs
					FS_SET_OPERATION(kInitializing);
					FS_OPERATION(sds.SetSourceDir(ref)); // required for the deletion below
		
					AccumulateItemsAndSize(entry);			// accumlate size and counts of the source entry
					mProgressInfo.SkipEntry();				// the current entry we are moving
					count_this_entry = false;
				}
				
				bool happened = false;
				try {
				
					happened = CopyEntry(i, ref, target_dir);				// copy entry
					
				} catch (FSException e) {
					
					FS_FILTER_EXCEPTION(kSkipEntry, mProgressInfo.SkipEntry(); mProgressInfo.DisableTotals());
				}
				
				call_callback = false;								// CopyEntry already did that for us

				if (happened) {
					switch ((int32)cmd) {
					
						case kCopyInsteadAndTrash:
							FS_OPERATION(entry.SetTo(ref));
							MoveToTrash(ref.Device(), entry);
							break;
							
						case kCopyInsteadAndDelete: {
							FS_SET_OPERATION(kRemoving);
							FS_OPERATION(entry.SetTo(ref));
							FS_SKIPABLE_OPERATION(Remove(entry));
							break;
						}
						
						case kCopyInstead:
							break;
						
						default: TRESPASS();
					}
				}

				keep_on_trying = false;							// this one is done, leave it alone
			}
			
// simple fs move from here

			if (keep_on_trying) {
	
				bool clobber = false;
				char *name = 0;
				char name_buf_for_target[B_FILE_NAME_LENGTH];
				
				// save redo data
				entry_ref orig_ref;
				if (gTrackerSettings.UndoEnabled())
					entry.GetRef(&orig_ref);
				
				while (keep_on_trying) {
					if (!ConfirmChangeIfWellKnownDirectory(&entry, "move", false, &askOnceOnly)) {
						FS_CONTROL_THROW(kSkipEntry);
						break;
					}
					
					switch ((rc = entry.MoveTo(&target_dir, name, clobber))) {
						default:
							if (ErrorHandler(rc) == false)
								keep_on_trying = false;
							continue;
						
					#if DEBUG
						case B_CROSS_DEVICE_LINK:
						
							TRESPASS();
							// XXX case: can not move to a different volume
							break;
					#endif								

						case B_OK:
							keep_on_trying = false;
							if (gTrackerSettings.UndoEnabled()) {
								entry_ref new_ref;
								entry.GetRef(&new_ref);
								
								BMessage *undo = new BMessage(kMoveSelectionTo);
								undo->AddRef("new_ref", &new_ref);
								undo->AddRef("orig_ref", &orig_ref);
								gUndoHistory.AddItemToContext(undo, this);
							}
							continue;
						
						case B_FILE_EXISTS: {
						
							interaction icode = (interaction)0;
							
							BEntry target_entry;
							FS_OPERATION(target_entry.SetTo(&target_dir, name_buf));
							
							if (entry.IsDirectory()  &&  target_entry.IsDirectory())
									icode = kDirectoryAlreadyExists;	// special case: target and source are both dirs
	
							if (icode == 0) 
								icode = kTargetAlreadyExists;
	
							name = name_buf;
							
							FS_BACKUP_VARIABLE_AND_SET(mSourceNewName, name);
							FS_BACKUP_VARIABLE_AND_SET(mTargetNewName, name_buf_for_target);
							FS_BACKUP_VARIABLE_AND_SET(mTargetEntry, &target_entry);
							
							switch ((int32)Interaction(icode)) {
								case kReplace: {
									FS_SET_OPERATION(kRemoving);
									FS_SKIPABLE_OPERATION(Remove(target_dir, name_buf));
									continue;
								}
								case kReplaceIfNewer:
									if (RemoveIfNewer(entry, target_dir, name_buf) == true)
										continue;
									
									FS_CONTROL_THROW(kSkipEntry);
									break;
									
								case kSuppliedNewNameForSource:
									continue;
									
								case kSuppliedNewNameForTarget: {
									FS_SET_OPERATION(kRenaming);
									
									FS_ADD_POSSIBLE_ANSWER(fIgnore);
	
									BEntry e;
									FS_OPERATION(e.SetTo(&target_dir, name));
									if (e.Exists()) {
										FS_OPERATION(e.Rename(name_buf_for_target));
									}
									continue;
								}
								case kMoveTargetToTrash: {
								
										BEntry entry;
										FS_OPERATION(entry.SetTo(&target_dir, ref.Name()));
										
										MoveToTrash(TargetDevice(), entry);
									}
									continue;
									
								case kMakeUniqueName:
									MakeUniqueName(target_dir, name_buf);
									name = name_buf;
									continue;
	
								case kEnterBoth:	{	// we should go deeper recursively and keep asking
									
									ASSERT(icode == kDirectoryAlreadyExists);
	
									DirEntryIterator dei;
									i.RegisterNested(dei);
									
									FS_OPERATION(dei.SetTo(entry));
									entry.Unset();
									
									BDirectory deeper_target;
									FS_OPERATION(deeper_target.SetTo(&target_dir, ref.Name()));
									
									{			// add new entries to the total progress count
										FS_SET_OPERATION(kInitializing);
										AccumulateItemsAndSize(dei, false);	// not recursive, count entries in the source dir only
										mProgressInfo.SkipEntry();
										count_this_entry = false;
									}
									
									MoveToRecursive(dei, deeper_target);
																	
									call_callback = false;				// we don't need to set the pose location in this case

									if (entry.Exists()) {				// someone might have deleted
									
										BDirectory dir;
										FS_OPERATION(dir.SetTo(&entry));
										
										if (dir.CountEntries() == 0)
											entry.Remove();				// should not leave an empty dir
										
										// otherwise there are entries left in the dir
										
										// XXX what to do here???
									}
									
									keep_on_trying = false;
									
									continue;
								}
								
								case kRetryOperation:
									continue;
								
								default: TRESPASS();
							}
							break;
						}
					}
				}
			}
						
			if (call_callback)	// XXX it will set the pose info even if we will end up in sweeping together two dirs, read comment at the beginning
				NextEntryCreated(target_dir, name_buf, &target_dir);

			if (count_this_entry == true)
				mProgressInfo.EntryDone();
	
		} catch (FSException e) {
			
			FS_FILTER_EXCEPTION_2(	kSkipEntry,		mProgressInfo.SkipEntry(),
									kRetryEntry,	retry_entry = true;
													mProgressInfo.SkipEntry();
								);
		}
	}
}

status_t
FSContext::MoveTo(EntryIterator *i, BDirectory &target_dir, bool async) FS_NOTHROW {

	if (async) {
		#if FS_CONFIG_MULTITHREADED
			LaunchInNewThread("FS move thread", B_NORMAL_PRIORITY, this, &FSContext::MoveTo, i, target_dir, false);
		#else
			return MoveTo(i, target_dir, false);	// XXX
		#endif		
	} else {
	
		auto_ptr<EntryIterator> _i(i);
		
		return MoveTo(*i, target_dir);
	}
	
	return B_OK;
}

status_t
FSContext::MoveTo(EntryIterator &i, BDirectory &target_dir) FS_NOTHROW {

	if (IsTrashDir(target_dir))				// if target is a trash then switch to MoveToTrash
		return MoveToTrash(i);

	try {
	
		PreparingOperation();
		
		status_t rc;
	
		FS_OPERATION(target_dir.InitCheck());
		FS_OPERATION(target_dir.GetNodeRef(&mRootTargetDirNodeRef));
	
		FS_SET_OPERATION(kMoving);

		// set target device
		{
			FS_SET_OPERATION(kInitializing);
			
			SetTargetVolume(target_dir);
		}
				
		ResetProgressIndicator();
		mProgressInfo.mTotalEntryCount = i.CountEntries();
		mProgressInfo.SetDirty();
		InitProgressIndicator();
		
		FS_ADD_POSSIBLE_ANSWER(fRetryEntry + fSkipEntry);
		
		OperationBegins();
		MoveToRecursive(i, target_dir, true);	// first run
		
	} catch (FSException e) {
		return e;
	}
	
	return B_OK;
}

status_t
FSContext::GetOriginalPath(BNode &in_node, BPath &in_path) FS_NOTHROW {

	int32 size;
	char buf[B_PATH_NAME_LENGTH + 4];
	
	if ((size = in_node.ReadAttr(kAttrOriginalPath, B_STRING_TYPE, 0, buf, B_PATH_NAME_LENGTH)) >= 0) {
		
		buf[size] = 0;					// append a possibly missing leading zero
		in_path.SetTo(buf);
		
		return B_OK;
	}
	
	return size;
}

void	// Precond:		SetTargetVolume() was called
FSContext::RestoreFromTrash(BEntry &entry) FS_THROW_FSEXCEPTION {

	status_t rc = B_OK;
	
	int32 size;
	char buf[B_PATH_NAME_LENGTH + 4];
	BNode node(&entry);
	BPath original_path;
	
	entry_ref orig_ref;
	if (gTrackerSettings.UndoEnabled())
		entry.GetRef(&orig_ref);
	
	if (GetOriginalPath(node, original_path) != B_OK) {
	
		// Iterate the parent directories to find one with the original path attribute
		BDirectory trash_dir;
		GetTrashDir(trash_dir, TargetDevice());
		
		if (trash_dir.Contains(&entry) == false) {
			TRESPASS();
			return;
		}
				
		BEntry parent;
		FS_OPERATION(entry.GetParent(&parent));
		
		for (;;) {
		
			BDirectory parent_dir;
			FS_OPERATION(parent_dir.SetTo(&parent));
			
			if (trash_dir == parent_dir)
				return;							// failed
		
			if ((size = parent_dir.ReadAttr(kAttrOriginalPath, B_STRING_TYPE, 0, buf, B_PATH_NAME_LENGTH)) > 0) {
					
				// Found the attribute, figure out where this file used to live
				buf[size] = 0;					// append a possibly missing leading zero
				
				FS_OPERATION(original_path.SetTo(buf));
				
				BPath path;
				FS_OPERATION(parent.GetPath(&path));
				int32 parent_len = strlen(path.Path());
				
				FS_OPERATION(entry.GetPath(&path));
				// Append the path from the directory with the attribute to the entry
				FS_OPERATION(original_path.Append(path.Path() + parent_len + 1));
				break;
			}
			
			{
				FS_REMOVE_POSSIBLE_ANSWER(fRetryOperation);
				FS_OPERATION(parent.GetParent(&parent));
			}
		}
	}
	
	CreateDirectory(original_path, false);
	
	bool keep_on_trying = true;
	
	while (keep_on_trying) {
		switch (entry.Rename(original_path.Path())) {
			case B_FILE_EXISTS:
				switch ((int32)Interaction(kOriginalNameAlreadyExists)) {
					case kReplace: {
						FS_SET_OPERATION(kRemoving);
						FS_SKIPABLE_OPERATION(Remove(original_path));
						continue;
					}
					case kReplaceIfNewer: {
						BEntry target_entry;
						FS_OPERATION(target_entry.SetTo(original_path.Path()));
						if (RemoveIfNewer(entry, target_entry) == true)
							continue;
						
						FS_CONTROL_THROW(kSkipEntry);
						break;
					}
					
					case kMoveTargetToTrash: {
							BEntry tent;
							FS_OPERATION(tent.SetTo(original_path.Path()));
							
							MoveToTrash(TargetDevice(), tent);
						}
						continue;
						
					case kMakeUniqueName:
					
						MakeUniqueName(original_path);	// substitute target with the new unique name
						continue;
					
					case kRetryOperation:
						continue;
						
					default: TRESPASS();
				}
			case B_OK:
				keep_on_trying = false;
				if (gTrackerSettings.UndoEnabled()) {
					entry_ref new_ref;
					entry.GetRef(&new_ref);
								
					BMessage *undo = new BMessage(kRestoreFromTrash);
					undo->AddRef("new_ref", &new_ref);
					undo->AddRef("orig_ref", &orig_ref);
					gUndoHistory.AddItemToContext(undo, this);
				}	
				break;

			default:
				if (ErrorHandler(rc) == false) {
					keep_on_trying = false;
					return;
				}
		}
	}
	
	node.RemoveAttr(kAttrOriginalPath);
}

status_t
FSContext::RestoreFromTrash(EntryIterator *i, bool async) FS_NOTHROW {

	if (async) {
		#if FS_CONFIG_MULTITHREADED
			LaunchInNewThread("FS restore thread", B_NORMAL_PRIORITY, this, &FSContext::RestoreFromTrash, i, false);
		#else
			return RestoreFromTrash(i, false);		// XXX
		#endif	
	} else {
	
		auto_ptr<EntryIterator> _i(i);

		return RestoreFromTrash(*i);		
	}
	
	return B_OK;
}

status_t
FSContext::RestoreFromTrash(EntryIterator &i) FS_NOTHROW {

	PreparingOperation();

	status_t rc;

	FS_SET_OPERATION(kRestoringFromTrash);
	
	ResetProgressIndicator();
	mProgressInfo.mTotalEntryCount = i.CountEntries();
	mProgressInfo.SetDirty();
	InitProgressIndicator();
	
	FS_ADD_POSSIBLE_ANSWER(fRetryEntry + fSkipEntry);
	
	EntryRef ref;
	bool retry_entry = false;
	
	OperationBegins();

	while (retry_entry  ||  i.GetNext(ref)) {
		retry_entry = false;
		BEntry entry;

		try {
			FS_SET_CURRENT_ENTRY(ref);
			CheckCancel();
		
			{
				FS_SET_OPERATION(kInitializing);
				FS_OPERATION(entry.SetTo(ref));

				if (TargetDevice() != ref.Device())
					SetTargetVolume(ref.Device());
			}

			RestoreFromTrash(entry);
			mProgressInfo.EntryDone();
		} catch (FSException e) {
			if (e == kSkipEntry)
				mProgressInfo.SkipEntry();
			else
				return e;
		}
	}
	
	return B_OK;
}

void	// Precond:		trash_dir may be uninitialized
		// Postcond:	entry will follow the entry to the Trash
FSContext::MoveToTrash(dev_t device, BEntry &entry, BDirectory &trash_dir) FS_THROW_FSEXCEPTION {

	status_t rc;
	
	{
		FS_SET_OPERATION(kInitializing);
		
		if (TargetDevice() != device  ||  trash_dir.InitCheck() != B_OK) {

			if (GetTrashDir(trash_dir, device) != B_OK) {
				if (Interaction(kCannotMoveToTrash) == kDeleteInstead) {
					FS_SET_OPERATION(kRemoving);
				
					FS_OPERATION_ETC(entry.Remove(), rc != B_ENTRY_NOT_FOUND, NOP);
				}
			}
		}
	}

	if (trash_dir.Contains(&entry))
		return;
		
	entry_ref orig_ref;
	if (gTrackerSettings.UndoEnabled())
		entry.GetRef(&orig_ref);
	
	node_ref dir_node_ref;
	
	bool is_dir = entry.IsDirectory();
	if (is_dir) {
		if (!ConfirmChangeIfWellKnownDirectory(&entry, "delete", false)) {
			FS_CONTROL_THROW(kSkipEntry);
			return;
		}
		
		BDirectory dir;
		FS_OPERATION(dir.SetTo(&entry));
		FS_OPERATION(dir.GetNodeRef(&dir_node_ref));
		
		BEntry trash_entry;
		FS_OPERATION(trash_dir.GetEntry(&trash_entry));
		
		if (dir == trash_dir  ||  dir.Contains(&trash_entry)) {
			FS_REMOVE_POSSIBLE_ANSWER(fRetryEntry | fRetryOperation);
			for (;;)
				Interaction(kCannotTrashHomeDesktopOrTrash);
		}
	}
	
	char name[B_FILE_NAME_LENGTH];
	entry.GetName(name);
	
	BPath old_path;
	FS_OPERATION(entry.GetPath(&old_path));
	
	do {
		MakeUniqueName(trash_dir, name);
		FS_OPERATION_ETC(entry.MoveTo(&trash_dir, name), rc != B_ENTRY_NOT_FOUND  &&  rc != B_FILE_EXISTS, NOP);
	} while (rc == B_FILE_EXISTS);
	
	// write old path to the attributes, no matter if fails
	BNode node;
	if (node.SetTo(&entry) == B_OK) {
		node.WriteAttr(kAttrOriginalPath, B_STRING_TYPE, 0, old_path.Path(), strlen(old_path.Path()) + 1);
		time_t time = real_time_clock();
		node.WriteAttr(kAttrTrashedDate, B_TIME_TYPE, 0, &time, sizeof(time));
	}
	
	if (gTrackerSettings.UndoEnabled()) {
		entry_ref new_ref;
		entry.GetRef(&new_ref);
		
		BMessage *undo = new BMessage(kDelete); // kMoveToTrash gives 22 here?!
		undo->AddRef("new_ref", &new_ref);
		undo->AddRef("orig_ref", &orig_ref);
		gUndoHistory.AddItemToContext(undo, this);
	}	

	if (is_dir)
		DirectoryTrashed(dir_node_ref);  // to close ev. open windows
}

status_t
FSContext::MoveToTrashAsync(EntryIterator *i, bool async) FS_NOTHROW {

	if (async) {
		#if FS_CONFIG_MULTITHREADED
			LaunchInNewThread("FS trash thread", B_NORMAL_PRIORITY, this, &FSContext::MoveToTrashAsync, i, false);
		#else
			return MoveToTrashAsync(i, false);		// XXX
		#endif	
	} else {
	
		auto_ptr<EntryIterator> _i(i);
		
		return MoveToTrash(*i);
	}
	
	return B_OK;
}

status_t
FSContext::MoveToTrash(EntryIterator &i) FS_NOTHROW {

	PreparingOperation();

	status_t rc;

	FS_SET_OPERATION(kMovingToTrash);
	
	ResetProgressIndicator();
	mProgressInfo.mTotalEntryCount = i.CountEntries();
	mProgressInfo.SetDirty();
	InitProgressIndicator();
	
	FS_ADD_POSSIBLE_ANSWER(fRetryEntry + fSkipEntry);
	
	EntryRef ref;
	bool retry_entry = false;
	
	SourceDirSetter sds(this);
	BEntry entry;
	BDirectory trash_dir;

	OperationBegins();
	
	while (retry_entry  ||  i.GetNext(ref)) {
		retry_entry = false;
		
		try {
			
			FS_SET_CURRENT_ENTRY(ref);
			
			CheckCancel();
		
			{
				FS_SET_OPERATION(kInitializing);
				
				FS_OPERATION(entry.SetTo(ref));
				
				FS_OPERATION(sds.SetSourceDir(ref));
				
				if (sds.IsEntryFromADifferentDir())
					GetTrashDir(trash_dir, ref.Device());
			}
			
			AboutToDeleteOrTrash(entry);

			if (trash_dir.InitCheck() == B_OK) {

				MoveToTrash(ref.Device(), entry, trash_dir);

			} else {
			
				if (Interaction(kCannotMoveToTrash) == kDeleteInstead) {
					FS_SET_OPERATION(kRemoving);
				
					FS_OPERATION_ETC(entry.Remove(), rc != B_ENTRY_NOT_FOUND, NOP);
				}
			}

			mProgressInfo.EntryDone();
			
		} catch (FSException e) {
			if (e == kSkipEntry)
				mProgressInfo.SkipEntry();
			else
				return e;
		}
	}
	
	return B_OK;
}


void	// Precond:		SetTargetVolume() was called, SourceDirSetter is active
FSContext::CheckTargetDirectory(BEntry &source_dir_entry, BDirectory &target_dir, bool check_source) FS_THROW_FSEXCEPTION {
	// check_source: this way we can spare an entry.IsDirectory() call, it's set by the caller with S_ISDIR()

	status_t rc;

	BEntry target_entry;
	FS_OPERATION(target_entry.SetTo(&target_dir, "."));

	{	// check for the thrash
		BDirectory trash;
		GetTrashDir(trash, TargetDevice());
		
		if (trash == target_dir  ||  trash.Contains(&target_entry)) {
			FS_REMOVE_POSSIBLE_ANSWER(fRetryOperation| fRetryEntry);
			for (;;)	// this is not optional, copying to the trash may lead to unexpected behaviour when trashing a file in case of an interaction
				Interaction(kTargetIsTrash);
		}
	}

	{	// check for the root
		BPath path;
		FS_OPERATION(target_entry.GetPath(&path));
		if (strcmp(path.Path(), "/") == 0) {
			for (;;)
				Interaction(kTargetIsTheRoot);
		}
	}
	
	if (check_source  &&  mSourceDir.Contains(&target_entry)) {	// quick check that fails in most of the time
	
		BDirectory source_dir;
		FS_OPERATION(source_dir.SetTo(&source_dir_entry));
		
		if (source_dir.Contains(&target_entry)  ||  source_dir == target_dir) {
			for (;;)	// no way out, only with exception. it could lead to an endless loop copying a dir into itself
				Interaction(kTargetIsSelfOrSubfolder);
		}
	}
}

void
FSContext::CopyAdditionals(BNode &source, BNode &target) FS_THROW_FSEXCEPTION {

#if defined(DOIT)
  #error Macro redefined
#else
  #define DOIT(x)														\
	try {																\
		x;																\
	} catch (FSException e) {											\
		FS_FILTER_EXCEPTION(kSkipOperation, NOP)						\
	}
#endif

	status_t rc;
	struct stat statbuf;
	
	FS_ADD_POSSIBLE_ANSWER(fSkipOperation);

	FS_OPERATION(source.GetStat(&statbuf));

	if (ShouldCopy(fAttributes))
		CopyAttributes(source, target);

#if DEBUG	// until beos bug (?) fixed
	if (ShouldCopy(fCreationTime)) {
		DOIT(	FS_SET_OPERATION(kCopyingCreationTime);
				FS_OPERATION(target.SetCreationTime(statbuf.st_ctime)));
	}			
#else
	if (ShouldCopy(fCreationTime)) {
		time_t time;
		try {
			FS_SET_OPERATION(kCopyingCreationTime);
			FS_OPERATION(source.GetCreationTime(&time));
			FS_OPERATION(target.SetCreationTime(time));
		} catch (FSException e) {
			FS_FILTER_EXCEPTION(kSkipOperation, NOP);
		}
	}			
#endif

#if DEBUG	// until beos bug (?) fixed
	if (ShouldCopy(fModificationTime)) {
		DOIT(	FS_SET_OPERATION(kCopyingModificationTime);
				FS_OPERATION(target.SetModificationTime(statbuf.st_mtime)));
	}
#else
	if (ShouldCopy(fModificationTime)) {
		time_t time;
		try {
			FS_SET_OPERATION(kCopyingModificationTime);
			FS_OPERATION(source.GetModificationTime(&time));
			FS_OPERATION(target.SetModificationTime(time));
		} catch (FSException e) {
			FS_FILTER_EXCEPTION(kSkipOperation, NOP);
		}
	}			
#endif
	if (ShouldCopy(fOwner)) {
		DOIT(	FS_SET_OPERATION(kCopyingOwner);
				FS_OPERATION(target.SetOwner(statbuf.st_uid)));
	}	
	if (ShouldCopy(fGroup)) {
		DOIT(	FS_SET_OPERATION(kCopyingGroup);
				FS_OPERATION(target.SetGroup(statbuf.st_gid)));
	}
	if (ShouldCopy(fPermissions)) {
		DOIT(	FS_SET_OPERATION(kCopyingPermissions);
				FS_OPERATION(target.SetPermissions(statbuf.st_mode)));
	}

	#if DEBUG
	{
		struct stat statbuf_t;
		time_t time1, time2;
		FS_OPERATION(target.GetStat(&statbuf_t));
		
		if (ShouldCopy(fCreationTime)) { //  &&  statbuf.st_ctime != statbuf_t.st_ctime
			FS_OPERATION(source.GetCreationTime(&time1));
			FS_OPERATION(target.GetCreationTime(&time2));
			printf("%s - Creation time copy (file: %s,\told: %lx, %lx; copied: %lx, %lx) !!!\n",
				(statbuf.st_ctime == statbuf_t.st_ctime && time1 == time2) ? "OK" : "FAILED",
				CurrentEntryName(), statbuf.st_ctime, time1, statbuf_t.st_ctime, time2);
		}
		if (ShouldCopy(fModificationTime)) { //  &&  statbuf.st_ctime != statbuf_t.st_ctime
			FS_OPERATION(source.GetModificationTime(&time1));
			FS_OPERATION(target.GetModificationTime(&time2));
			printf("%s - Modification time copy (file: %s,\told: %lx, %lx; copied: %lx, %lx) !!!\n",
				(statbuf.st_mtime == statbuf_t.st_mtime && time1 == time2) ? "OK" : "FAILED",
				CurrentEntryName(), statbuf.st_ctime, time1, statbuf_t.st_ctime, time2);
		}
	}
	#endif

#undef DOIT
}



void
FSContext::CopyAttributes(BNode &source, BNode &target) FS_THROW_FSEXCEPTION {

	if (mTargetVolume.KnowsAttr() == false)
		return;

	status_t rc;

	source.RewindAttrs();
	char name[B_ATTR_NAME_LENGTH];
	
	{
		FS_SET_OPERATION(kCopyingAttributes);
		
		FS_ADD_POSSIBLE_ANSWER(fSkipOperation);
		
		while (source.GetNextAttrName(name) == B_OK) {
			if (mSkipTrackerPoseInfoAttibute  &&  strcmp(name, kAttrPoseInfo) == 0)
				continue;	// force auto placement when duplicating

			try  {
			
				attr_info info;
				FS_OPERATION(source.GetAttrInfo(name, &info));
				
				SuggestBufferSize(info.size);
				
				// XXX beos does not yet honor the offset parameter for attr read/write, so the loop below
				// is good only if the buffer is big enough to copy attributes in one chunk. remove below lines when ok.
				if (BufferSize() < info.size) {
					TRESPASS();
					FS_ERROR_THROW(	"Attribute is too big to fit in buffer! And R5 BeOS does not honor the offset parameter"
										"for attribute read/writes.", B_NO_MEMORY);
				}
				
				size_t size, written = 0;
				while (written < info.size) {
				
					{
						FS_SET_OPERATION(kReadingAttribute);
						FS_OPERATION(size = source.ReadAttr(name, B_ANY_TYPE, written, Buffer(), BufferSize()));
					}

					{
						FS_SET_OPERATION(kWritingAttribute);
						FS_OPERATION(target.WriteAttr(name, info.type, written, Buffer(), size));
					}

					written += size;
				}
							
			} catch (FSException e) {
	
				FS_FILTER_EXCEPTION(kSkipOperation, return);
			}
		}
	}
}

status_t
FSContext::CreateNewFolder(const node_ref &in_dir_node, const char *in_name, entry_ref *in_new_entry,
								node_ref *in_new_node) FS_NOTHROW {

	status_t rc;
	
	BDirectory dir(&in_dir_node), new_dir;
	char name_buf[B_FILE_NAME_LENGTH];
	strcpy(name_buf, (in_name) ? in_name : LOCALE("New Folder"));
	
	if ((rc = dir.InitCheck()) == B_OK) {
	
		// if name was given don't try to find an original name
		while ((rc = dir.CreateDirectory(name_buf, &new_dir)) == B_FILE_EXISTS  &&  in_name == NULL)
			MakeUniqueName(dir, name_buf);
		
		if (rc != B_OK)
			return rc;
			
		if (in_new_node != NULL)
			FS_STATIC_OPERATION(new_dir.GetNodeRef(in_new_node));
	
		if (in_new_entry != NULL) {
			BEntry entry;
			FS_STATIC_OPERATION(new_dir.GetEntry(&entry));
			FS_STATIC_OPERATION(entry.GetRef(in_new_entry));
		}
		
		if (gTrackerSettings.UndoEnabled()) {
			entry_ref new_ref;
			BNode node;
			if (in_new_entry == NULL) {
				BEntry entry;
				new_dir.GetEntry(&entry);
				entry.GetRef(&new_ref);
			}
			BEntry entry;
			entry_ref orig_ref;
			dir.GetEntry(&entry);
			entry.GetRef(&orig_ref);
			
			BMessage *undo = new BMessage(kNewFolder);
			undo->AddRef("new_ref", (in_new_entry == NULL ? &new_ref : in_new_entry));
			undo->AddRef("orig_ref", &orig_ref);
			if (in_name != NULL)
				undo->AddString("name", in_name);
			gUndoHistory.AddItem(undo);
		}
	}
	return rc;
}

void
FSContext::MakeUniqueName(BPath &path, const char *suffix) FS_THROW_FSEXCEPTION {

	status_t rc;
	BString str(path.Path());
	
	int32 i = str.FindLast('/');
	if (i == B_ERROR)
		return;
	
	str.Truncate(i);
	
	BDirectory dir;
	FS_OPERATION(dir.SetTo(str.String()));
	
	char name_buf[B_FILE_NAME_LENGTH];
	strcpy(name_buf, path.Leaf());
	
	MakeUniqueName(dir, name_buf, suffix);
	
	FS_OPERATION(path.SetTo(str.String(), name_buf));
}

void
FSContext::MakeUniqueName(BDirectory &dir, char *name, const char *suffix) FS_NOTHROW {

	if ( ! dir.Contains(name))
		return;

	char	root[B_FILE_NAME_LENGTH];
	char	base[B_FILE_NAME_LENGTH];
	char	tmp[B_FILE_NAME_LENGTH + 16];
	int32	fnum;

	// Determine if we're copying a 'copy'. This algorithm isn't perfect.
	// If you're copying a file whose REAL name ends with 'copy' then
	// this method will return "<filename> 1", not "<filename> copy"

	// However, it will correctly handle file that contain 'copy' 
	// elsewhere in their name.

	bool has_suffix = false;
	uint32 len = strlen(name);
	uint32 suffix_len = (suffix) ? strlen(suffix) : 0;
	
	char *p = name + len - 1;			// get pointer to end of name

	while (p > name  &&  isdigit(*p))	// eat up optional numbers
		--p;
	
	while (p > name && isspace(*p))		// eat up optional spaces
		--p;

	if (p >= name) {					// now look for the suffix
		
		// p points to the last char of the word. For example, 'y' in 'copy'
		
		++p;
		if (suffix) {
			if (p - suffix_len > name  &&  strncmp(p - suffix_len, suffix, suffix_len) == 0) {
				*p = '\0';				// we found the suffix in the right place, so truncate after it
				has_suffix = true;
	
				// save the 'root' name of the file, for possible later use.
				// that is copy everything but trailing suffix. Need to
				// NULL terminate after copy
	
				strncpy(root, name, (uint32)((p - name) - suffix_len));
				root[(p - name) - suffix_len] = '\0';
			
			} else {
			
				strcpy(root, name);		// simply copy the name, it does not seem to fit the template
			}
			
		} else if (isspace(*p)) {		// if there's not any space then it's part of the original file name

			strncpy(root, name, (uint32)((p - name)));
			root[(p - name)] = '\0';
		} else {
			
			strcpy(root, name);
		}
	}

	if (has_suffix == false) {
		//	The name can't be longer than B_FILE_NAME_LENGTH.
		//	The algoritm adds suffix + " XX" to the name.
		//	B_FILE_NAME_LENGTH already accounts for NULL termination so we
		//	don't need to save an extra char at the end.

		if (strlen(root) > B_FILE_NAME_LENGTH - suffix_len - 4)
			root[B_FILE_NAME_LENGTH - suffix_len - 4] = '\0';
	}

	strcpy(base, root);
	if (suffix)
		strcat(base, suffix);

	// if name already exists then add a number
	fnum = 1;
	strcpy(tmp, base);
	while (dir.Contains(tmp)) {
		sprintf(tmp, "%s %ld", base, ++fnum);

		if (strlen(tmp) > (B_FILE_NAME_LENGTH - 1)) {
//			 The name has grown too long. Maybe we just went from
//			 "<filename> copy 9" to "<filename> copy 10" and that extra
//			 character was too much. The solution is to further
//			 truncate the 'root' name and continue.
			root[strlen(root) - 1] = '\0';
			strcpy(base, root);
			if (suffix)
				strcat(base, suffix);
				
			sprintf(tmp, "%s %ld", base, fnum);
		}
	}
	
	ASSERT((strlen(tmp) <= (B_FILE_NAME_LENGTH - 1)));
	strcpy(name, tmp);
}

// private recursive delete
void
FSContext::RemoveRecursive(EntryIterator &i, bool progress_enabled, bool first_run) FS_THROW_FSEXCEPTION {

	status_t rc;

	EntryRef ref;
	bool retry_entry = false;

	while (retry_entry  ||  i.GetNext(ref)) {
		retry_entry = false;
		BEntry entry;
		
		try {
			FS_SET_CURRENT_ENTRY(ref);
			CheckCancel();

			{
				FS_SET_OPERATION(kInitializing);
				FS_OPERATION(entry.SetTo(ref));
			}
			
			if (first_run)
				AboutToDeleteOrTrash(entry);
			
			if (!ConfirmChangeIfWellKnownDirectory(&entry, "delete", false)) {
				FS_CONTROL_THROW(kSkipEntry);
			}
			
			while ((rc = entry.Remove()) < 0) {
				if (rc == B_DIRECTORY_NOT_EMPTY) {
					DirEntryIterator dei;
					i.RegisterNested(dei);
					
					FS_OPERATION(dei.SetTo(entry));
					
					RemoveRecursive(dei, progress_enabled, false);		// empty directory, not the first run
					continue;									// and try again
				}
				if (rc == B_ENTRY_NOT_FOUND  ||  ErrorHandler(rc) == false)
					break;
			}
			
			if (progress_enabled)
				mProgressInfo.EntryDone();
			
		} catch (FSException e) {
		
			FS_FILTER_EXCEPTION_2(	kSkipEntry,		if (progress_enabled) mProgressInfo.SkipEntry(),
									kRetryEntry,	retry_entry = true;
													if (progress_enabled)
														mProgressInfo.SkipEntry();
								);
		}
	}
}

status_t
FSContext::Remove(EntryIterator *i, bool askbefore, bool async) FS_NOTHROW {

	if (async) {
		#if FS_CONFIG_MULTITHREADED
			LaunchInNewThread("FS delete thread", B_NORMAL_PRIORITY, this, &FSContext::Remove, i, askbefore, false);
		#else
			return Remove(i, askbefore, false);
		#endif	
	} else {
		
		auto_ptr<EntryIterator> _i(i);
		
		return Remove(*i, askbefore);
	}
	
	return B_OK;
}

status_t
FSContext::Remove(EntryIterator &i, bool in_ask_before) FS_NOTHROW {

	PreparingOperation();
	FS_SET_OPERATION(kRemoving);

	try {
		if (in_ask_before) {
			FS_REMOVE_POSSIBLE_ANSWER(fRetryOperation | fRetryEntry);

			switch (Interaction(kAboutToDelete)) {
				case kGoOnAndDelete:
					break;
				case kMoveToTrash:
					MoveToTrash(i);
					return B_OK;
				default:
					TRESPASS();
					return B_OK;
			}
		}

		ResetProgressIndicator();
		InitProgressIndicator();

		{
			FS_SET_OPERATION(kInitializing);
			AccumulateItemsAndSize(i);
		}

		FS_ADD_POSSIBLE_ANSWER(fRetryEntry + fSkipEntry);
		OperationBegins();
		RemoveRecursive(i, true, true);	// progress on, first run
		
	} catch (FSException e) {
		return e;
	}
	return B_OK;
}

void		// smart delete: will set the operation, and it's interactive; no progress indication
FSContext::Remove(BPath &path) FS_THROW_FSEXCEPTION {

	status_t rc;
	BEntry entry;
	FS_OPERATION_ETC(entry.SetTo(path.Path()), rc != B_ENTRY_NOT_FOUND, NOP);
	if (rc != B_OK)
		return;
	
	Remove(entry);
}

void
FSContext::Remove(BDirectory &dir, const char *file) FS_THROW_FSEXCEPTION {

	status_t rc;
	
	FS_OPERATION(dir.InitCheck());
	
	BEntry entry;
	FS_OPERATION_ETC(entry.SetTo(&dir, file), rc != B_ENTRY_NOT_FOUND, NOP);
	
	if (rc != B_OK  ||  entry.Exists() == false)
		return;
	
	Remove(entry);
}


// central private delete: will set the operation, and it's interactive; no progress indication
void
FSContext::Remove(BEntry &entry, operation in_op) FS_THROW_FSEXCEPTION {

	status_t rc;
	
	FS_SET_OPERATION(in_op);
	
	SingleEntryIterator i;
	FS_OPERATION(i.SetTo(entry));
	
	FS_ADD_POSSIBLE_ANSWER(fRetryEntry + fSkipEntry);
	
	RemoveRecursive(i, false, true);	// no progress indication, only a sub-remove; first run
}

status_t
FSContext::CreateLinkTo(EntryIterator *i, BDirectory &target_dir, bool relative, bool async) FS_NOTHROW {
	
	if (async) {
		#if FS_CONFIG_MULTITHREADED
			LaunchInNewThread("FS create link thread", B_NORMAL_PRIORITY, this, &FSContext::CreateLinkTo,
								i, target_dir, relative, false);
		#else
			return CreateLinkTo(i, target_dir, relative, false);
		#endif
	} else {

		auto_ptr<EntryIterator> _i(i);
	
		return CreateLinkTo(*i, target_dir, relative);
	}
	
	return B_OK;
}

status_t
FSContext::CreateLinkTo(EntryIterator &i, BDirectory &target_dir, bool relative) FS_NOTHROW {

	PreparingOperation();

	status_t rc;
	FS_SET_OPERATION(kCreatingLink);
	
	try {
	
		{
			FS_SET_OPERATION(kInitializing);
			
			FS_OPERATION(target_dir.InitCheck());
			FS_OPERATION(target_dir.GetNodeRef(&mRootTargetDirNodeRef));
		}
		
		ResetProgressIndicator();
		mProgressInfo.mTotalEntryCount = i.CountEntries();
		mProgressInfo.SetDirty();
		InitProgressIndicator();

		char target_name[B_FILE_NAME_LENGTH];

		SourceDirSetter sds(this);
		
		EntryRef ref;
		BEntry entry;
		bool retry_entry = false;
	
		FS_ADD_POSSIBLE_ANSWER(fSkipEntry + fRetryEntry);

		OperationBegins();

		while (retry_entry  ||  i.GetNext(ref)) {
			retry_entry = false;
			bool count_this_entry = true;
	
			try {
		
				FS_SET_CURRENT_ENTRY(ref);
		
				CheckCancel();
				
				strcpy(target_name, ref.Name());
	
				{
					FS_SET_OPERATION(kInitializing);
	
					FS_OPERATION(entry.SetTo(ref));
					FS_OPERATION(sds.SetSourceDir(ref));
					
					// XXX what is if the entry is a link itself? should we ask the user?
				}
				
				CheckTargetDirectory(entry, target_dir);	// may not create a link into itself (directory), and in the trash
		
				node_ref target_nref;
				FS_OPERATION(target_dir.GetNodeRef(&target_nref));
				if (target_nref == mSourceDirNodeRef)
					MakeUniqueName(target_dir, target_name, LOCALE(" link"));	// make a unique name if link to the same dir
				
				if (CreateLink(entry, target_dir, target_name, relative) == true) {
	
					// at last we actually created a link, set it's MIME type
					BNode target_node;
					FS_OPERATION(target_node.SetTo(&target_dir, target_name));
						
					BNodeInfo info(&target_node);
					info.SetType(B_LINK_MIMETYPE);				// it's not a big progblem that the mime type could not be set...
					
					node_ref target_dir_ref;	// XXX rework... look at the comment at PoseInfo in Utility.h
					FS_OPERATION(target_dir.GetNodeRef(&target_dir_ref));
				
					NextEntryCreated(target_dir_ref, target_node);	// CopyEntry below calls NextEntryCreated

				} else {
					// link was not created, we should copy instead (we can not get here in any other case)
					FS_SET_OPERATION(kCopying);
			
					mProgressInfo.EnableTotalSizeProgress();

					EntryRef tmp_ref;							// set ref to the link target, if it's a link
					entry_ref eref;
					FS_OPERATION(entry.GetRef(&eref));
					tmp_ref.SetTo(eref);
					FS_OPERATION(entry.SetTo(tmp_ref));			// required for the Accumulate() function below
					
					FS_SET_CURRENT_ENTRY(tmp_ref);				// set it again, it's the links target now
			
					{											// initialize, set up dirs
						FS_SET_OPERATION(kInitializing);
						
						FS_OPERATION(sds.SetSourceDir(tmp_ref));
			
						AccumulateItemsAndSize(entry);			// calculate size of the source
						mProgressInfo.SkipEntry();				// the entry we are linking
						count_this_entry = false;				// CopyEntry will do it
					}
					
					CopyEntry(i, tmp_ref, target_dir);
				}
				
				// XXX total count may be one more, because accumulate... calculates the entry again, and copyentry sets it to be done
				// not a big problem imho...
				if (count_this_entry)								// do not count the entry twice if we copied it instead of linking
					mProgressInfo.EntryDone();
					
			} catch (FSException e) {
			
				FS_FILTER_EXCEPTION_2(	kSkipEntry,		mProgressInfo.SkipEntry(),
										kRetryEntry,	retry_entry = true;
														mProgressInfo.SkipEntry()
									);
			}
		}

	} catch (FSException e) {
		return e;		// kSkipEntry = 0 = B_OK
	}
	
	return B_OK;
}

// XXX should it set entry type to link?
// source_entry is the actual target of the link and target_name will be the link name.
bool	// Precond:		target_name is big enough to hold B_FILE_NAME_LENGTH
FSContext::CreateLink(BEntry &source_entry, BDirectory &target_dir, char *target_name, bool relative) FS_THROW_FSEXCEPTION {

	status_t rc;
	
	entry_ref source_eref;
	node_ref target_nref;
	BPath source_path;
	
	BString source;
	BSymLink new_link;
	
	// save redo data
	entry_ref orig_ref;
	if (gTrackerSettings.UndoEnabled())
		source_entry.GetRef(&orig_ref);
	
	FS_OPERATION(target_dir.InitCheck());

	FS_SET_OPERATION(kCreatingLink);

	{
		FS_SET_OPERATION(kInitializing);
		
		FS_OPERATION(source_entry.GetRef(&source_eref));
		FS_OPERATION(target_dir.GetNodeRef(&target_nref));
	
		if (relative) {
			if (source_eref.device != target_nref.device) {
				switch ((int32)Interaction(kCannotRelativeLink)) {
					case kCreateAbsolute:
						relative = false;
						break;
					case kCopyEachOneInstead:
						mCopyLinksInstead = true;
						break;
						
					default: TRESPASS();
				}
			}
		}
	
		FS_OPERATION(source_entry.GetPath(&source_path));
	}

	if (mCopyLinksInstead == false) {
		WDChanger wdc;
		
		if (relative) {
			BPath path;
			BEntry entry;
			FS_OPERATION(target_dir.GetEntry(&entry));
			FS_OPERATION(entry.GetPath(&path));
			
			FS_OPERATION(wdc.Change(path));				// change working dir to target dir
			
			BString destString(path.Path());
			destString.Append("/");
			
			BString srcString(source_path.Path());
			srcString.RemoveLast(source_path.Leaf());
			
			const char *src = srcString.String();
			const char *dest = destString.String();
			
			const char *src_slash = src;
			const char *dest_slash = dest;
			
			while (*src  &&  *dest  &&  *src == *dest) {// find index while paths are the same
				if (*src == '/') {
					src_slash = src + 1;				// save positions to be able to revert
					dest_slash = dest + 1;
				}
				++src;
				++dest;
			}
			
			if (*src != *dest) {						// step back to the last level where paths were the same
				src = src_slash;
				dest = dest_slash;
			}

			if (*dest == 0  &&  *src != 0) {			// if source is deeper in the same tree as the target
				source.Append(src);
			} else if (*dest != 0) {					// opposite
				while (*dest) {
					if (*dest == '/')
						source.Prepend("../");
					++dest;
				}
				if (*src  !=  0)
					source.Append(src);
			}											// else source and target are in the same dir
			
			source.Append(source_path.Leaf());
		} else {	// absolute link
			source = source_path.Path();
		}
		
		for (;;) {
			rc = target_dir.CreateSymLink(target_name, source.String(), &new_link);
			if (rc >= 0) {
				if (gTrackerSettings.UndoEnabled()) {
					gUndoHistory.SetTargetForContext(target_dir, this);
					BEntry entry(&target_dir, target_name);
					entry_ref new_ref;
					entry.GetRef(&new_ref);
					
					BMessage *undo = new BMessage((relative ? kCreateRelativeLink : kCreateLink));
					undo->AddRef("new_ref", &new_ref);
					undo->AddRef("orig_ref", &orig_ref);
					gUndoHistory.AddItemToContext(undo, this);
				}
				break;
			}
				
			if (rc == B_FILE_EXISTS) {
			
				BEntry target_entry;
				FS_OPERATION(target_entry.SetTo(&target_dir, target_name));
				
				FS_BACKUP_VARIABLE_AND_SET(mTargetEntry, &target_entry);

				interaction iact;
				if (mOperationStack.RootOperation() == kCreatingLink)
					iact = kTargetNameOccupied;
				else
					iact = kTargetAlreadyExists;
					
				switch ((int32)Interaction(iact)) {
					case kReplace: {
						FS_SET_OPERATION(kRemoving);
						FS_SKIPABLE_OPERATION(Remove(target_dir, target_name));
						continue;
					}
					case kMoveTargetToTrash: {
							BEntry tent;
							FS_OPERATION(tent.SetTo(&target_dir, target_name));
							
							MoveToTrash(target_nref.device, tent);
						}
						continue;

					case kReplaceIfNewer:
						if (RemoveIfNewer(source_entry, target_dir, target_name) == true)
							continue;
							
						FS_CONTROL_THROW(kSkipEntry);
						break;

					case kMakeUniqueName:
						MakeUniqueName(target_dir, target_name);	// substitute target with the new unique name
						continue;
						
					case kRetryOperation:
						continue;
					
					default: TRESPASS();
				}
			} else if (rc == B_BAD_VALUE) {
			
				if (Interaction(kLinksNotSupported) == kCopyEachOneInstead) {
					mCopyLinksInstead = true;
					break;
				}
			} else {
			
				if (ErrorHandler(rc) == false)
					break;
			}
		}
	}
	
	// If mCopyLinksInstead == true then we don't get in this function at all, and if it's true here
	// at the and then we just switched to copy mode, so we didn't created any link, so return false
	return !mCopyLinksInstead;
}


status_t
FSContext::Duplicate(EntryIterator *i, bool async) FS_NOTHROW {

	if (async) {
		#if FS_CONFIG_MULTITHREADED
			LaunchInNewThread("FS duplicate thread", B_NORMAL_PRIORITY, this, &FSContext::Duplicate, i, false);
		#else
			return Duplicate(i, false);
		#endif	
	} else {
	
		auto_ptr<EntryIterator> _i(i);
		
		return Duplicate(*i);
	
	}
	return B_OK;
}

status_t
FSContext::Duplicate(EntryIterator &i) FS_NOTHROW {

	PreparingOperation();

	status_t rc;

	FS_SET_OPERATION(kDuplicating);
	
	ONLY_WITH_TRACKER(
		mSkipTrackerPoseInfoAttibute = true;		// force auto placement for icons
	);
	
	try {
		ResetProgressIndicator();
		mProgressInfo.EnableTotalSizeProgress();	// indicate that we will be dealing with sizes, not only entries
		InitProgressIndicator();

		{
			FS_SET_OPERATION(kInitializing);
			AccumulateItemsAndSize(i);
		}
		
		char target_name[B_FILE_NAME_LENGTH];

		SourceDirSetter sds(this);
		
		EntryRef ref;
		BDirectory target_dir;
		BEntry entry;
		bool retry_entry = false;
	
		FS_ADD_POSSIBLE_ANSWER(fSkipEntry + fRetryEntry);
	
		OperationBegins();

		while (retry_entry  ||  i.GetNext(ref)) {
			retry_entry = false;
	
			try {
				FS_SET_CURRENT_ENTRY(ref);
		
				CheckCancel();
	
				strcpy(target_name, ref.Name());

				{
					FS_SET_OPERATION(kInitializing);
	
					FS_OPERATION(sds.SetSourceDir(ref));
					sds.SetRootSourceDir(true);
				}
				
				if (sds.IsEntryFromADifferentDir()) {
				
					FS_OPERATION(entry.SetTo(ref));
					{
						FS_REMOVE_POSSIBLE_ANSWER(fRetryOperation);
						FS_OPERATION(entry.GetParent(&entry));
					}
					FS_OPERATION(target_dir.SetTo(&entry));
					FS_OPERATION(target_dir.GetNodeRef(&mRootTargetDirNodeRef));
					SetTargetVolume(target_dir);
					
					CheckTargetDirectory(entry, target_dir);	// may not duplicate in the trash...
				}
				
				MakeUniqueName(target_dir, target_name, LOCALE(" copy"));
				CopyEntry(i, ref, target_dir, target_name);

			} catch (FSException e) {
				FS_FILTER_EXCEPTION_2(	kSkipEntry,		mProgressInfo.SkipEntry(); mProgressInfo.DisableTotals(),
										kRetryEntry,	retry_entry = true;
														mProgressInfo.SkipEntry();
														mProgressInfo.DisableTotals();
									);
			}
		}
	} catch (FSException e) {
		return e;
	}

	return B_OK;
}


// returns true if the given device is on the same hardware as the previously set target
bool	// Precond:		SetTargetVolume() was called and it set mSourceDevice
FSContext::IsTargetOnSameDevice() FS_NOTHROW {
	
	if (TargetDevice() == mSourceDevice)
		return true;
	
	fs_info info;
	BString dev1, dev2;
	
	if (fs_stat_dev(TargetDevice(), &info) == B_OK) {
	
		dev1 = info.device_name;
		
		if (fs_stat_dev(mSourceDevice, &info) == B_OK) {
				
			dev2 = info.device_name;
			
			int32 i;
			if ((i = dev1.FindLast('/')) != B_ERROR)
				dev1.Truncate(i);
			if ((i = dev2.FindLast('/')) != B_ERROR)
				dev2.Truncate(i);
			
			if (dev1 == dev2)
				return true;
		}
	}
	
	return false;
}


void
FSContext::SetTargetVolume(dev_t device) FS_THROW_FSEXCEPTION {

	status_t rc;
	FS_OPERATION(mTargetVolume.SetTo(device));
}

void
FSContext::SetTargetVolume(BDirectory &target_dir) FS_THROW_FSEXCEPTION {

	status_t rc;
	
	node_ref nref;
	FS_OPERATION(target_dir.GetNodeRef(&nref));
	FS_OPERATION(mTargetVolume.SetTo(nref.device));
}

void
FSContext::CreateDirectory(BPath &path, bool is_dir) FS_THROW_FSEXCEPTION {

	FS_SET_OPERATION(kCreatingDirectory);		// recursion-safe

	status_t rc;
	
	BEntry entry;
	for (;;) {
		entry.SetTo(path.Path());
		if (entry.InitCheck() == B_ENTRY_NOT_FOUND) {
			BPath parent_path;
			FS_OPERATION(path.GetParent(&parent_path));
	
			CreateDirectory(parent_path, true);
			break;
		} else if (entry.InitCheck() == B_OK) {
			break;
		} else {
			if (ErrorHandler(entry.InitCheck()) == false)
				return;
		}
	}
	
	if (is_dir) {
		FS_OPERATION(entry.SetTo(path.Path()));
		char name[B_FILE_NAME_LENGTH];
		BDirectory parent;
	
		FS_OPERATION(entry.GetParent(&parent));
		FS_OPERATION(entry.GetName(name));
		
		CreateDirectory(&parent, name, NULL);
	}
}


status_t
FSContext::CreateDirectory(BDirectory *target_dir, char *target_name, BDirectory *new_dir)
{
	status_t rc;
	FS_SET_OPERATION(kCreatingDirectory);
	FS_OPERATION(target_dir->CreateDirectory(target_name, new_dir));

	if (gTrackerSettings.UndoEnabled()) {
		BEntry entry(target_dir, target_name);
		entry_ref new_ref;
		entry.GetRef(&new_ref);
		entry_ref orig_ref;
		target_dir->GetEntry(&entry);
		entry.GetRef(&orig_ref);
		
		BMessage *undo = new BMessage(kNewFolder);
		undo->AddRef("new_ref", &new_ref);
		undo->AddRef("orig_ref", &orig_ref);
		undo->AddString("name", target_name);
		gUndoHistory.AddItemToContext(undo, this);
	}

	return rc;
}


EntryList::STLEntryIterator::STLEntryIterator(const STLEntryIterator &other) FS_NOTHROW
						: EntryIterator(), mContainer(other.mContainer), mPos(other.mPos) {
}

EntryList::STLEntryIterator::STLEntryIterator(cont_type &incontainer) FS_NOTHROW : mContainer(incontainer), mPos(mContainer.begin()) {
}

int32
EntryList::STLEntryIterator::CountEntries() FS_NOTHROW {
	return mContainer.size();
}

bool
EntryList::STLEntryIterator::GetNext(EntryRef &entry) FS_NOTHROW {
	while (mPos < mContainer.end()  &&  IsFilteredOut(*mPos))
		++mPos;
			
	if (mPos < mContainer.end()) {
		entry.SetTo(*mPos);
		++mPos;
		return true;
	}
	return false;
}

EntryIterator *
EntryList::STLEntryIterator::Clone() FS_NOTHROW {
	return new STLEntryIterator(*this);
}
	
void
EntryList::STLEntryIterator::Rewind() FS_NOTHROW {
	mPos = mContainer.begin();
}

EntryList::EntryList(BObjectList<entry_ref> *inlist) FS_NOTHROW {
	back_insert_iterator<inherited> bii(*this);

	int32 count = inlist->CountItems();
	if (count) {
		for (int32 i = 0; i < count; i++) {
			entry_ref *ref = inlist->ItemAt(i);
			*bii = *ref;
			++bii;
		 }
	}
}

EntryList::EntryList(const BMessage &in_msg) FS_NOTHROW {
	back_insert_iterator<inherited> bii(*this);

	for (int32 i = 0;  ; ++i) {
		entry_ref ref;
		if (in_msg.FindRef("refs", i, &ref) != B_OK)
			break;
		*bii = ref;
		++bii;
	}
}

status_t
FSContext::GetSpecialDir(BDirectory &in_dir, dev_t in_device, directory_which in_which, node_ref_list_t &in_list) FS_NOTHROW {
	status_t rc;
	node_ref ref;
	if ((rc = GetSpecialDir(ref, in_device, in_which, in_list)) == B_OK)
		rc = in_dir.SetTo(&ref);

	if (rc != B_OK)
		in_dir.Unset();
		
	return rc;
}

status_t
FSContext::GetSpecialDir(node_ref &in_dir_ref, dev_t in_device, directory_which in_which, node_ref_list_t &in_list) FS_NOTHROW {
	BAutolock l(sLocker);
	
	status_t rc;
	node_ref_list_t::iterator		pos = in_list.begin(),
									end = in_list.end();
	while (pos < end) {
		
		if ((*pos).device == in_device) {
			in_dir_ref = *pos;
			return B_OK;						// we've got it, return
		}
		++pos;
	}
	
									// we've been asked about a dir on such a device on which we don't know what the special dir is
	char buf[B_PATH_NAME_LENGTH];	// so try to record it (probably it was mounted after we started up)
	
	if ((rc = find_directory(in_which, in_device, true, buf, B_PATH_NAME_LENGTH)) == B_OK) {
		BDirectory dir(buf);
		if ((rc = dir.InitCheck()) == B_OK) {
			
			ONLY_WITH_TRACKER(
				if (in_which == B_TRASH_DIRECTORY) {
					attr_info a_info;
					if (dir.GetAttrInfo(kAttrPoseInfo, &a_info) != B_OK) {
				
						struct stat sbuf;
						dir.GetStat(&sbuf);
				
						// move trash to bottom left of main screen initially
						BScreen screen(B_MAIN_SCREEN_ID);
						BRect scrn_frame = screen.Frame();
				
						PoseInfo poseInfo;
						poseInfo.fInvisible = false;
						poseInfo.fInitedDirectory = sbuf.st_ino;
						poseInfo.fLocation = BPoint(scrn_frame.left + 20, scrn_frame.bottom - 60);
						dir.WriteAttr(kAttrPoseInfo, B_RAW_TYPE, 0, &poseInfo,
							sizeof(PoseInfo));
					}
				}
			);
		
			if ((rc = dir.GetNodeRef(&in_dir_ref)) == B_OK)
				in_list.push_back(in_dir_ref);		// store this unknown dir for later reuse
		}
	}

	return rc;
}

bool
FSContext::IsSpecialDir(const node_ref &in_ref, directory_which in_which, node_ref_list_t &in_list) FS_NOTHROW {

	node_ref special_ref;

	return GetSpecialDir(special_ref, in_ref.device, in_which, in_list) == B_OK  &&  special_ref == in_ref;
}



status_t
FSContext::EmptyTrash(bool async) FS_NOTHROW {

	if (async) {
		#if FS_CONFIG_MULTITHREADED
			LaunchInNewThread("FS empty trash thread", B_NORMAL_PRIORITY, this, &FSContext::EmptyTrash, false);
		#else
			return EmptyTrash(false);
		#endif	
	} else {

		PreparingOperation();

		FS_SET_OPERATION(kEmptyingTrash);

		if (atomic_or(&sEmptyTrashRunning, 1) != 0)			// only one at a time
			return B_OK;
		
		vector<EntryIterator *>	trash_iterators;
	
		try {
		
			status_t rc;
			
			ResetProgressIndicator();
			InitProgressIndicator();
		
			BVolumeRoster roster;
			BVolume vol;
			BPath path;
			while (roster.GetNextVolume(&vol) == B_OK) {
				
				if (vol.IsReadOnly()  ||  vol.IsPersistent() == false)
					continue;
			
				if (find_directory(B_TRASH_DIRECTORY, &path, true, &vol) == B_OK) {
					BDirectory dir(path.Path());
					if (dir.InitCheck() != B_OK)
						continue;
					
					auto_ptr<DirEntryIterator> dei(new DirEntryIterator());
					FS_OPERATION(dei -> SetTo(dir));
					
					{
						FS_SET_OPERATION(kInitializing);
						
						AccumulateItemsAndSize(*dei);
					}					
					
					trash_iterators.push_back(dei.release());
				}
			}
			
			vector<EntryIterator *>::iterator	pos = trash_iterators.begin(),
												end = trash_iterators.end();
			{
				FS_SET_OPERATION(kRemoving);
				
				FS_ADD_POSSIBLE_ANSWER(fSkipEntry + fRetryEntry);
				
				OperationBegins();
				
				while (pos < end) {
				
					RemoveRecursive(**pos, true, true);	// progress indication is on; first run
					
					delete *pos;
					*pos = 0;	// must be cleared because in case of an exception we delete all items. faster then erase().
					
					++pos;
				}
			}
						
		} catch (FSException e) {
		
			for_each(trash_iterators.begin(), trash_iterators.end(), deleter<EntryIterator>());
			sEmptyTrashRunning = 0;
			return e;
		}
	}
	
	sEmptyTrashRunning = 0;

	return B_OK;
}

ONLY_WITH_TRACKER(

	EntryList::EntryList(const PoseList &inlist) FS_NOTHROW {
		int32 count = inlist.CountItems();
		reserve(count);
		back_insert_iterator<inherited> bii(*this);
		
		for (int32 i = 0;  i < count;  ++i) {
			*bii = *inlist.ItemAt(i) -> TargetModel() -> EntryRef();
			++bii;
		}
	}

);


status_t
FSContext::GetOriginalPath(BEntry &in_entry, BPath &in_path) FS_NOTHROW {
	BNode node(&in_entry);
	if (node.InitCheck() != B_OK)
		return node.InitCheck();
	return GetOriginalPath(node, in_path);
}

bool
FSContext::IsInTrash(const entry_ref &in_ref) FS_NOTHROW {
	BEntry entry(&in_ref);
	if (entry.InitCheck() != B_OK)
		return false;
	return IsInTrash(entry);
}

bool
FSContext::IsInTrash(const BEntry &in_entry) FS_NOTHROW {
	BDirectory trash;
	entry_ref ref;
	if (in_entry.GetRef(&ref) != B_OK ||  GetTrashDir(trash, ref.device) != B_OK)
		return false;
	return trash.Contains(&in_entry);
}

bool
FSContext::IsTrashDir(const entry_ref &in_ref) FS_NOTHROW {
	BDirectory dir(&in_ref);
	if (dir.InitCheck() != B_OK)
		return false;
	return IsSpecialDir(dir, B_TRASH_DIRECTORY, sTrashDirList);
}

bool
FSContext::IsTrashDir(const BEntry &in_entry) FS_NOTHROW {
	BDirectory dir(&in_entry);
	if (dir.InitCheck() != B_OK)
		return false;
	return IsSpecialDir(dir, B_TRASH_DIRECTORY, sTrashDirList);
}

bool
FSContext::IsTrashEmpty() FS_NOTHROW {
	BDirectory dir;
	BVolume vol;
	BVolumeRoster roster;
	roster.Rewind();
	
	while (roster.GetNextVolume(&vol) == B_OK) {
		if (GetSpecialDir(dir, vol.Device(), B_TRASH_DIRECTORY, sTrashDirList) != B_OK)
			continue;
		
		if (dir.CountEntries() > 0)
			return false;
	}
	
	return true;
}

bool
FSContext::IsHomeDir(const BEntry &in_entry) FS_NOTHROW {
	BDirectory dir(&in_entry);
	if (dir.InitCheck() != B_OK)
		return false;
	return IsSpecialDir(dir, B_USER_DIRECTORY, sHomeDirList);
}

bool
FSContext::IsHomeDir(const entry_ref &in_ref) FS_NOTHROW {
	BDirectory dir(&in_ref);
	if (dir.InitCheck() != B_OK)
		return false;
	return IsSpecialDir(dir, B_USER_DIRECTORY, sHomeDirList);
}

bool
FSContext::IsPrintersDir(const BEntry &in_entry) FS_NOTHROW {
	BDirectory dir(&in_entry);
	if (dir.InitCheck() != B_OK)
		return false;
	return IsSpecialDir(dir, B_USER_PRINTERS_DIRECTORY, sPrintersDirList);
}

bool
FSContext::IsDevDir(const char* in_string) FS_NOTHROW {
	BString teststring = in_string;
	teststring = teststring.Truncate(4);
	if (strcmp(teststring.String(), "/dev/") == 0 || strcmp(teststring.String(), "/dev") == 0)
		return true;
	else
		return false;
}

bool
FSContext::IsDevDir(const BPath &in_path) FS_NOTHROW {
	BString teststring = in_path.Path();
	return IsDevDir(teststring.String());
}

bool
FSContext::IsDevDir(const BEntry &in_entry) FS_NOTHROW {
	BPath path;
	in_entry.GetPath(&path);
	return IsDevDir(path);
}

bool
FSContext::IsSystemDir(const BEntry &in_entry) FS_NOTHROW {
	BDirectory dir(&in_entry);
	if (dir.InitCheck() != B_OK)
		return false;
	return IsSpecialDir(dir, B_BEOS_SYSTEM_DIRECTORY, sSystemDirList);
}

bool
FSContext::IsBeOSDir(const BEntry &in_entry) FS_NOTHROW {
	BDirectory dir(&in_entry);
	if (dir.InitCheck() != B_OK)
		return false;
	return IsSpecialDir(dir, B_BEOS_DIRECTORY, sBeOSDirList);
}

status_t
FSContext::GetBootDesktopDir(BDirectory &in_dir) FS_NOTHROW {
	BVolume root;
	BVolumeRoster().GetBootVolume(&root);
	return GetSpecialDir(in_dir, root.Device(), B_DESKTOP_DIRECTORY, sDesktopDirList);
}

bool
FSContext::IsDesktopDir(const BEntry &in_entry) FS_NOTHROW {
	BDirectory dir(&in_entry);
	if (dir.InitCheck() != B_OK)
		return false;
	return IsSpecialDir(dir, B_DESKTOP_DIRECTORY, sDesktopDirList);
}

bool
FSContext::IsSpecialDir(const BDirectory &in_dir, directory_which in_which, node_ref_list_t &in_list) FS_NOTHROW {
	node_ref ref;
	if (in_dir.GetNodeRef(&ref) != B_OK)
		return false;

	return IsSpecialDir(ref, in_which, in_list);
}


void
FSContext::NextEntryCreated(BDirectory &target_dir, const char *target_name, BDirectory *for_pi_noderef) {

	status_t rc;
	BNode target_node;
	FS_OPERATION(target_node.SetTo(&target_dir, target_name));

	node_ref target_dir_ref;	// XXX rework... look at the comment at PoseInfo in Utility.h
	if (for_pi_noderef) {
		FS_OPERATION(for_pi_noderef -> GetNodeRef(&target_dir_ref));
	} else {
		FS_OPERATION(target_dir.GetNodeRef(&target_dir_ref));
	}
	
	NextEntryCreated(target_dir_ref, target_node);
}

bool
FSContext::RemoveIfNewer(BEntry &source_entry, BDirectory &target_dir, const char *target_name) FS_THROW_FSEXCEPTION {
	status_t rc;
	BEntry target_entry;
	
	FS_OPERATION_ETC(target_entry.SetTo(&target_dir, target_name), rc != B_ENTRY_NOT_FOUND, NOP);
	if (rc == B_OK)
		return RemoveIfNewer(source_entry, target_entry);

	return false;
}
	
bool
FSContext::RemoveIfNewer(BEntry &source_entry, BEntry &target_entry) FS_THROW_FSEXCEPTION {
	status_t rc;
	time_t source_mod, target_mod;
	
	FS_OPERATION(source_entry.GetModificationTime(&source_mod));
	FS_OPERATION(target_entry.GetModificationTime(&target_mod));
	
	if (source_mod > target_mod) {
		FS_SET_OPERATION(kRemoving);
		FS_SKIPABLE_OPERATION(Remove(target_entry));
		return true;
	}
	return false;
}


}	// namespace fs
