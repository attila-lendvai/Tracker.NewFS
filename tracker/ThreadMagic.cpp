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

#include "Defines.h"
#include <Autolock.h>
#include <algorithm>
#include <Debug.h>

#include <memory>

#include "ThreadMagic.h"

namespace TM_NAMESPACE {

using namespace std;

int32 Thread :: sTLSInitialized = 0;
int32 Thread :: sThreadObject_tls;

#if TM_NEED_THREADPOOL

ThreadPool ThreadPool :: sInstance(2, 10, B_NORMAL_PRIORITY, true, 10*1000000);

ThreadPool :: ThreadPool(int iMin, int iMax, int32 iPriority, bool iLazy, bigtime_t iTimeOut) :
								mTimeOut(iTimeOut),
								mPriority(iPriority),
								mMin(iMin),
								mMax(iMax),
								mCurrent(0),
								mIdle(0),
								mLocker("ThreadPool locker") {
//	BAutolock l(Locker());

	mWorkerSem = create_sem(0, "ThreadPool :: mWorkerSem");
	if ( mWorkerSem < 0 )
		throw ThreadException("Failed to create semaphore for ThreadPool");
	
	if ( ! iLazy ) {
		mTimeOut = B_INFINITE_TIMEOUT;
		for (int i = 0;  i < iMax;  i++)
			SpawnAThread();
			
		mIdle	 = iMax;
		mCurrent = iMax;
	}
}

ThreadPool :: ~ThreadPool() TM_NOTHROW {
	delete_sem(mWorkerSem);	// this will tell all the threads to quit

	for (int i = 0;  i < 10;  i++) {	// wait at most 10 secs if threads are working
		if ( mThreads.size() > 0 )
			snooze(100000);
		else
			break;
	}
}

void ThreadPool::SpawnAThread() TM_NOTHROW {
	ASSERT_WITH_MESSAGE(mLocker.IsLocked() == true, "ThreadPool must be locked before SpawnAThread()");
	thread_id id = spawn_thread(& ThreadPool :: sThreadFunc, "ThreadPool (waiting)", B_NORMAL_PRIORITY, this);
	if ( id < 0 )
		DEBUGGER("Failed to spawn thread for ThreadPool");
	else {
		resume_thread( id );
		mThreads.push_back( id );
	}
}

void ThreadPool::Do(functor_list_t &list, int32 prio, bool async) {
	functor_list_t::iterator pos = list.begin(), end = list.end();
	
	if (async) {
		BAutolock l(sInstance.Locker());
		
		for (; pos < end; ++pos)
			sInstance.mTasks.push_back(function_info(*pos, prio, "TaskSequence item"));

	} else {
		for (; pos < end; ++pos) {
			(*(*pos))();
			delete *pos;
		}
	}
}

void ThreadPool :: Do(FunctionObject *iFunctor, const char *iName, int32 iPriority) {
	BAutolock l(Locker());
	
	mTasks.push_back( function_info(iFunctor, iPriority, iName) );
	release_sem_etc( mWorkerSem, 1, B_DO_NOT_RESCHEDULE );
	
	if ( mIdle <= 0  &&  mCurrent < mMax ) {
		SpawnAThread();
		atomic_add( &mIdle, 1 );
		mCurrent++;
	}
}


void ThreadPool :: ThreadFunc() TM_NOTHROW {
	bigtime_t	timeout = mTimeOut;
	for (;;) {
		status_t	rc;
		thread_id	thread = find_thread(0);
		while ( (rc = acquire_sem_etc(mWorkerSem, 1, B_RELATIVE_TIMEOUT, timeout)) == B_INTERRUPTED )
			;
		
		if ( rc == B_TIMED_OUT ) {
			if ( mCurrent > mMin ) {
				{
					BAutolock l(Locker());
					mThreads.erase( find( mThreads.begin(), mThreads.end(), thread ) );
					mCurrent--;
					atomic_add(&mIdle, -1);
				}
				exit_thread( B_OK );
			} else {
				timeout = B_INFINITE_TIMEOUT;
				continue;
			}
		} else if ( rc == B_BAD_SEM_ID ) {
			exit_thread( B_OK );	// the threadpool is deleted
		}
		
		atomic_add(&mIdle, -1);
									
		function_info info;
		{
			BAutolock l(Locker());
			ASSERT(mTasks.size() > 0);
												
			info = mTasks.front();
			mTasks.pop_front();
		}
		if (info.mPriority != static_cast<int32>(0xdeedbeef))
			set_thread_priority( thread, info.mPriority);
		{
			char buf[512];
			sprintf(buf, "ThreadPool (%s)", (info.mName) ? info.mName : "unnamed");
			rename_thread(thread, buf);
		}
		(*info.mFunctor)();			// do the job
		set_thread_priority(thread, mPriority);
		delete info.mFunctor;
												
		rename_thread(thread, "ThreadPool (waiting)");
		atomic_add(&mIdle, 1);
//		timeout = mTimeOut;		// XXX needed ???
	}
}

status_t ThreadPool :: sThreadFunc(void *iThis) TM_NOTHROW {
	(static_cast<ThreadPool *>(iThis)) -> ThreadFunc();
	return B_OK;
}


/*	!!! UNTESTED code, it just compiles... I don't need TimedThreadPool, but I had a few minutes to write it.
	!!! TEST HEAVILY before use (and please mail me your fixes to be able to redistribute them to the others)
//
//	TimedThreadPool
//
void TimedThreadPool :: Do(FunctionObject *iFunctor, int32 iPriority) {
	BAutolock l(Locker());
	
	mTasks.push_back( function_info(iFunctor, iPriority, 0) );
	release_sem_etc( mWorkerSem, 1, B_DO_NOT_RESCHEDULE );
	
	if ( mIdle <= 0  &&  mCurrent < mMax ) {
		thread_id id = spawn_thread(& TimedThreadPool :: sThreadFunc, "TimedThreadPool :: ThreadFunc", B_NORMAL_PRIORITY, this);
		if ( id < 0 )
			throw ThreadException("Failed to spawn thread for TimedThreadPool");
		resume_thread( id );
		mThreads.push_back( id );
		atomic_add( &mIdle, 1 );
		mCurrent++;
	}
}

status_t TimedThreadPool :: sThreadFunc(void *iThis) TM_NOTHROW {
	(static_cast<TimedThreadPool *>(iThis)) -> ThreadFunc();
	return B_OK;
}

void TimedThreadPool :: ThreadFunc() TM_NOTHROW {
	bigtime_t	timeout = mTimeOut;
	for (;;) {
		status_t	rc;
		bigtime_t when;
		do {		// we must recalculate the time if interrupted
			{
				BAutolock l(Locker());
				
				when = (mTasks.front().mWhen) ?
								mTasks.front().mWhen :
								when = system_time() + timeout;
			}										
		} while ( (rc = acquire_sem_etc(mWorkerSem, 1, B_ABSOLUTE_TIMEOUT, when)) == B_INTERRUPTED );
		
		if ( rc == B_TIMED_OUT  ||  rc == B_OK) {
			atomic_add(&mIdle, -1);
										
			function_info info;
			{
				BAutolock l(Locker());
				if (mTasks.size() <= 0) {
					if ( mCurrent > mMin ) {
						mThreads.erase( find( mThreads.begin(), mThreads.end(), find_thread(0) ) );
						mCurrent--;
						atomic_add(&mIdle, -1);
						mLocker.Unlock();	// ~BAutolock will not be called becaouse ot exit_thread()
						exit_thread( B_OK );
					} else {
						timeout = kHugeTimeout;
						continue;
					}
				}
													
				info = mTasks.front();
				mTasks.pop_front();
			}
			if (info.mPriority != static_cast<int32>(0xdeedbeef))
				set_thread_priority( find_thread(0), info.mPriority);
			(*info.mFunctor)();			// do the job
			set_thread_priority(find_thread(0), mPriority);
			delete info.mFunctor;
													
			atomic_add(&mIdle, 1);
	//		timeout = mTimeOut;		// XXX needed ???
		} else if ( rc == B_BAD_SEM_ID ) {
			exit_thread( B_OK );	// the threadpool is deleted
		}
	}
}
*/

#endif // TM_NEED_THREADPOOL



//
//	ThreadPrimitive
//
ThreadPrimitive :: ThreadPrimitive(const char *inName, int32 inPriority) TM_NOTHROW
													  : fName(inName),
													  	fThreadID(-1),
													    fPriority(inPriority) {
}
							
								
ThreadPrimitive :: ~ThreadPrimitive() TM_NOTHROW {
	if (fThreadID > 0) {
		kill_thread(fThreadID);
//		ASSERT(!"should not be here");
	}
}
							
								
thread_id ThreadPrimitive :: Go() {
	thread_id id;
	if ((id = spawn_thread(&ThreadPrimitive::RunBinder, fName ? fName : "Unnamed", fPriority, this)) >= B_OK) {
		fThreadID = id;
		resume_thread(id);		// *?*?*? should it be here by default ?
								// might cause race condition with fast-exiting threads
	} else {
		throw ThreadException("Failed to spawn thread in ThreadPrimitive");
	}
	return id;	// Should not access this... thread might be gone.
}

status_t ThreadPrimitive :: RunBinder(void *inThisPtr) TM_NOTHROW {
	ThreadPrimitive *instance = (ThreadPrimitive *)inThisPtr;
	instance -> Run();
	return B_OK;
}

thread_state ThreadPrimitive :: State() TM_NOTHROW {
	thread_info ti;
	status_t rc;
	if ((rc = GetThreadInfo(&ti)) != B_OK)
		return (thread_state)rc;
	
	return ti.state;
}

bool ThreadPrimitive :: WaitForThreadWithTimeout(bigtime_t inTimeout) TM_NOTHROW {

const bigtime_t delay = 100000;

	for (int i = 0;  ; i++) {
		if (State() < 0)
			return true;
		
		if (i * delay > inTimeout)
			return false;
		
		snooze(delay);
	}
}

//
//	old MouseDownThread
//

#if 0
//#if TM_NEED_MOUSE_DOWN_THREAD

MouseDownThread :: MouseDownThread(BView *inView, bool (*inPressingFunc)(BView *, BPoint &, uint32),
					void (*inDonePressingFunc)(BView *, BPoint &), bigtime_t inDelay) TM_NOTHROW
							: ThreadPrimitive("MouseTracker", B_NORMAL_PRIORITY),
							  fDelay(inDelay),
							  fView(inView),
							  fDonePressingFunc(inDonePressingFunc),
							  fPressingFunc(inPressingFunc) {
}

MouseDownThread	*MouseDownThread :: Launch(BView *inView, bool (*inPressingFunc)(BView *, BPoint &, uint32),
						void (*inDonePressingFunc)(BView *, BPoint &), bigtime_t inDelay) {

	MouseDownThread *thread = new MouseDownThread(inView, inPressingFunc, inDonePressingFunc, inDelay);
	if (thread -> Go() < B_OK) {
		delete thread;
		throw ThreadException("Failed to spawn thread in a MouseDownThread");
	}
	return thread;
}

void MouseDownThread :: Run() TM_NOTHROW {
	uint32 buttons;
	BPoint location;
	
	bool doit = true;
	while (doit) {
		if ( ! fView -> LockLooper())
			break;
							
		fView -> GetMouse(&location, &buttons, false);
		if (buttons == 0) {
			if (fDonePressingFunc)
				(*fDonePressingFunc)(fView, location);
			fView -> UnlockLooper();
			break;
		}

		if (fPressingFunc)
			doit = (*fPressingFunc)(fView, location, buttons);
			
		fView -> UnlockLooper();
		if (fDelay)
			snooze(fDelay);
	}
				
	delete this;
	return B_OK;
}

#endif // TM_NEED_MOUSE_DOWN_THREAD


//
// Thread
//
Thread :: Thread(FunctionObject *inFunctor, const char *inName, int32 inPriority, int32 inFlags) TM_NOTHROW
					: ThreadPrimitive(inName, inPriority),
					  fFunctor(inFunctor), fQuitRequested(false),
					  fDeleteSelf(inFlags & kDeleteSelf),
					  fRunReturned(false) {
					  
	if (atomic_or(&sTLSInitialized, 1) == 0)
		sThreadObject_tls = tls_allocate();
		
}

Thread :: ~Thread() TM_NOTHROW {
	ASSERT_WITH_MESSAGE(fRunReturned == true, "Thread :: dtor was called while Run() didn't return. "
									"You should find a way to exit the thread instead of killing it. (delete)");
	delete fFunctor;
}

Thread *Thread :: Launch(FunctionObject *inFunctor,
						const char *inName, int32 inPriority, int32 inFlags) {
															
	auto_ptr<Thread> thread(new Thread(inFunctor, (inName) ? inName : "Some unnamed Thread", inPriority, inFlags));
	
	thread -> Go();
	
	return thread.release();
}

bool Thread :: RequestToQuit(bigtime_t inTimeOut, bool inKillAfterTimeout) TM_NOTHROW {

	fQuitRequested = true;
	
	if (WaitForThreadWithTimeout(inTimeOut) == false) {
		if ( ! inKillAfterTimeout)
			return false;
		Kill();
		if (fDeleteSelf)
			delete this;
	}
	return true;
}

void Thread :: Run() TM_NOTHROW {
	// put the Thread object in the TLS
	tls_set(sThreadObject_tls, this);
	
	(*fFunctor)();
	fRunReturned = true;
	
	if (fDeleteSelf)
		delete this;
	
	return;
}


//
// PeriodicThread
//
PeriodicThread :: PeriodicThread(FunctionObjectWithResult<bool> *inFunctor,
						bigtime_t inDelay, const char *inName, int32 inPriority, int32 inFlags) TM_NOTHROW
					: Inherited(inFunctor, inName, inPriority, inFlags),
					  fDelay(inDelay) {
}

// This will not wait until the thread exists... to kill the thread call Kill()
//PeriodicThread :: ~PeriodicThread() TM_NOTHROW {
//	if (fRunReturned == false) {
//		debugger("PeriodicThread :: dtor was called while the thread was still running !");
//	}
//}


bool PeriodicThread :: RequestToQuit(bigtime_t inTimeOut, bool inKillAfterTimeout) TM_NOTHROW {
	fQuitRequested = true;
	
	for (int i = 0;  ;  i++) {
		SendSignal(SIGCONT);			// ?*?*?* Be careful... comment out if don't need it !
		if (i * fDelay > inTimeOut) {
			if (inKillAfterTimeout)
				Kill();
			break;
			// ?*?*?*? notify thread user somehow
		}
		
		if (fDeleteSelf) {
			if (State() < 0)	// Because of race conditions we cannot use
				break;			// fReadyToQuit
		} else {
			if (fRunReturned)
				break;
		}

		snooze(fDelay >> 1);
	}

	return (fDeleteSelf) ? true : fRunReturned;
}

PeriodicThread *PeriodicThread :: Launch(FunctionObjectWithResult<bool> *inFunctor,
						bigtime_t inDelay, const char *inName, int32 inPriority, int32 inFlags) {

	auto_ptr<PeriodicThread> thread(new PeriodicThread(inFunctor, inDelay, (inName) ? inName : "Some unnamed PeriodicThread", inPriority, inFlags));
	
	thread -> Go();

	return thread.release();
}

void PeriodicThread :: Run() TM_NOTHROW {
	for (;;) {
		if (fQuitRequested)
			break;
			
		(*fFunctor)();
		
		if (fFunctor -> Result() != true)
			break;

		if (fDelay)
			snooze(fDelay);
	}
	
	fRunReturned = true;
	if (fDeleteSelf)
		delete this;
}


void
ExecuteThreadSequence(functor_list_t &list) {
	functor_list_t::iterator pos = list.begin(), end = list.end();
	
	while (pos < end) {
//		if ((**pos)() != B_OK)	// if any of the functions return some error cancel the sequence
//			return;
		
		(**pos)();

		delete *pos;
		++pos;
	}
}

} // namespace threadmagic
