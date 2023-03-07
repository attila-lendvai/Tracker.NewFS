#if !defined(_FSCONTEXT_H)
#define _FSCONTEXT_H

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

#include <exception>
#include <Entry.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Beep.h>
#include <List.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <String.h>

#include <unistd.h>

#include <vector>

#include <boost/call_traits.hpp>
#include <boost/utility.hpp>

#define FS_MONITOR_THREAD_WAITINGS	1		// print how much time the writer/reader thread was waiting for the next chunk

//#define FS_NOTHROW			throw()		// with these the object file is a lot bigger... ?!
//#define FS_THROW_FSEXCEPTION	throw(FSException)

//#define FULL_ERROR_EXCEPTION	1	// uncomment for a complete FSException with detailed error message

#if !defined(FS_NOTHROW)
  #define FS_NOTHROW
#endif

#if !defined(FS_THROW_FSEXCEPTION)
  #define FS_THROW_FSEXCEPTION
#endif

#define FS_CONTROL_THROW(x)		throw (fs::FSException(x))

#if FULL_ERROR_EXCEPTION
  #define FS_ERROR_THROW(x, y)	throw (fs::FSException(x, y))
#else
  #define FS_ERROR_THROW(x, y)	throw (fs::FSException(y))
#endif


#if _BUILDING_tracker
  #include "Pose.h"
  #include "PoseList.h"
  #include "Attributes.h"
  #define ONLY_WITH_TRACKER(x)	x
#else
  #define ONLY_WITH_TRACKER(x)
#endif


#define NOP (void)0

#define FS_FILTER_EXCEPTION(code, x)									\
	if (e == (code)) {													\
		x;																\
	} else throw;

#define FS_FILTER_EXCEPTION_2(code1, x1, code2, x2)						\
	if (e == (code1)) {													\
		x1;																\
	} else if (e == (code2)) {											\
		x2;																\
	} else throw;

// The skippable operation should be set before FS_SKIPABLE_OPERATION, that
// way the requester will show in braces which operation will be skipped
#define FS_SKIPABLE_OPERATION(x)										\
	try {																\
		FS_ADD_POSSIBLE_ANSWER(fSkipOperation);							\
		x;																\
	} catch (FSException e) {											\
		FS_FILTER_EXCEPTION(kSkipOperation, NOP)						\
	}

/*
// this will do something when leaving a block of code
#define FS_WHEN_LEAVING_SCOPE(x)										\
	struct foobar {														\
		FSContext &mContext;											\
																		\
		foobar(FSContext &icontext) : mContext(icontext) {}				\
		~foobar() {														\
			x;															\
		}																\
	} _when_leaving_scope_(*this);
*/

#define FS_OPERATION_ETC(x, cond, cleanup)								\
	try {																\
		while ((rc = (x)) < 0  &&  (cond)  &&							\
				(ErrorHandler(rc)));									\
	} catch (FSException e) {											\
		(cleanup);														\
		throw;															\
	}																	\

#define FS_OPERATION(x)													\
	while ((rc = (x)) < 0 &&  (ErrorHandler(rc)));

#define FS_STATIC_OPERATION(x)											\
	if ((rc = (x)) < 0)													\
		return rc;

#define FS_MEMBER_CLASS_OPERATION(x)									\
	while ((rc = (x)) < 0 &&  (mContext -> ErrorHandler(rc)));

#define FS_SET_OPERATION(x)												\
	FSContext :: OperationPusher _op_##x(x, this);

#define FS_MEMBER_CLASS_SET_OPERATION(x)								\
	FSContext :: OperationPusher _op_##x(x, mContext);

#define FS_SET_POSSIBLE_ANSWERS(x)										\
	FSContext :: AnswerChanger _apa_((x), this);
#define FS_ADD_POSSIBLE_ANSWER(x)										\
	FSContext :: AnswerChanger _apa_(static_cast<answer_flags>(mPossibleAnswers | (x)), this);
#define FS_REMOVE_POSSIBLE_ANSWER(x)									\
	FSContext :: AnswerChanger _apa_(static_cast<answer_flags>(mPossibleAnswers & (~(x))), this);

#define FS_INIT_COPY_PROGRESS(x)										\
	FSContext :: FileProgressAdder _fpa_(x, this);

#define FS_SET_CURRENT_ENTRY(x)											\
	FSContext :: CurrentEntrySetter _ces_((x), this);

#define FS_BACKUP_VARIABLE_AND_SET(x, value)							\
	backup_struct<typeof(x)> __backup_of_##x(&(x));						\
	##x = (value);

namespace fs {

	using namespace boost;

class EntryIterator;

static const int32	kMaxNestedOperationCount		= 32;	// operation stack size
static const int32	kMaxDefaultErrorAnswers			= 32;	// up to this much default error answers will be stored
															// if it's exceeded then the default answer won't be recorded
															// and the requester will still keep coming up again and again

#if FULL_ERROR_EXCEPTION

class FSException : public exception {
	status_t		mStatus;
	const char *	mWhat;
	bool			mDeleteWhat;

	FSException &operator=(const FSException &);
	
public:
	FSException(const FSException &other);

	FSException();
	FSException(const FSContext::command);
	FSException(const status_t inStatus);
	FSException(const char *inStr);
	FSException(const char *inStr, const status_t inStatus);
	FSException(const char *inStr1, const char *inStr2, const char *inStr3,
			const status_t inStatus);
	~FSException();

	operator status_t () const {
		return mStatus;
	}
	
	virtual	const char *what() const;
};

#else

typedef status_t FSException;

#endif	// FULL_ERROR_EXCEPTION

enum {
	FS_ERRORS_END = B_ERRORS_END + 2000
};


// this is a wrapper around entry_ref, it will not allocate buffer for the name string if it's less then 32 bytes long
class EntryRef {
	static const int32 kBufferSize = 32;

	entry_ref	mRef;
	
	char		mBuffer[kBufferSize];
	int32		mSize;
	bool			mDeleteName;

	void	initialize() {
				mRef.name = mBuffer;
				mSize = kBufferSize;
				mDeleteName = false;
			}
			
public:

	EntryRef(const EntryRef &other)									{ initialize(); SetTo(other); }
	EntryRef() FS_NOTHROW											{ initialize(); }
	EntryRef(dev_t idev, ino_t idir, const char *iname) FS_NOTHROW;
	EntryRef(const entry_ref &) FS_NOTHROW;
	~EntryRef();

	const char *Name() const FS_NOTHROW 							{ return mRef.name; }
	dev_t		Device() const FS_NOTHROW 							{ return mRef.device; }
	ino_t		Directory() const FS_NOTHROW						{ return mRef.directory; }

	bool		IsYourParent(const node_ref &ref) const FS_NOTHROW	{ return (ref.device == mRef.device  &&  ref.node == mRef.directory); }
	void		GetParentDirNodeRef(node_ref &ref) const FS_NOTHROW	{ ref.device = mRef.device; ref.node = mRef.directory; }
	void		SetTo(const EntryRef &ref) FS_NOTHROW				{ SetTo(ref.Device(), ref.Directory(), ref.Name()); }
	void		SetTo(const entry_ref &ref) FS_NOTHROW				{ SetTo(ref.device, ref.directory, ref.name); }
	void		SetTo(dev_t dev, ino_t dir, const char *name) FS_NOTHROW;

	// !!! watch out, these are const entry_refs, may not be modified. you'll get a warning or error, too
					operator const entry_ref *() const				{ return &mRef; }
					operator const entry_ref &() const				{ return mRef; }
	EntryRef &		operator=(const entry_ref &other)				{ SetTo(other); return *this;}
	EntryRef &		operator=(const entry_ref *other)				{ SetTo(*other); return *this; }
	EntryRef &		operator=(const EntryRef &other)				{ SetTo(other); return *this; }
};

#if _BUILDING_tracker

class TrashedEntryRef : public EntryRef {	// be careful when using, should be protected inheritance
	time_t		mTrashedDate;			// (mTrashedDate is cached, and does not handle SetTo()!)
	
public:
	TrashedEntryRef() : mTrashedDate(0) { }
	
	time_t	TrashedDate() {
				if (mTrashedDate == 0) {
					BNode node(*this);
					if (node.ReadAttr(kAttrTrashedDate, B_TIME_TYPE, 0, &mTrashedDate, sizeof(mTrashedDate)) < 0)
						mTrashedDate = 1;	// very old
				}
				return mTrashedDate;
			}
};

#endif	// _BUILDING_tracker

class WDChanger : noncopyable {
	char	mOldPath[B_PATH_NAME_LENGTH];
	bool	mChangeBack;

public:
	WDChanger() FS_NOTHROW : mChangeBack(false) {}
	
	~WDChanger() FS_NOTHROW {
		if (mChangeBack)
			chdir(mOldPath);
	}

	status_t	Change(const BPath &newpath) FS_NOTHROW {
					getcwd(mOldPath, B_PATH_NAME_LENGTH);
					mChangeBack = true;
					return chdir(newpath.Path());
				}
};



class FSContext : noncopyable {
public:	
	typedef vector<node_ref>	node_ref_list_t;

	enum answer_flags {
		fCancel			= 0x0001,
		fSkipEntry		= 0x0002,
		fSkipOperation	= 0x0004,
		fRetryEntry		= 0x0008,
		fRetryOperation	= 0x0010,
		fSkipDirectory	= 0x0020,
		fIgnore			= 0x0040,
		
		fDefaultPossibleAnswers = fRetryOperation + fCancel
	};
	
	enum copy_flags {
		fCreationTime		= 0x0001,
		fOwner				= 0x0002,
		fGroup				= 0x0004,
		fPermissions		= 0x0008,
		fAttributes			= 0x0010,
		fModificationTime	= 0x0020,
		
		fDefaultCopyFlags	= fAttributes | fOwner | fGroup | fPermissions | fCreationTime | fModificationTime
	};

	enum command {
		#define FSCMD_INITED(a, b, c, d)	a = c,
		#define FSCMD_HIDDEN(a, b)		a = b,
		#define FSCMD(a, b, c)			a,
		
		#include "FSCommands.tbl"
		
		kTotalCommands,
	};

	static const int32 kMaxAnswersForInteraction = 14;	// max number of possible answers + 1

	enum interaction {
		// Interaction constants for FSContext::Interaction().
		#define		INT_0(name, string, showfile)													name,
		#define		INT_1(name, string, showfile, a, ax)												name,
		#define		INT_2(name, string, showfile, a, ax, b, bx)										name,
		#define		INT_3(name, string, showfile, a, ax, b, bx, c, cx)									name,
		#define		INT_4(name, string, showfile, a, ax, b, bx, c, cx, d, dx)							name,
		#define		INT_5(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex)						name,
		#define		INT_6(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx)					name,
		#define		INT_7(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx, g, gx)				name,
		#define		INT_8(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx, g, gx, h, hx)		name,
		#define		INT_9(name, string, showfile, a, ax, b, bx, c, cx, d, dx, e, ex, f, fx, g, gx, h, hx, i, ix)	name,
		#define		INT_10(name, string, showfile,a,ax,b,bx,c,cx,d,dx,e,ex,f,fx,g,gx,h,hx,i,ix,j,jx)			name,
		#define		INT_11(name, string, showfile,a,ax,b,bx,c,cx,d,dx,e,ex,f,fx,g,gx,h,hx,i,ix,j,jx,k,kx)		name,
		#define		INT_12(name, string, showfile,a,ax,b,bx,c,cx,d,dx,e,ex,f,fx,g,gx,h,hx,i,ix,j,jx,k,kx,l,lx)	name,
		#define		INT_13(name, string, showfile,a,ax,b,bx,c,cx,d,dx,e,ex,f,fx,g,gx,h,hx,i,ix,j,jx,k,kx,l,lx,m,mx) name,
		#include	"FSInteractions.tbl"

		kTotalInteractions
	};
	
	enum operation {

		#define FSOP(name, string, suffix)	name,
		#include "FSOperations.tbl"

		kTotalOperations,
		kInvalidOperation = -1
	};
	
	struct InteractionDescription {
		const char *		mUserVisibleString;
		bool				mShouldPrintFileInfo;
		command		mPossibleAnswers[kMaxAnswersForInteraction];
		bool				mAnswerIsExtended[kMaxAnswersForInteraction];
	};
	
	static const char *	sCommandStringTable[kTotalCommands];
	static const char *	sTargetDirPrefixTable[kTotalOperations];
	static const char *	sOperationStringTable[kTotalOperations];
	static const InteractionDescription sInteractionsTable[kTotalInteractions];

	class ProgressInfo : noncopyable {
	public:
		off_t				mCurrentFileTotalSize;
		off_t				mTotalSize;
		int32				mTotalEntryCount;
		int32				mTotalFileCount;
		int32				mTotalDirCount;
		int32				mTotalLinkCount;
		volatile off_t			mCurrentFileCurrentSize;
		volatile off_t			mCurrentSize;
		int32				mCurrentEntryCount;
		int32				mCurrentFileCount;
		int32				mCurrentDirCount;
		int32				mCurrentLinkCount;
		bool					mDirty;
		bool					mTotalDisabled;		// this is set to true when an entry is skipped in such a way that it's impossible to follow total values right. (skipping a whole dir, no stat info yet available about the entry...)
		bool					mTotalSizeProgressEnabled;	// set by operations that are related to size, not only count (copy and duplicate)

				// Functions
				ProgressInfo() FS_NOTHROW							{ Clear(); }
				
				void		Clear() FS_NOTHROW						{ memset(this, 0, sizeof(*this)); }
				
				void		EntryDone() FS_NOTHROW					{ ++mCurrentEntryCount; SetDirty(); }
				void		DirectoryDone() FS_NOTHROW				{ ++mCurrentDirCount; EntryDone(); }
				void		FileDone() FS_NOTHROW						{ ++mCurrentFileCount; EntryDone(); }
				void		LinkDone() FS_NOTHROW						{ ++mCurrentLinkCount; EntryDone(); }

				void		NewEntry() FS_NOTHROW					{ ++mTotalEntryCount; SetDirty(); }
				void		NewDirectory() FS_NOTHROW				{ ++mTotalDirCount; NewEntry(); }
				void		NewFile(off_t &size) FS_NOTHROW			{ ++mTotalFileCount; mTotalSize += size; NewEntry(); }
				void		NewLink() FS_NOTHROW						{ ++mTotalLinkCount; NewEntry(); }

				void		SkipEntry() FS_NOTHROW						{ --mTotalEntryCount; mCurrentFileCurrentSize = 0; SetDirty(); }
				void		SkipFile(off_t &size) FS_NOTHROW			{ --mTotalFileCount; mTotalSize -= size; SkipEntry(); }
				void		SkipLink() FS_NOTHROW						{ --mTotalLinkCount; SkipEntry(); }
				void		SkipDirectory() FS_NOTHROW					{ --mTotalDirCount; SkipEntry(); DisableTotals(); }
				
				// XXX replace with atomic_add64 when available
				void		ReadProgress(size_t size) FS_NOTHROW		{ mCurrentSize += size / 2; mCurrentFileCurrentSize += size / 2; SetDirty(); }
				void		WriteProgress(size_t size) FS_NOTHROW		{ mCurrentSize += size / 2; mCurrentFileCurrentSize += size / 2; SetDirty(); }
				
				float	EntryProgress() const FS_NOTHROW			{ return (mTotalEntryCount > 0) ? (float)mCurrentEntryCount / mTotalEntryCount : 0; }
				void		DisableTotalSizeProgress() FS_NOTHROW		{ mTotalSizeProgressEnabled = false; }
				void		EnableTotalSizeProgress() FS_NOTHROW		{ mTotalSizeProgressEnabled = true; }
				bool		IsTotalSizeProgressEnabled() const FS_NOTHROW	{ return mTotalSizeProgressEnabled; }
				float	TotalSizeProgress() const FS_NOTHROW		{ return (mTotalSize > 0) ? (float)mCurrentSize / mTotalSize : 0; }
				bool		HasFileSizeProgress() const FS_NOTHROW		{ return mCurrentFileTotalSize != 0; }
				float	FileSizeProgress() const FS_NOTHROW			{ return (mCurrentFileTotalSize > 0) ? (float)mCurrentFileCurrentSize / mCurrentFileTotalSize : 0; } 
	
				bool		IsTotalEnabled() const FS_NOTHROW			{ return ! mTotalDisabled; }
				void		DisableTotals() FS_NOTHROW					{ mTotalDisabled = true; }
				
				bool		IsDirty() const FS_NOTHROW					{ return mDirty; }
				void		ClearDirty() FS_NOTHROW					{ mDirty = false; }
				void		SetDirty() FS_NOTHROW						{ mDirty = true; }
				
				void		PrintToStream() FS_NOTHROW;
	};
	
private:
	friend struct FileProgressAdder : noncopyable {
		off_t			mOldCurrentSize;
		FSContext		*mContext;
		
		FileProgressAdder(off_t &isize, FSContext *icontext) : mContext(icontext) {
			mOldCurrentSize = Info().mCurrentSize;

			Info().mCurrentFileTotalSize = isize;
			Info().mCurrentFileCurrentSize = 0;
			
			mContext -> EffectiveCopyBegins();
		}
		~FileProgressAdder() {
			mContext -> EffectiveCopyEnds();
			Info().mCurrentFileTotalSize = 0;	// means that there's no file copy progressbar
			if (Info().mCurrentSize < mOldCurrentSize + Info().mCurrentFileTotalSize)	// if file was not finished
				Info().mCurrentSize = mOldCurrentSize;			// restore old total size
		}
		FSContext::ProgressInfo &Info() {
			return mContext -> mProgressInfo;
		}
	};

	friend struct SourceDirSetter {
		FSContext		*mContext;
		node_ref		mOldNodeRef;
		dev_t			mCurrentDevice;
		bool			mEntryFromADifferentDir;
		
		SourceDirSetter(FSContext *icontext) : mContext(icontext), mCurrentDevice(0), mEntryFromADifferentDir(false) {
			mOldNodeRef = mContext -> mSourceDirNodeRef;
		}
	
		// sets mRootSourceDirNodeRef if required. SetSourceDir must have been called
		void SetRootSourceDir(bool enabled) {
			if (enabled  &&  mContext -> mRootSourceDirNodeRef != mContext -> mSourceDirNodeRef)
				mContext -> mRootSourceDirNodeRef = mContext -> mSourceDirNodeRef;
		}
		// sets mSourceDir
		status_t SetSourceDir(const EntryRef &ref) {
			if (mContext -> mSourceDir.InitCheck() != B_OK  ||  ref.IsYourParent(mContext -> mSourceDirNodeRef) == false) {
				mEntryFromADifferentDir = true;
				ref.GetParentDirNodeRef(mContext -> mSourceDirNodeRef);	// XXX what is if a whole volume is copied?
				status_t rc;
				if ((rc = mContext -> mSourceDir.SetTo(&mContext -> mSourceDirNodeRef)) != B_OK)
					return rc;
				
				if (mCurrentDevice != ref.Device()) {	// if we are dealing with a ref from a new device
					mCurrentDevice = ref.Device();
#if 0
					struct stat statbuf;	// XXX set as low limit?
					if (mContext -> mSourceDir.GetStat(&statbuf) == B_OK)
						mContext -> SuggestBufferSize(statbuf.st_blksize);
#endif
					mContext -> mSourceDevice = ref.Device();
					mContext -> mSameDevice = mContext -> IsTargetOnSameDevice();
				}
			} else {
				mEntryFromADifferentDir = false;
			}
			return B_OK;
		}
		
		bool IsEntryFromADifferentDir() const {
			return mEntryFromADifferentDir;		
		}
		
		~SourceDirSetter() {
			mContext -> mSourceDirNodeRef = mOldNodeRef;
			mContext -> mSourceDir.Unset();
		}
	};

	friend struct CurrentEntrySetter {
		FSContext *		mContext;
		const EntryRef *mOld;
		mode_t			mOldType;
		
		CurrentEntrySetter(const EntryRef &inref, FSContext *iContext)
							: mContext(iContext),
							  mOld(mContext -> CurrentEntry()),
							  mOldType(mContext -> CurrentEntryType()) {
			
			mContext -> SetCurrentEntry(&inref);
			mContext -> SetCurrentEntryType(0);		// no info available yet
		}
		
		~CurrentEntrySetter() {
//			mContext -> LeavingEntry();
			mContext -> SetCurrentEntry(mOld);
			mContext -> SetCurrentEntryType(mOldType);
		}
	};

	friend struct error_answer {
		operation	operation;
		status_t	error;
		command		answer;
		
		error_answer() {}
		error_answer(operation iop, status_t ierr, command ianswer) : operation(iop), error(ierr), answer(ianswer) {}

		bool Equals(const operation op, const status_t err) const {
			return op == operation  &&  error == err;
		}
	};
	
//	friend struct ProgressBackup {					// overkill... but it would make it possible to keep
//		struct progress_info	mOldProgressInfo;	// progress bar in sync when skipping dirs
//		FSContext				&mContext;
//		
//		ProgressBackup(FSContext &icontext) : mContext(icontext) { Backup(); }
//		~ProgressBackup() {}
//
//		void Backup() {
//			memcpy(&mOldProgressInfo, &mContext.mProgressInfo, sizeof(progress_info));
//		}
//		void Restore() {
//			memcpy(&mContext.mProgressInfo, &mOldProgressInfo, sizeof(progress_info));
//		}
//	};

	template <typename T, int size>
	class Stack {
	protected:
		T		mElements[size];
		int		mPosition;
	public:
				Stack() : mPosition(0) { }
		void	Push(call_traits<T>::param_type item) {
					if (mPosition < size) {
						mElements[mPosition] = item;
						mPosition++;
					} else
						DEBUGGER("A stack ran out of the reserved space");
				}
		void	Pop() {
					if (mPosition > 0)
						--mPosition;
					else
						DEBUGGER("Pop on a stack with no elements");
				}
		int		CountItems() const				{ return mPosition; }
		const call_traits<T>::param_type
					Current() const				{ ASSERT(mPosition > 0); return mElements[(mPosition) ? mPosition - 1 : 0]; }
		const call_traits<T>::param_type
					operator[](int index) const	{ ASSERT(index >= 0  &&  index < mPosition); return mElements[index]; }
//		void	Clear()					{ memset(mElements, 0, sizeof(size * sizeof(mElements[0]))); }

// STL compatibility
//		typedef	T *			iterator;
//		typedef	const T *	const_iterator;
//		
//		iterator		begin() 			{ return &mElements[0]; }
//		const_iterator	begin() const		{ return &mElements[0]; }
//		iterator		end()				{ return &mElements[mPosition]; }	// last element + 1
//		const_iterator	end() const			{ return &mElements[mPosition]; }	// last element + 1
//		size_t			size() const		{ return CountItems(); }
	};

	friend struct CreateSemScoped {	// create a semaphore, delete when leaving scope (exception safe)
		FSContext	*mContext;
		sem_id &	mSemaphore;
		int32		mCount;
		const char *	mName;
		
		CreateSemScoped(sem_id &sem, int32 count, const char *name, FSContext *context) :
						mContext(context), mSemaphore(sem), mCount(count), mName(name) { }
						
		~CreateSemScoped() {
			if (mSemaphore > 0)
				delete_sem(mSemaphore);
		}
		
		status_t DoIt() {
			FS_MEMBER_CLASS_SET_OPERATION(kCreatingSemaphore);
			mSemaphore = create_sem(mCount, mName);
			return static_cast<status_t>(mSemaphore);
		}
	};

	friend class OperationStack : public Stack<operation, kMaxNestedOperationCount> {
		BString			mStackString;

	public:
		OperationStack()	{ mElements[0] = kInvalidOperation; }
		
	static	const char *AsString(operation) FS_NOTHROW;
			const char *AsString() FS_NOTHROW;			// the returned string is only valid until the next call
			operation	RootOperation() const FS_NOTHROW		{ ASSERT(CountItems() > 0); return mElements[0]; }
			bool		Contains(operation in_op) const FS_NOTHROW {
							for (int32 i = 0;  i < mPosition;  ++i) {
								if (mElements[i] == in_op)
									return true;
							}
							return false;
						}
			operation	LastPrimaryOperation() const FS_NOTHROW {
							operation op = kInvalidOperation;
							for (int32 i = 0;  i < mPosition;  ++i) {
								if (mElements[i] < kLastPrimaryOperation)
									op = mElements[i];
							}
							return op;
						}
//			bool		IsRootOperation() FS_NOTHROW			{ return CountItems() <= 1; }
	};

	friend struct OperationPusher {
		FSContext *mContext;
		
		OperationPusher(operation iop, FSContext *iContext) : mContext(iContext) {
			if (mContext -> PushOperation(iop) == false)
				mContext = 0;									// don't push the same code again, recursion safe
		}
		~OperationPusher() {
			if (mContext)
				mContext -> PopOperation();
		}
	};

	friend struct AnswerChanger {
		FSContext		*mContext;
		answer_flags	mOldFlags;
		operation		mOldSkipOpTarget;
		
		AnswerChanger(answer_flags iFlags, FSContext *iContext) :
							mContext(iContext),	mOldSkipOpTarget(kInvalidOperation) {
							
			mOldFlags = mContext -> mPossibleAnswers;
			mContext -> mPossibleAnswers = iFlags;
			if (iFlags & fSkipOperation) {
				mOldSkipOpTarget = mContext -> mSkipOperationTargetOperation;
				mContext -> mSkipOperationTargetOperation = mContext -> mOperationStack.Current();
			}
		}
		
		~AnswerChanger() {	// restore original values
			mContext -> mPossibleAnswers = mOldFlags;
			if (mOldSkipOpTarget != kInvalidOperation)
				mContext -> mSkipOperationTargetOperation = mOldSkipOpTarget;
		}
	};

	class ChunkList : public vector<pair<uint8 *, size_t> *> {
		public:
			ChunkList() {}
			~ChunkList() {
				iterator pos = begin();
				while (pos != end()) {
					delete [] (*pos) -> first;
					delete *pos;
					++pos;
				}
			}
	};

public:
// Public operations, the API
			status_t	CopyTo(EntryIterator *i, BDirectory &target_dir, bool async)			FS_NOTHROW;
			status_t	CopyTo(EntryIterator &i, BDirectory &target_dir)						FS_NOTHROW;
			status_t	CopyFileTo(entry_ref &_ref, BDirectory &target_dir, char *target_name) FS_NOTHROW;
			status_t	Duplicate(EntryIterator *i, bool async)									FS_NOTHROW;
			status_t	Duplicate(EntryIterator &i)												FS_NOTHROW;
			status_t	MoveTo(EntryIterator *i, BDirectory &target_dir, bool async)			FS_NOTHROW;
			status_t	MoveTo(EntryIterator &i, BDirectory &target_dir)						FS_NOTHROW;
			status_t	CreateLinkTo(EntryIterator *i, BDirectory &target_dir, bool relative, bool async) FS_NOTHROW;
			status_t	CreateLinkTo(EntryIterator &i, BDirectory &target_dir, bool relative)	FS_NOTHROW;
			status_t	MoveToTrashAsync(EntryIterator *i, bool async)							FS_NOTHROW;	// no way to select for &FSContext::MoveToTrash from the two functions if the MoveToTrash name is overloaded
			status_t	MoveToTrash(EntryIterator &i)											FS_NOTHROW;
			status_t	RestoreFromTrash(EntryIterator *i, bool async)							FS_NOTHROW;
			status_t	RestoreFromTrash(EntryIterator &i)										FS_NOTHROW;
			status_t	Remove(EntryIterator *i, bool askbefore, bool async)					FS_NOTHROW;
			status_t	Remove(EntryIterator &i, bool askbefore)								FS_NOTHROW;
			status_t	CalculateItemsAndSize(EntryIterator &i, ProgressInfo &)					FS_NOTHROW;
			status_t	EmptyTrash(bool async = true)											FS_NOTHROW;
			
	static	status_t	CreateNewFolder(const node_ref &in_dir_node, const char *name = 0, entry_ref * = 0, node_ref * = 0) FS_NOTHROW;
	static	void		MakeUniqueName(BDirectory &, char *name_buf, const char *suffix = 0)	FS_NOTHROW;

	static	status_t	GetOriginalPath(BEntry &in_entry, BPath &in_path)						FS_NOTHROW;
	static	status_t	GetOriginalPath(BNode &in_node, BPath &in_path)							FS_NOTHROW;
	
	static	bool		IsInTrash(const entry_ref &in_ref)										FS_NOTHROW;
	static	bool		IsInTrash(const BEntry &in_entry)										FS_NOTHROW;
	static	bool		IsTrashDir(const entry_ref &in_ref)										FS_NOTHROW;
	static	bool		IsTrashDir(const BEntry &in_entry)										FS_NOTHROW;
	static	bool		IsHomeDir(const BEntry &in_entry)										FS_NOTHROW;
	static	bool		IsPrintersDir(const BEntry &in_entry)									FS_NOTHROW;
	static	status_t	GetBootDesktopDir(BDirectory &in_dir)									FS_NOTHROW;
	static	bool		IsDesktopDir(const BEntry &in_entry)									FS_NOTHROW;

//	static	bool		IsTrashDir(const BEntry *);
//	static	bool		IsDesktopDir(const BEntry *);

	static	status_t	GetTrashDir(BDirectory &in_dir, dev_t in_device) FS_NOTHROW {
						return GetSpecialDir(in_dir, in_device, B_TRASH_DIRECTORY, sTrashDirList);
					}
	static	bool		IsTrashDir(const node_ref &in_ref) FS_NOTHROW {
						return IsSpecialDir(in_ref, B_TRASH_DIRECTORY, sTrashDirList);
					}
	static	bool		IsTrashDir(const BDirectory &in_dir) FS_NOTHROW {
						return IsSpecialDir(in_dir, B_TRASH_DIRECTORY, sTrashDirList);
					}

	static	bool		IsHomeDir(const BDirectory &in_dir) FS_NOTHROW {
						return IsSpecialDir(in_dir, B_USER_DIRECTORY, sHomeDirList);
					}
	static	status_t	GetDesktopDir(BDirectory &in_dir, dev_t in_device) FS_NOTHROW {
						return GetSpecialDir(in_dir, in_device, B_DESKTOP_DIRECTORY, sDesktopDirList);
					}
	static	bool		IsDesktopDir(const BDirectory &in_dir) FS_NOTHROW {
						return IsSpecialDir(in_dir, B_DESKTOP_DIRECTORY, sDesktopDirList);
					}
	static	bool		IsDesktopDir(const node_ref &in_dir) FS_NOTHROW {
						return IsSpecialDir(in_dir, B_DESKTOP_DIRECTORY, sDesktopDirList);
					}

private:
						FSContext(const FSContext &);
protected:
						FSContext() FS_NOTHROW;
	virtual				~FSContext() FS_NOTHROW;

// Misc
	static	bool		IsSpecialDir(const node_ref &in_dir, directory_which in_which, node_ref_list_t &in_list) FS_NOTHROW;
	static	bool		IsSpecialDir(const BDirectory &in_dir, directory_which in_which, node_ref_list_t &in_list) FS_NOTHROW;
	static	status_t	GetSpecialDir(node_ref &in_dir_ref, dev_t in_device, directory_which in_which, node_ref_list_t &in_list) FS_NOTHROW;
	static	status_t	GetSpecialDir(BDirectory &in_dir, dev_t in_device, directory_which in_which, node_ref_list_t &in_list) FS_NOTHROW;
			void		MakeUniqueName(BPath &, const char * = 0) FS_THROW_FSEXCEPTION;
public:
	static	int32	GetSizeString(char *in_ptr, float in_size1, float in_size2 = 0);

// Options
	virtual	bool		ShouldCopy(copy_flags flag) FS_NOTHROW;	// XXX virtual?
			void		DontCopy(copy_flags flag) FS_NOTHROW {
						mCopyFlags = (copy_flags)(mCopyFlags & ~flag);
					}

// Interaction
public:
	static	const command *PossibleAnswersFor(interaction iact) FS_NOTHROW {
						ASSERT(iact >= 0  &&  iact < kTotalInteractions);
						return sInteractionsTable[iact].mPossibleAnswers;
					}	// returns a zero terminated list of possible commands
	static	bool		IsExtendedCommand(interaction iact, command icmd) FS_NOTHROW;
	static	bool		IsExtendedInteraction(interaction iact) FS_NOTHROW {
						ASSERT(iact >= 0  &&  iact < kTotalInteractions);
						return sInteractionsTable[iact].mShouldPrintFileInfo;
					}
	static	bool		IsThrowableCommand(command cmd) FS_NOTHROW {
						return ((uint32)cmd == 0  ||
								((uint32)cmd >= (uint32)kFirstThrowableCommand  &&
									(uint32)cmd <= (uint32)kLastThrowableCommand));
					}
	static	bool		IsDefaultableCommand(command cmd) FS_NOTHROW {
						switch (cmd) {
							case kSuppliedNewNameForSource:
							case kSuppliedNewNameForTarget:
							case kRetryOperation:
							case kRetryEntry:
								return false;
							
							default:
								return true;
						}
					}

			operation	SkipOperationTarget() FS_NOTHROW					{ return mSkipOperationTargetOperation; }	// the operation that will be skipped by a thrown kSkipOperation
protected:
	virtual	void			CheckCancel() FS_NOTHROW	{ }
			void			CheckCancelInCopyFile() FS_THROW_FSEXCEPTION;
	virtual	command	Interaction(interaction icode) FS_THROW_FSEXCEPTION = 0;
public:
			// these are _OPTIONALLY_ non-zero when calling Interaction()
			char *		SourceNewName()										{ return mSourceNewName; }
			char *		TargetNewName()										{ return mTargetNewName; }
			BEntry *		TargetEntry()										{ return mTargetEntry; }

			size_t		TotalMemoryUsage() FS_NOTHROW {
							if (mChunkList) {
								size_t size = 0;
								ChunkList::iterator pos = mChunkList -> begin(), end = mChunkList -> end();
								while (pos != end) {
									size += (*pos) -> second;
									++pos;
								}
								return size;
							} else {
								return mBufferSize;
							}
						}
protected:
				
// Buffer management
			bool			IsBufferReleasable() FS_NOTHROW const				{ return mBufferReleasable; }	// true only while it's ok to free and reallocate the buffer
			uint8 *		Buffer() FS_NOTHROW;
			size_t &		BufferSize()										{ return mBufferSize; }
			size_t		ReleaseBuffer() FS_NOTHROW {
			
							ASSERT(IsBufferReleasable() == true);
							
							size_t tmp = mBufferSize;
							delete [] mBuffer;
							mBuffer = 0;
							mBufferSize = 0;
							return tmp;
						}
			void			ReallocateBuffer(size_t) FS_NOTHROW {
						//	if (size)
						//		SetBufferSize(size);
						}
			void			SetBufferSize(size_t) FS_THROW_FSEXCEPTION;
			size_t &		MaxBufferSize()										{ return mMaxBufferSize; }
			void			SuggestBufferSize(size_t new_size) FS_NOTHROW;

// Tracker callbacks
			void		NextEntryCreated(BDirectory &target_dir, const char *target_name, BDirectory *for_pi_noderef = 0);
	virtual	void		NextEntryCreated(node_ref &, BNode &) FS_NOTHROW {}	// to save icon positions
	virtual	void		DirectoryTrashed(node_ref &) FS_NOTHROW {}			// to close open windows
	virtual	void		AboutToDeleteOrTrash(BEntry &) FS_NOTHROW {}		// to unmount a device (will not be called for every entry!)
	virtual	void		EffectiveCopyBegins() FS_NOTHROW {}
	virtual	void		EffectiveCopyEnds() FS_NOTHROW {}

// Default answers
public:
	static	const char *	AsString(interaction iact) FS_NOTHROW				{ return sInteractionsTable[iact].mUserVisibleString; }
	static	const char *	AsString(command) FS_NOTHROW;
	static	const char *	AsString(operation op) FS_NOTHROW					{ return OperationStack::AsString(op); }
protected:	command	DefaultErrorAnswer(status_t ierr) FS_THROW_FSEXCEPTION;
			void			SetDefaultErrorAnswer(command ianswer, status_t ierr) FS_NOTHROW;
			command	DefaultInteractionAnswer(interaction iact) FS_THROW_FSEXCEPTION;
			void			SetDefaultInteractionAnswer(interaction iact, command cmd) {
							mDefaultInteractionAnswers[iact] = cmd;
						}
			void			SetLastSelectedInteractionAnswer(interaction iact, command cmd) {
							sLastSelectedInteractionAnswers[iact] = cmd;
						}
			command	LastSelectedInteractionAnswer(interaction iact) FS_THROW_FSEXCEPTION	{ return sLastSelectedInteractionAnswers[iact]; }

// Progress indication
			void		ResetProgressIndicator() FS_NOTHROW;
	virtual	void		InitProgressIndicator() FS_NOTHROW					{ }
	virtual	void		PreparingOperation() FS_NOTHROW						{ }
	virtual	void		OperationBegins() FS_NOTHROW						{ }

			void		SetCurrentEntry(const EntryRef *inref) FS_NOTHROW	{ mCurrentEntry = inref; }
			void		SetCurrentEntryType(mode_t intype) FS_NOTHROW		{ mCurrentEntryType = intype; }

//			void		LeavingEntry() FS_NOTHROW							{ if (S_ISDIR(mCurrentEntryType)) PopDirectory(); }
//			void		PushDirectory(const char *) FS_NOTHROW;
//			void		PopDirectory() FS_NOTHROW;
public:

			bool		WasMultithreadedCopy()								{ return mWasMultithreaded; }	// was: { return mChunkList != 0; }
#if FS_MONITOR_THREAD_WAITINGS
		bigtime_t	ReaderThreadWaiting()								{ return mReaderThreadWaiting.ElapsedTime(); }
		bigtime_t	WriterThreadWaiting()								{ return mWriterThreadWaiting.ElapsedTime(); }
#endif

		const EntryRef *CurrentEntry() const FS_NOTHROW						{ return mCurrentEntry; }
			const char *CurrentEntryName() const FS_NOTHROW					{ return (mCurrentEntry) ? CurrentEntry() -> Name() : ""; }
			mode_t		CurrentEntryType() const FS_NOTHROW					{ return mCurrentEntryType; }
			const char *AsString(mode_t type) FS_NOTHROW;
protected:
			
// Operation management
public:
	static	const char *TargetDirPrefix(operation op) FS_NOTHROW			{ return sTargetDirPrefixTable[op]; }
	static	bool		HasTargetDir(operation op) FS_NOTHROW				{ return TargetDirPrefix(op) != 0; }
			status_t	GetRootTargetDir(BPath &path) {
							BDirectory dir(&mRootTargetDirNodeRef);
							BEntry entry;
							dir.GetEntry(&entry);
							return entry.GetPath(&path);
						}
			operation	CurrentOperation() FS_NOTHROW						{ return mOperationStack.Current(); }
protected:	bool		PushOperation(operation iop) FS_NOTHROW;
			void		PopOperation() FS_NOTHROW;
			int32		NestedOperationCount() FS_NOTHROW					{ return mOperationStack.CountItems(); }

// Error handling mainly implemented by derived classes
		answer_flags	PossibleAnswers() const FS_NOTHROW					{ return mPossibleAnswers; }
			bool		IsAnswerPossible(answer_flags iflag) const FS_NOTHROW { return PossibleAnswers() & iflag; }
	virtual	bool		ErrorHandler(status_t iStatus) FS_THROW_FSEXCEPTION = 0;

// Private fs operations
			void		AccumulateItemsAndSize(BEntry &, bool recursive = true) FS_THROW_FSEXCEPTION;
			void		AccumulateItemsAndSize(EntryIterator &i, bool recursive = true) FS_THROW_FSEXCEPTION;
			void		CalculateItemsAndSizeRecursive(EntryIterator &i, ProgressInfo &progress_info, bool recursive = true) FS_THROW_FSEXCEPTION;

			void		CopyRecursive(EntryIterator &i, BDirectory &target_dir, bool first_run = false) FS_THROW_FSEXCEPTION;
			void		CopyAdditionals(BNode &source, BNode &target) FS_THROW_FSEXCEPTION;
			void		CopyAttributes(BNode &source, BNode &target) FS_THROW_FSEXCEPTION;

			// These assume that mSourceDir is set to the actual source dir; EntryIterators are for inheriting filtering to new DirEntryIterators
			bool		CopyEntry(EntryIterator &, EntryRef &ref, BDirectory &target_dir, const char *target_name = 0, bool first_run = false) FS_THROW_FSEXCEPTION; // return value means wether the copy actually happened or skip was issued
			bool		CopyLink(EntryIterator &, BEntry &, BDirectory &target_dir, char *target_name) FS_THROW_FSEXCEPTION;
			void		RawCopyLink(const BEntry &source_entry, BDirectory &target_dir, char *target_name) FS_THROW_FSEXCEPTION;
			void		CopyDirectory(EntryIterator &, BEntry &, BDirectory &target_dir, char *target_name) FS_THROW_FSEXCEPTION;
			void		CopyFile(BEntry &, BDirectory &target_dir, char *target_name) FS_THROW_FSEXCEPTION;
			void		CopyFileInnerLoop(BFile &source, BFile &target, off_t size) FS_THROW_FSEXCEPTION;
			void		CopyFileInnerLoopTwoDevices(BFile &source, BFile &target) FS_THROW_FSEXCEPTION;
			void		CopyFileReaderThread(BFile *_file, ChunkList *, sem_id, sem_id, sem_id) FS_THROW_FSEXCEPTION;
			void		CopyFileWriterThread(BFile *_file, ChunkList *, sem_id, sem_id &reader_sem, sem_id) FS_THROW_FSEXCEPTION;
			void		CalculateNewChunkSize(size_t &chunk_size, off_t upper_limit, bigtime_t start_time) FS_NOTHROW;
			void		ThrowIfNecessary(command &input) FS_THROW_FSEXCEPTION {
							command cmd;
							if ((cmd = input) != kInvalidCommand) {
								input = kInvalidCommand;
								FS_CONTROL_THROW(cmd);
							}
						}

			void		RemoveRecursive(EntryIterator &i, bool progress_enabled, bool first_run) FS_THROW_FSEXCEPTION;
			void		Remove(BDirectory &dir, const char *file) FS_THROW_FSEXCEPTION;
			void		Remove(BPath &path) FS_THROW_FSEXCEPTION;
			void		Remove(BEntry &entry, operation = kRemoving) FS_THROW_FSEXCEPTION;
			bool		RemoveIfNewer(BEntry &source_entry, BDirectory &target_dir, const char *target_name) FS_THROW_FSEXCEPTION;
			bool		RemoveIfNewer(BEntry &source_entry, BEntry &target_entry) FS_THROW_FSEXCEPTION;

			void		MoveToRecursive(EntryIterator &i, BDirectory &target_dir, bool first_run = false) FS_THROW_FSEXCEPTION;

			bool		IsTargetOnSameDevice() FS_NOTHROW;
			void		SetTargetVolume(BDirectory &) FS_THROW_FSEXCEPTION;
			void		SetTargetVolume(dev_t) FS_THROW_FSEXCEPTION;
public:		dev_t		TargetDevice() const FS_NOTHROW				{ return mTargetVolume.Device(); }
			dev_t		SourceDevice() const FS_NOTHROW				{ return mSourceDevice; }
			bool		IsDeviceInvolved(dev_t dev) const FS_NOTHROW {
							return (mSourceDevice == dev  ||  mTargetVolume.Device() == dev);
						}
protected:	void		CheckFreeSpaceOnTarget(off_t size, interaction icode) FS_THROW_FSEXCEPTION;

			void		MoveToTrash(dev_t device, BEntry &entry, BDirectory &trash_dir) FS_THROW_FSEXCEPTION;
			void		MoveToTrash(dev_t device, BEntry &entry) FS_THROW_FSEXCEPTION {
							BDirectory trash_dir;
							MoveToTrash(device, entry, trash_dir);
						}
			void		RestoreFromTrash(BEntry &entry) FS_THROW_FSEXCEPTION;
			void		MakeSpaceFromTrash(off_t size) FS_NOTHROW;

			bool		CreateLink(BEntry &source_entry, BDirectory &target_dir, char *target_name, bool relative) FS_THROW_FSEXCEPTION;
			void		CreateDirectory(BPath &path, bool is_dir) FS_THROW_FSEXCEPTION;
			void		CheckTargetDirectory(BEntry &entry, BDirectory &target_dir) FS_THROW_FSEXCEPTION {
							CheckTargetDirectory(entry, target_dir, entry.IsDirectory());
						}
			void		CheckTargetDirectory(BEntry &entry, BDirectory &target_dir, bool check_source) FS_THROW_FSEXCEPTION;

// Progress indication
public:
	mutable ProgressInfo mProgressInfo;
	
protected:	
	OperationStack		mOperationStack;
	node_ref			mRootSourceDirNodeRef;		// this is needed to smartly copy relative links
	node_ref			mRootTargetDirNodeRef;		// this is the target dir given in the public API call, e.g. in CopyTo() or MoveTo()... this is required when an operation with a broblematic entry is retried
	node_ref			mSourceDirNodeRef;			// this is to speed up SourceDirSetter, to skip unneded mSourceDir.SetTo()'s
	BDirectory			mSourceDir;					// it always points to the source dir of the actual entry
	
private:
	static	int32						sInitialized;
//	static	void							sInitialize();	delete me 
	static	uint64						sMaxMemorySize;
	static	BLocker						sLocker;
	static	node_ref_list_t				sTrashDirList;
	static	node_ref_list_t				sDesktopDirList;
	static	node_ref_list_t				sPrintersDirList;
	static	node_ref_list_t				sHomeDirList;
	static	int32						sEmptyTrashRunning;
	static	command					sLastSelectedInteractionAnswers[kTotalInteractions]; // initialized to all kInvalidCommand

	bigtime_t			mShrinkBufferSuggestionTime;
	uint8 *				mBuffer;
	size_t				mMaxBufferSize;
	size_t				mBufferSize;
	thread_id			mMainThreadID;

	const EntryRef *		mCurrentEntry;
	mode_t				mCurrentEntryType;			// we do not inspect an entry twice, once in the operation and once in SetCurrentEntry(), instead the operation calles SetCurrentEntryType() when it has the information
	
	command			mDefaultInteractionAnswers[kTotalInteractions]; // initialized to all kInvalidCommand
	answer_flags		mPossibleAnswers;				// flags of currenlty possible answers
	operation			mSkipOperationTargetOperation;	// this operation will be skipped by a thrown kSkipOperation
	BVolume			mTargetVolume;
	dev_t				mSourceDevice;
	
	// these are for the multithreaded copy loop
	command			mThrowThisInMainThread;
	bool					mWriterThreadRunning;
	thread_id			mWriterThreadID;
	ChunkList *			mChunkList;
	bool					mWasMultithreaded;

#if FS_MONITOR_THREAD_WAITINGS
	BStopWatch		mWriterThreadWaiting;
	BStopWatch		mReaderThreadWaiting;
#endif

	bool				mCopyLinksInstead;
	bool				mDirectoryCreatedByUs;			// this varibale tells CopyFile if it should finish the current file in case of a kSkipDirectory (if the dir was not created by us rather we are sweeping togather two dirs then CopyFile should finish the copy)
	command		mThrowThisAfterFileCopyFinished;// this flag stores the information that we delayed a throw due to the above comment
	bool				mFileCreatedByUs;				// this varibale tells CopyFile if it should finish the current file in case of a kSkipDirectory (if the file was created by us then remove the half-done part and skip dir)
	copy_flags		mCopyFlags;						// these are the config flags that tell which additional file infos should be copied

	char *			mSourceNewName;					// these are optional infos for Interaction()
	char *			mTargetNewName;					// they should be zero when not available
	BEntry *			mTargetEntry;

	bool				mBufferReleasable;
	bool				mSameDevice;					// source and target on the same device

	ONLY_WITH_TRACKER(
		bool			mSkipTrackerPoseInfoAttibute;	// do not copy pose info (force autoplacement)
	);
	
	Stack<error_answer, kMaxDefaultErrorAnswers>	mDefaultErrorAnswers;
};


template <typename T>
struct backup_struct {
	T	*mPointer;
	T	mOldValue;
	backup_struct(T *ptr) :	mPointer(ptr), mOldValue(*ptr) { }
	~backup_struct() {
		*mPointer = mOldValue;
	}
};
template <>
struct backup_struct<BDirectory> {
	node_ref	mOldRef;
	BDirectory	*mPointer;
	
	backup_struct(BDirectory *ptr) : mPointer(ptr) {
		ptr -> GetNodeRef(&mOldRef);
	}
	~backup_struct() {
		if (mPointer -> SetTo(&mOldRef) != B_OK) {
			beep();
			FS_CONTROL_THROW(FSContext::kCancel);
		}
	}
};


class EntryIterator : noncopyable {
public:
	class Filter {
	public:
		Filter() FS_NOTHROW									{ }
		Filter(const Filter &other) FS_NOTHROW				{ SetTo(other); }
		// XXX implement		
		void	SetTo(const Filter &)						{ }
		bool	IsFilteredOut(const entry_ref &) const FS_NOTHROW { return false; }
	};

			EntryIterator() FS_NOTHROW : mFilter(0), mOwningFilter(false) { }
	virtual	~EntryIterator() {
				if (mOwningFilter)
					delete mFilter;
			}
	
	virtual	bool			GetNext(EntryRef &)	FS_NOTHROW			= 0; // return false if none left
	virtual	void			Rewind()			FS_NOTHROW			= 0;
	virtual	int32			CountEntries()		FS_NOTHROW			= 0; // may not be called while in an iteration
	virtual	EntryIterator *	Clone()				FS_NOTHROW			= 0; // create an exact copy of this iterator with the pointer, too
			void			SetFilter(const Filter *filter) FS_NOTHROW { // takes ownership
								mFilter = filter;
								mOwningFilter = true;
							}
			void			RegisterNested(EntryIterator &other) FS_NOTHROW { // this is called when a subiterator is created to link additional info like entry filters...
								other.mFilter = mFilter;
							}
			bool			IsFilteredOut(const entry_ref &ref) const FS_NOTHROW {
								if (mFilter == 0) return false;
								return mFilter -> IsFilteredOut(ref);
							}
private:
	const Filter	*mFilter;
	bool			mOwningFilter;
};


class EntryList : public vector<EntryRef> {
	typedef vector<EntryRef> inherited;

	class STLEntryIterator : public EntryIterator {
		typedef inherited				cont_type;
		typedef cont_type::iterator		iter_type;
		
		cont_type &		mContainer;
		iter_type		mPos;
	
	public:
		STLEntryIterator(cont_type &) FS_NOTHROW;
		STLEntryIterator(const STLEntryIterator &other) FS_NOTHROW;
	
		int32 CountEntries() FS_NOTHROW;
	
		bool			GetNext(EntryRef &entry) FS_NOTHROW;
		EntryIterator *	Clone() FS_NOTHROW;
		void			Rewind() FS_NOTHROW;
		void			RegisterNested(const EntryIterator *) FS_NOTHROW { }
	};

public:
	ONLY_WITH_TRACKER(
		EntryList(const PoseList &) FS_NOTHROW;
	);

	EntryList() FS_NOTHROW {}
	EntryList(BObjectList<entry_ref> *) FS_NOTHROW;
	EntryList(const BMessage &) FS_NOTHROW;
	EntryList(const entry_ref &in_ref) FS_NOTHROW {
		push_back(EntryRef(in_ref));
	}

	EntryIterator *	NewIterator()	{ return new STLEntryIterator(*this); }
};


class SingleEntryIterator : public EntryIterator {
	entry_ref	mRef;
	bool		mAlreadyRead;
	
public:
	SingleEntryIterator() FS_NOTHROW { }

	SingleEntryIterator(const SingleEntryIterator &other) FS_NOTHROW : EntryIterator(), mAlreadyRead(false) {
		mRef = other.mRef;
	}

	SingleEntryIterator(BEntry &inentry) FS_NOTHROW : mAlreadyRead(false) {
		inentry.GetRef(&mRef);
	}
	
	status_t	SetTo(BEntry &inentry) FS_NOTHROW	{ mAlreadyRead = false; return inentry.GetRef(&mRef); }
	status_t	SetTo(entry_ref &inref) FS_NOTHROW	{
					mAlreadyRead = false;
					mRef.device = inref.device;
					mRef.directory = inref.directory;
					return mRef.set_name(inref.name);
				}
	
	int32		CountEntries() FS_NOTHROW			{ return 1; }

	bool		GetNext(EntryRef &entry) FS_NOTHROW {
					if (mAlreadyRead == false) {
						mAlreadyRead = true;
						if (IsFilteredOut(mRef) == true)
							return false;
							
						entry.SetTo(mRef);
						return true;
					}
					return false;
				}
	void		Rewind() FS_NOTHROW				{ mAlreadyRead = false; }
EntryIterator *	Clone() FS_NOTHROW				{ return new SingleEntryIterator(*this); }
};


class DirEntryIterator : public EntryIterator {
	typedef EntryIterator inherited;
	BDirectory		mDir;
	
public:

	DirEntryIterator() {}
	
	DirEntryIterator(const DirEntryIterator &other) : inherited() {
		SetTo(other.mDir);
	}
	DirEntryIterator &operator=(const DirEntryIterator &other) {
		SetTo(other.mDir);
		return *this;
	}

	status_t SetTo(const BDirectory &dir) FS_NOTHROW {
//		return mDir.SetTo(&dir);	missing form the Be API ?!
		BEntry entry;
		dir.GetEntry(&entry);
		return mDir.SetTo(&entry);
	}

	status_t SetTo(const BDirectory &dir, const char *name) FS_NOTHROW {
		return mDir.SetTo(&dir, name);
	}

	status_t SetTo(BEntry &entry) FS_NOTHROW {
		return mDir.SetTo(&entry);
	}

	int32 CountEntries() FS_NOTHROW			{ return mDir.CountEntries(); }

	bool GetNext(EntryRef &entry) {
		entry_ref ref;
		
		for (;;) {
			if (mDir.GetNextRef(&ref) != B_OK)
				return false;
				
			if (IsFilteredOut(ref))
				continue;
				
			entry.SetTo(ref);
			return true;
		}
	}
	
	EntryIterator *Clone() {
		return new DirEntryIterator(*this);
	}
	
	void Rewind() {
		mDir.Rewind();
	}
};


}	// namespace fs


#endif // _FSCONTEXT_H














