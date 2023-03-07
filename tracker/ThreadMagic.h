#if !defined(_THREADMAGIC_H)
#define _THREADMAGIC_H

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

//	Author e-mail:	101@inf.bme.hu

//#define TM_NEED_THREADPOOL			1			// #undef the one you don't need
#define TM_NEED_MOUSE_DOWN_THREAD		1
//#define TM_THREADPOOL_OPT				1			// ThreadPool behind LaunchInNewThread

#if TM_THREADPOOL_OPT
  #define TM_NEED_THREADPOOL			1			// #undef the one you don't need
#endif

#include <OS.h>
#include <signal.h>
#include <Debug.h>
#include <View.h>
#include <Locker.h>
#include <TLS.h>

#include <vector>
#include <deque>
#include <functional>
#include <algorithm>
#include <exception>

#if TM_NEED_MOUSE_DOWN_THREAD
  #include "Utilities.h"		// XXX Tracker specific
#endif

#include "FunctionObject.h"

#define TM_NOTHROW
//#define TM_NOTHROW	throw()

#if _BUILDING_tracker
  #define TM_NAMESPACE BPrivate
#else
  #define TM_NAMESPACE threadmagic
#endif

namespace TM_NAMESPACE {

using namespace boost;

class ThreadException : public exception {
	const char *mWhat;
public:
	ThreadException(const char *inWhat) : mWhat(inWhat) {}
	const char *what() const { return mWhat; }
};

typedef vector<FunctionObject *> functor_list_t;

#if TM_NEED_THREADPOOL

class ThreadPool {
	struct function_info {
		FunctionObject	*mFunctor;
		int32			mPriority;
		const char		*mName;
		
		function_info() { }
		function_info(FunctionObject *iFunctor, int32 iPri, const char *iName) :
					mFunctor(iFunctor), mPriority(iPri), mName(iName) { }
	};

	typedef vector<thread_id>						threadlist_t;
	typedef deque<function_info>					tasklist_t;		// XXX * to avoid a plus instantiation?
																			
	ThreadPool &		operator=(ThreadPool &);
						ThreadPool(ThreadPool &);
	static	status_t	sThreadFunc(void *)		TM_NOTHROW;
			void		ThreadFunc()			TM_NOTHROW;
			void		SpawnAThread()			TM_NOTHROW;
			
protected:
	bigtime_t		mTimeOut;		// The time after an idle thread exits
	int32			mPriority;		// Default priority for the pool
	vint			mMin;			// Min number of threads
	vint			mMax;			// Max number of threads
	vint			mCurrent;		// Current number of threads
	int32			mIdle;			// Current idle threads
	sem_id			mWorkerSem;
	BLocker			mLocker;
	threadlist_t	mThreads;
	tasklist_t		mTasks;
															
public:												
						// Min/max threads; default priority; lazy: instantiate threads in ctor; timeout: when unneded idle threads should quit
						ThreadPool(int min, int max, int32 prio = B_NORMAL_PRIORITY, bool lazy_fill = true, bigtime_t timeout = 5000000);
	virtual				~ThreadPool() TM_NOTHROW;
											
			// Not specifying any priority means the default priority defined in the ctor
			void		Do(FunctionObject *, const char *name = 0, int32 prio = 0xdeedbeef);
	static	void		Do(functor_list_t &list, int32 priority = B_NORMAL_PRIORITY, bool async = true);											
				
			// For debug purposes only
			int			MaxThreads()	TM_NOTHROW	{ return mMax; }
			int			MinThreads()	TM_NOTHROW	{ return mMin; }
			int			CurrentThreads() TM_NOTHROW	{ return mCurrent; }
			int			IdleThreads()	TM_NOTHROW	{ return mIdle; }
			int			EnqueuedTasks()	TM_NOTHROW	{ return mTasks.size(); }
															
			BLocker *	Locker()	TM_NOTHROW		{ return &mLocker; }
			bool		Lock()		TM_NOTHROW		{ return mLocker.Lock(); }
			void		Unlock()	TM_NOTHROW		{ mLocker.Unlock(); }

	static	ThreadPool	sInstance;
	
};

#endif	// TM_NEED_THREADPOOL

/*	!!! UNTESTED code, see .cpp for comments
class TimedThreadPool : public ThreadPool {
	struct function_info {
		FOBC_Thread		*mFunctor;
		int32			mPriority;
		bigtime_t		mWhen;
		
		function_info() {}
		function_info(FOBC_Thread *inFunctor, int32 inPri, bigtime_t inWhen) :
					mFunctor(inFunctor), mPriority(inPri), mWhen(inWhen) {}
//		function_info &operator=(const function_info &other) {	// default is ok
//			mFunctor		= other.mFunctor;
//			mPriority 		= other.mPriority;
//			mWhen			= other.mWhen;
//		}
	};
	typedef deque<function_info>				tasklist_t;
						
	tasklist_t			mTasks;
															
	static		status_t	sThreadFunc(void *) TM_NOTHROW;
			void			ThreadFunc() TM_NOTHROW;
public:
						TimedThreadPool(int min, int max, int32 prio = B_NORMAL_PRIORITY, bool lazy_fill = true, bigtime_t timeout = 5000000) :
								ThreadPool(min, max, prio, lazy_fill, timeout) {}
								
			void			Do		(FOBC_Thread *,						int32 prio = 0xdeedbeef);
			void			DoAfter	(FOBC_Thread *, bigtime_t delay,	int32 prio = 0xdeedbeef);
			void			DoAt	(FOBC_Thread *, bigtime_t time,		int32 prio = 0xdeedbeef);
};
*/

/***************************************************************
	Threads
****************************************************************/

//	ThreadPrimitive
//
//	NOTE:	The return code, which is returned by the virtual Run() method, is the
//			return code of the exit_thread().
class ThreadPrimitive {
	const char			*fName;
													
	static	status_t	RunBinder(void *) TM_NOTHROW;
								
protected:
	thread_id			fThreadID;
	int32				fPriority;
									
public:
							ThreadPrimitive(const char * = NULL, int32 = B_NORMAL_PRIORITY) TM_NOTHROW;
	virtual					~ThreadPrimitive() TM_NOTHROW;
	virtual	void			Run() = 0;
	// Use this one with care ! Instead of Kill() use RequestToQuit() or similar
	// in derived classes !
	virtual	void				Kill() TM_NOTHROW					{ kill_thread(fThreadID); }
															
			thread_id			Go();
			thread_state		State() TM_NOTHROW;
																
	inline		void			Resume() TM_NOTHROW				{ resume_thread(fThreadID); }
	inline		void			Suspend() TM_NOTHROW				{ suspend_thread(fThreadID); }
	inline		thread_id		ThreadID() TM_NOTHROW				{ return fThreadID; }
	inline		int				SendSignal(uint inSignal) TM_NOTHROW{ return send_signal(fThreadID, inSignal); }
	inline		status_t		GetThreadInfo(thread_info *inInfo) TM_NOTHROW { return get_thread_info(fThreadID, inInfo); }
				bool			WaitForThreadWithTimeout(bigtime_t) TM_NOTHROW;
	inline		status_t		WaitForThread(status_t * inRCContainer = 0) TM_NOTHROW {
									status_t dummy;
									return wait_for_thread(fThreadID, (inRCContainer) ? inRCContainer : &dummy);
								}
};


/*******************************************************************************
	Thread
			If you want the thread to automatically delete itself when it's
			done (like a FireAndForgetThread) then set the DELETE_SELF flag in
			Launch();

*******************************************************************************/

class Thread : public ThreadPrimitive {
													
	typedef ThreadPrimitive Inherited;
												
	FunctionObject			*fFunctor;

protected:
	bool					fQuitRequested;
	bool					fDeleteSelf;
	bool					fRunReturned;
	static int32			sTLSInitialized;
	static int32			sThreadObject_tls;

							Thread(FunctionObject *, const char *, int32, int32) TM_NOTHROW;
			void			Run() TM_NOTHROW;
																					
public:
	static		Thread		*Launch(FunctionObject *, const char * = 0, int32 = B_NORMAL_PRIORITY, int32 = kDeleteSelf);
	static		Thread		*ThisThread()					{ return (Thread *)tls_get(sThreadObject_tls); }
	
							~Thread() TM_NOTHROW;

			bool			RequestToQuit(bigtime_t = 0, bool = false) TM_NOTHROW;
			void			RequestToQuitAsync() TM_NOTHROW		{ fQuitRequested = true; }
			bool			IsQuitRequested() TM_NOTHROW		{ return fQuitRequested; }
			void			Kill() TM_NOTHROW					{ fRunReturned = true; Inherited :: Kill(); }
					
	// Flags
	enum {
		kDeleteSelf = 1
	};
};

//
//	PeriodicThread
//
//	NOTE:	The method, which is packed in the functor object, should return B_OK
//			as long as it wants to run. The first time it returns sg other then B_OK
//			the thread will exit with that return code.
class PeriodicThread : public Thread {

	typedef Thread Inherited;

protected:
	bigtime_t						fDelay;
	FunctionObjectWithResult<bool>	*fFunctor;
//	bool				fReadyToQuit;	// use instead fRunReturned in Thread
														
							PeriodicThread(FunctionObjectWithResult<bool> *, bigtime_t, const char *, int32, int32) TM_NOTHROW;
			void			Run() TM_NOTHROW;
																					
public:
	static PeriodicThread 	*Launch(FunctionObjectWithResult<bool> *, bigtime_t, const char * = 0, int32 = B_NORMAL_PRIORITY, int32 = 0);
//							~PeriodicThread() TM_NOTHROW;
			bool			RequestToQuit(bigtime_t, bool = false) TM_NOTHROW;
			bool			IsReadyToQuit() TM_NOTHROW		{ return fRunReturned; }
							
//private:
	// Kill should be private ! Use with care, it calls kill_thread() !
};


#if TM_NEED_MOUSE_DOWN_THREAD

template<class View>
class MouseDownThread {
public:
	static void TrackMouse(View *view, void (View::*)(BPoint),
		void (View::*)(BPoint, uint32) = 0, bigtime_t pressingPeriod = 100000);

protected:
	MouseDownThread(View *view, void (View::*)(BPoint),
		void (View::*)(BPoint, uint32), bigtime_t pressingPeriod);

	virtual ~MouseDownThread();
	
	void Go();
	virtual void Track();
	
	static status_t TrackBinder(void *);
private:
	
	BMessenger fOwner;
	void (View::*fDonePressing)(BPoint);
	void (View::*fPressing)(BPoint, uint32);
	bigtime_t fPressingPeriod;
	volatile thread_id fThreadID;
};


template<class View>
void
MouseDownThread<View>::TrackMouse(View *view,
	void(View::*donePressing)(BPoint),
	void(View::*pressing)(BPoint, uint32), bigtime_t pressingPeriod)
{
	(new MouseDownThread(view, donePressing, pressing, pressingPeriod))->Go();
}


template<class View>
MouseDownThread<View>::MouseDownThread(View *view,
	void (View::*donePressing)(BPoint),
	void (View::*pressing)(BPoint, uint32), bigtime_t pressingPeriod)
	:	fOwner(view, view->Window()),
		fDonePressing(donePressing),
		fPressing(pressing),
		fPressingPeriod(pressingPeriod)
{
}


template<class View>
MouseDownThread<View>::~MouseDownThread()
{
	if (fThreadID > 0) {
		kill_thread(fThreadID);
		// dead at this point
		TRESPASS();
	}
}


template<class View>
void 
MouseDownThread<View>::Go()
{
	fThreadID = spawn_thread(&MouseDownThread::TrackBinder, "MouseTrackingThread",
		B_NORMAL_PRIORITY, this);
	
	if (fThreadID <= 0 || resume_thread(fThreadID) != B_OK)
		// didn't start, don't leak self
		delete this;
}

template<class View>
status_t 
MouseDownThread<View>::TrackBinder(void *castToThis)
{
	MouseDownThread *self = static_cast<MouseDownThread *>(castToThis);
	self->Track();
	// dead at this point
	TRESPASS();
	return B_OK;
}

template<class View>
void 
MouseDownThread<View>::Track()
{
	for (;;) {
		MessengerAutoLocker lock(&fOwner);
		if (!lock)
			break;

		BLooper *looper;
		View *view = dynamic_cast<View *>(fOwner.Target(&looper));
		if (!view)
			break;

		uint32 buttons;
		BPoint location;
		view->GetMouse(&location, &buttons, false);
		if (!buttons) {
			(view->*fDonePressing)(location);
			break;
		}
		if (fPressing)
			(view->*fPressing)(location, buttons);
		
		lock.Unlock();
		snooze(fPressingPeriod);
	}
	
	delete this;
	ASSERT(!"should not be here");
}

#if 0
/****************************************************************************
	CPPMouseDownThread

	NOTE:	Use inside a class that's derived from BHandler and is attached
			to a valid looper (otherwise it won't compile or the thread will
			just simply exit).

// Class definition
class FooBar {
	...
	bool		TrackMouseThread(BPoint &, uint32)
	status_t	DoneTracking(BPoint &);
}

// Class implementation
bool FooBar :: TrackMouseThread(BPoint &inCursorLocation, uint32 inButtons) {
	while(ok) {
		...
		return true;
	}
	return false;	// this means that the tracking is over...
}

status_t FooBar :: DoneTracking(BPoint &inCursorLocation) {
	...
	return B_OK;	// return code for exit_thread()
}

// in some method of FooBar
CPPMouseDownThread<FooBar> *thread = CPPMouseDownThread :: Launch(this,
										&TrackMouseThread, &DoneTracking);
// or somewhere else
CPPMouseDownThread<FooBar> *thread = CPPMouseDownThread :: Launch((FooBar *)object,
										&FooBar :: TrackMouseThread, &FooBar :: DoneTracking);

*****************************************************************************/
template <typename ClassType>
class CPPMouseDownThread : private ThreadPrimitive {
	typedef ThreadPrimitive Inherited;
	
	bigtime_t	fDelay;
	bool			(ClassType :: *fPressingFunc)(BPoint &, uint32);
	void			(ClassType :: *fDonePressingFunc)(BPoint &);
	ClassType	*fObject;
											
				~CPPMouseDownThread()	{}	// Private !
																
public:													
	static CPPMouseDownThread<ClassType>	*Launch(ClassType *inObject, bool (ClassType :: *inPressingFunc)(BPoint &, uint32),
							  					void (ClassType :: *inDonePressingFunc)(BPoint &) = 0, bigtime_t inDelay = 100000) {
	
		CPPMouseDownThread<ClassType> *thread = new CPPMouseDownThread<ClassType>(inObject, inPressingFunc, inDonePressingFunc, inDelay);
		if (thread -> Go() < B_OK) {
			delete thread;
			return 0;
			// *?*?*? exception ?
		}
		return thread;
	}
	
	void			Kill()			{ delete this; }	// Don't use !
						  
protected:									
	CPPMouseDownThread(ClassType *inObject, bool (ClassType :: *inPressingFunc)(BPoint &, uint32), void (ClassType :: *inDonePressingFunc)(BPoint &), bigtime_t inDelay)
						: Inherited("Some CPPMouseDownThread"),
						  fDelay(inDelay),
						  fPressingFunc(inPressingFunc),
						  fDonePressingFunc(inDonePressingFunc),
						  fObject(inObject) {
	}
	
	virtual	void Run() {
		uint32 buttons;
		BPoint location;

		bool doit = true;
		while (doit) {
			if ( ! fObject -> LockLooper())	// It's safe to delete the window while the
				break;						// thread is on it's way
								
			fObject -> GetMouse(&location, &buttons, false);
			if (buttons == 0) {
				if (fDonePressingFunc)
					(fObject ->* fDonePressingFunc)(location);
				fObject -> UnlockLooper();
				break;
			}
	
			if (fPressingFunc)
				doit = (fObject ->* fPressingFunc)(location, buttons);
				
			fObject -> UnlockLooper();
			if (fDelay)
				snooze(fDelay);
		}
					
		delete this;
		return;
	}
};


//
//	MouseDownThread
//
class MouseDownThread : private ThreadPrimitive {
	bigtime_t	fDelay;
	BView		*fView;
	void			(*fDonePressingFunc)(BView *, BPoint &);
	bool			(*fPressingFunc)(BView *, BPoint &, uint32);
																
				~MouseDownThread() TM_NOTHROW	{}	// Private !

public:													
	static MouseDownThread	*	Launch(BView *, bool (*)(BView *, BPoint &, uint32), void (*)(BView *, BPoint &) = 0, bigtime_t = 100000);
		void					Kill() TM_NOTHROW	{ delete this; }	// Don't use !
protected:									
							MouseDownThread(BView *, bool (*)(BView *, BPoint &, uint32), void (*)(BView *, BPoint &), bigtime_t) TM_NOTHROW;
		void				Run() TM_NOTHROW;
};
#endif

#endif // TM_NEED_MOUSE_DOWN_THREAD

#if TM_THREADPOOL_OPT
  #define LAUNCH(name, prio, functor)		ThreadPool::sInstance.Do(functor, name, prio)
 #define RET_TYPE							void
#else
  #define LAUNCH(name, prio, functor)		return Thread::Launch(functor, name, prio)
 #define RET_TYPE							Thread *
#endif

template <typename T>
struct _lint_param {

// XXX Should be this one, but add_const does not seem to add const for the parameter in
// FunctionObjectTemplate ctor, or call_traits<>::param_type removes it? test with new gcc
//	typedef typename call_traits<add_const<T>::type>::param_type	type;

	typedef typename call_traits<T>::param_type	type;
};

//template <typename T>
//struct executer : public unary_function<T, void> {
//	void operator()(call_traits<T>::param_type obj) {
//		(*obj)();
//	}
//};

void ExecuteThreadSequence(functor_list_t &list);

inline RET_TYPE
LaunchInNewThread(functor_list_t &list) {
	LAUNCH("Some tasklist", B_NORMAL_PRIORITY, new FunctionObjectTemplate<typeof(&ExecuteThreadSequence)>(&ExecuteThreadSequence, list));
}

template<typename RetType>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, RetType (*func)()) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (*)()>(func));
}

template<typename RetType, typename Param1>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, RetType (*func)(Param1), _lint_param<Param1>::type p1) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (*)(Param1)>(func, p1));
}

template<typename RetType, typename Param1, typename Param2>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, RetType (*func)(Param1, Param2), 
												_lint_param<Param1>::type p1, _lint_param<Param2>::type p2) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (*)(Param1, Param2)>(func, p1, p2));
}

template<typename RetType, typename Param1, typename Param2, typename Param3>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, RetType (*func)(Param1, Param2, Param3), 
												_lint_param<Param1>::type p1, _lint_param<Param2>::type p2, _lint_param<Param3>::type p3) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (*)(Param1, Param2, Param3)>(func, p1, p2, p3));
}

template<typename RetType, typename Param1, typename Param2, typename Param3, typename Param4>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, RetType (*func)(Param1, Param2, Param3, Param4), 
												_lint_param<Param1>::type p1, _lint_param<Param2>::type p2, _lint_param<Param3>::type p3, _lint_param<Param4>::type p4) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (*)(Param1, Param2, Param3, Param4)>(func, p1, p2, p3, p4));
}


// For C++ member functions
template<typename RetType, typename T>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, T *onThis, RetType (T::*func)()) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (T::*)()>(onThis, func));
}

template<typename RetType, typename T, typename Param1>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, T *onThis, RetType (T::*func)(Param1),
												_lint_param<Param1>::type p1) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (T::*)(Param1)>(onThis, func, p1));
}

template<typename RetType, typename T, typename Param1, typename Param2>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, T *onThis, RetType (T::*func)(Param1, Param2),
												_lint_param<Param1>::type p1, _lint_param<Param2>::type p2) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (T::*)(Param1, Param2)>(onThis, func, p1, p2));
}

template<typename RetType, typename T, typename Param1, typename Param2, typename Param3>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, T *onThis, RetType (T::*func)(Param1, Param2, Param3),
												_lint_param<Param1>::type p1, _lint_param<Param2>::type p2, _lint_param<Param3>::type p3) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (T::*)(Param1, Param2, Param3)>(onThis, func, p1, p2, p3));
}

template<typename RetType, typename T, typename Param1, typename Param2, typename Param3, typename Param4>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, T *onThis, RetType (T::*func)(Param1, Param2, Param3, Param4),
												_lint_param<Param1>::type p1, _lint_param<Param2>::type p2, _lint_param<Param3>::type p3, _lint_param<Param4>::type p4) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (T::*)(Param1, Param2, Param3, Param4)>(onThis, func, p1, p2, p3, p4));
}

// ---
template<typename RetType, typename T, typename Param1, typename Param2, typename Param3, typename Param4, typename Param5>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, T *onThis, RetType (T::*func)(Param1, Param2, Param3, Param4, Param5),
												_lint_param<Param1>::type p1, _lint_param<Param2>::type p2, _lint_param<Param3>::type p3, _lint_param<Param4>::type p4, _lint_param<Param5>::type p5) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (T::*)(Param1, Param2, Param3, Param4, Param5)>(onThis, func, p1, p2, p3, p4, p5));
}

// ---

// For C++ const member functions
template<typename RetType, typename T>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, T *onThis, RetType (T::*func)() const) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (T::*)() const>(onThis, func));
}

template<typename RetType, typename T, typename Param1>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, T *onThis, RetType (T::*func)(Param1) const,
												_lint_param<Param1>::type p1) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (T::*)(Param1) const>(onThis, func, p1));
}

template<typename RetType, typename T, typename Param1, typename Param2>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, T *onThis, RetType (T::*func)(Param1, Param2) const,
												_lint_param<Param1>::type p1, _lint_param<Param2>::type p2) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (T::*)(Param1, Param2) const>(onThis, func, p1, p2));
}

template<typename RetType, typename T, typename Param1, typename Param2, typename Param3>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, T *onThis, RetType (T::*func)(Param1, Param2, Param3) const,
												_lint_param<Param1>::type p1, _lint_param<Param2>::type p2, _lint_param<Param3>::type p3) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (T::*)(Param1, Param2, Param3) const>(onThis, func, p1, p2, p3));
}

template<typename RetType, typename T, typename Param1, typename Param2, typename Param3, typename Param4>
RET_TYPE
LaunchInNewThread(const char *name, int32 priority, T *onThis, RetType (T::*func)(Param1, Param2, Param3, Param4) const,
												_lint_param<Param1>::type p1, _lint_param<Param2>::type p2, _lint_param<Param3>::type p3, _lint_param<Param4>::type p4) {
	LAUNCH(name, priority, new FunctionObjectTemplate<RetType (T::*)(Param1, Param2, Param3, Param4) const>(onThis, func, p1, p2, p3, p4));
}

#undef LAUNCH
#undef RET_TYPE

}		// end of namespace

#endif	// THREADMAGIC_H
