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

#ifndef _FUNCTIONOBJECT_H_
#define _FUNCTIONOBJECT_H_

#include <boost/call_traits.hpp>
#include <boost/type_traits.hpp>
#include <boost/utility.hpp>

#include <Message.h>

#include <Debug.h>

#define TM_NEED_PLAIN_C_FUNCTION_SUPPORT		1
//#define TM_NEED_CUSTOM_THIS_SUPPORT			1

#if _BUILDING_tracker
  namespace BPrivate {
#else
  namespace threadmagic {
#endif

using namespace boost;

// parameter binders serve to store a copy of a struct and
// pass it in and out by pointers, allowing struct parameters to share
// the same syntax as scalar ones

// the one in boost is probably buggy (had trouble with double &)
// simply writing "const" before a type in a template will not make it const (gcc 2.95)

template <typename T>	struct is_const 			{	static const bool value = false;	};
template <typename T>	struct is_const<const T>	{	static const bool value = true;	};

template <typename T>	struct add_const			{	typedef const T type;			};
template <typename T>	struct add_const<T &>		{	typedef const T &type;			};
template <typename T>	struct add_const<T *>		{	typedef const T *type;			};

// Does not compile with gcc 2.9
//template<typename T, bool is_const = is_const<T>::value,
//			bool ok =	is_arithmetic<remove_cv<T>::type>::value || is_pointer<remove_cv<T>::type>::value ||
//						is_reference<remove_cv<T>::type>::value
//		>
//class ParameterBinder;

// Possible workaround for above but does not produce with gcc what I need... :| let's hope users read the readme!
//template<typename T> struct ok_to_pbinder {
//	static const bool value = false;
//};
//
//template <typename T> struct ok_to_pbinder<T * const> {
//	static const bool value = true;				};
//template <typename T> struct ok_to_pbinder<T &> {
//	static const bool value = true;				};
//template <> struct ok_to_pbinder<const int>		{
//	static const bool value = true;				};
//template <> struct ok_to_pbinder<const long int>{
//	static const bool value = true;				};
//template <> struct ok_to_pbinder<const char>	{
//	static const bool value = true;				};
//template <> struct ok_to_pbinder<const double>	{
//	static const bool value = true;				};
//template <> struct ok_to_pbinder<const float>	{
//	static const bool value = true;				};
//template <> struct ok_to_pbinder<const bool>	{
//	static const bool value = true;				};
//
//template<typename T, bool is_const = is_const<T>::value, bool ok = ok_to_pbinder<T>::value>
//class ParameterBinder;

template<typename T, bool is_const = is_const<T>::value>
class ParameterBinder;

template<typename T>
class ParameterBinder<T, false> {
	remove_reference<T>::type	mParam;

public:
			ParameterBinder(call_traits<add_const<T>::type>::param_type inParam) throw() : mParam(inParam) {}
	T		Pass()	throw() { return mParam; }
};

template<typename T>
class ParameterBinder<T, true> {
	remove_reference<T>::type	mParam;

public:
			ParameterBinder(call_traits<add_const<T>::type>::param_type inParam) throw() : mParam(inParam) {}
	T		Pass()	const throw() { return mParam; }
};

#if 0	// gcc is fool, warning triggered not only when instantiated
template<typename T>
class ParameterBinder<T *, false> {
	T *		mParam;

public:
			ParameterBinder(const T * inParam) throw() : mParam(inParam) {
				#warning "Passing a variable pointer to a different thread!"
			}
	T *		Pass()	throw() { return mParam; }
};

template<typename T>
class ParameterBinder<T *, true> {
	const T *	mParam;

public:
			ParameterBinder(const T * inParam) throw() : mParam(inParam) {
				#warning "Passing a variable pointer to a different thread!"
			}
	const T *Pass()	const throw() { return mParam; }
};
#endif

// These two templates are here to forbid passing non-trivial types by value, because that
// requires three copy ctor calls. If you want to create a copy of your data make your function's
// input parameter a reference.
//template<typename T>
//class ParameterBinder<T, false, false> {
//public:
//			ParameterBinder(T) throw() {}
//	T		Pass()	throw() { dont_pass_args_by_value_use_references_instead_and_read_the_readme(); }
//};
//template<typename T>
//class ParameterBinder<T, true, false> {
//public:
//			ParameterBinder(T) throw() {}
//	T		Pass() const throw() { dont_pass_args_by_value_use_references_instead_and_read_the_readme(); }
//};


/***************************************************************
	FunctionObjects

	USAGE:	FunctionObject<Class, typeof(&FunctionToCall), ParamType1, ...>(...)
			Class: void in case of a plain C function
****************************************************************/

class FunctionObject : noncopyable {

public:
	virtual				~FunctionObject() 				{}
	virtual void		operator()() 					{ TRESPASS(); }
#if TM_NEED_CUSTOM_THIS_SUPPORT
	virtual void	 	CallWithThis(void *) 			{ TRESPASS(); }
#endif
};

template <typename T>
class FunctionObjectWithResult : public FunctionObject {
protected:
	T	mResult;
public:
	call_traits<add_const<T>::type>::param_type  Result()  { return mResult; }
};

template <typename T>
struct FOBaseSelector {
	typedef FunctionObjectWithResult<T> type;
};

template <>
struct FOBaseSelector<void> {
	typedef FunctionObject type;
};

//	==================================
//	FunctionPtrTraits for C++ function
//	==================================
template <typename T>
struct FunctionPtrTraits;

template <typename ReturnType, typename ClassType, typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4, typename ParamType5>
struct FunctionPtrTraits<ReturnType (ClassType :: *)(ParamType1, ParamType2, ParamType3, ParamType4, ParamType5)> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef typename call_traits<ParamType2>::param_type	param2_type;
	typedef typename call_traits<ParamType3>::param_type	param3_type;
	typedef typename call_traits<ParamType4>::param_type	param4_type;
	typedef typename call_traits<ParamType5>::param_type	param5_type;
	typedef ReturnType		return_type;
	typedef ClassType		class_type;
	static const bool		memb	= true;
	static const int		params	= 5;
};

// ---
template <typename ReturnType, typename ClassType, typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
struct FunctionPtrTraits<ReturnType (ClassType :: *)(ParamType1, ParamType2, ParamType3, ParamType4) const> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef typename call_traits<ParamType2>::param_type	param2_type;
	typedef typename call_traits<ParamType3>::param_type	param3_type;
	typedef typename call_traits<ParamType4>::param_type	param4_type;
	typedef ReturnType		return_type;
	typedef ClassType		class_type;
	static const bool		memb	= true;
	static const int		params	= 4;
};
template <typename ReturnType, typename ClassType, typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
struct FunctionPtrTraits<ReturnType (ClassType :: *)(ParamType1, ParamType2, ParamType3, ParamType4)> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef typename call_traits<ParamType2>::param_type	param2_type;
	typedef typename call_traits<ParamType3>::param_type	param3_type;
	typedef typename call_traits<ParamType4>::param_type	param4_type;
	typedef ReturnType		return_type;
	typedef ClassType		class_type;
	static const bool		memb	= true;
	static const int		params	= 4;
};


template <typename ReturnType, typename ClassType, typename ParamType1, typename ParamType2, typename ParamType3>
struct FunctionPtrTraits<ReturnType (ClassType :: *)(ParamType1, ParamType2, ParamType3) const> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef typename call_traits<ParamType2>::param_type	param2_type;
	typedef typename call_traits<ParamType3>::param_type	param3_type;
	typedef ReturnType		return_type;
	typedef ClassType		class_type;
	static const bool		memb	= true;
	static const int		params	= 3;
};
template <typename ReturnType, typename ClassType, typename ParamType1, typename ParamType2, typename ParamType3>
struct FunctionPtrTraits<ReturnType (ClassType :: *)(ParamType1, ParamType2, ParamType3)> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef typename call_traits<ParamType2>::param_type	param2_type;
	typedef typename call_traits<ParamType3>::param_type	param3_type;
	typedef ReturnType		return_type;
	typedef ClassType		class_type;
	static const bool		memb	= true;
	static const int		params	= 3;
};


template <typename ReturnType, typename ClassType, typename ParamType1, typename ParamType2>
struct FunctionPtrTraits<ReturnType (ClassType :: *)(ParamType1, ParamType2) const> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef typename call_traits<ParamType2>::param_type	param2_type;
	typedef ReturnType		return_type;
	typedef ClassType		class_type;
	static const bool		memb	= true;
	static const int		params	= 2;
};
template <typename ReturnType, typename ClassType, typename ParamType1, typename ParamType2>
struct FunctionPtrTraits<ReturnType (ClassType :: *)(ParamType1, ParamType2)> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef typename call_traits<ParamType2>::param_type	param2_type;
	typedef ReturnType		return_type;
	typedef ClassType		class_type;
	static const bool		memb	= true;
	static const int		params	= 2;
};


template <typename ReturnType, typename ClassType, typename ParamType1>
struct FunctionPtrTraits<ReturnType (ClassType :: *)(ParamType1) const> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef ReturnType		return_type;
	typedef ClassType		class_type;
	static const bool		memb	= true;
	static const int		params	= 1;
};
template <typename ReturnType, typename ClassType, typename ParamType1>
struct FunctionPtrTraits<ReturnType (ClassType :: *)(ParamType1)> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef ReturnType		return_type;
	typedef ClassType		class_type;
	static const bool		memb	= true;
	static const int		params	= 1;
};


template <typename ReturnType, typename ClassType>
struct FunctionPtrTraits<ReturnType (ClassType :: *)() const> {
	typedef ReturnType		return_type;
	typedef ClassType		class_type;
	static const bool		memb	= true;
	static const int		params	= 0;
};
template <typename ReturnType, typename ClassType>
struct FunctionPtrTraits<ReturnType (ClassType :: *)()> {
	typedef ReturnType		return_type;
	typedef ClassType		class_type;
	static const bool		memb	= true;
	static const int		params	= 0;
};

//	========================================
//	FunctionObjects for C++ member functions
//	========================================
template<	typename	func_ptr_type,
			typename	base_type	= FOBaseSelector<typename FunctionPtrTraits<func_ptr_type>::return_type>::type,
			bool		has_ret_type= !is_void<typename FunctionPtrTraits<func_ptr_type>::return_type>::value,
			bool		memb		= FunctionPtrTraits<func_ptr_type>::memb,
			int			params		= FunctionPtrTraits<func_ptr_type>::params
		>
class FunctionObjectTemplate;

template<	typename	func_ptr_type,
			typename	base_type	= FOBaseSelector<typename FunctionPtrTraits<func_ptr_type>::return_type>::type,
			bool		has_ret_type= !is_void<typename FunctionPtrTraits<func_ptr_type>::return_type>::value,
			bool		memb		= FunctionPtrTraits<func_ptr_type>::memb,
			int			params		= FunctionPtrTraits<func_ptr_type>::params
		>
class LockingFunctionObjectTemplate;

template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, true, true, 4> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param3_type		param3_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param4_type		param4_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	typedef typename call_traits<add_const<param3_type>::type>::param_type		cparam3_type;
	typedef typename call_traits<add_const<param4_type>::type>::param_type		cparam4_type;
	
	func_ptr_type					mFunction;
	class_type						*mObject;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	ParameterBinder<param3_type>	mParamBinder3;
	ParameterBinder<param4_type>	mParamBinder4;
	
public:
	FunctionObjectTemplate(class_type *inObject, func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3, cparam4_type inParam4)
									: mFunction(callThis), mObject(inObject), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3), mParamBinder4(inParam4) {}
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3, cparam4_type inParam4)
									: mFunction(callThis), mObject(0), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3), mParamBinder4(inParam4) {}
#if 0	
	typedef class FunctionObjectTemplate<func_ptr_type, base_type, true, true, 4>	this_type;

	template <typename T = return_type>
	friend struct Do {
		Do(this_type *us) {
			us -> mResult = (us->mObject ->* us->mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass(), mParamBinder4.Pass());
		}
	};

	template <>
	friend struct Do<void> {
		Do(this_type *us) {
			(us->mObject ->* us->mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass(), mParamBinder4.Pass());
		}
	};
#endif
	void operator()() {
		ASSERT(mObject != 0);
		mResult = (mObject ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass(), mParamBinder4.Pass());
	}
	
	void CallWithThis(void *inObject) {
		ASSERT(inObject != 0);
		mResult = (static_cast<class_type *>(inObject) ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass(), mParamBinder4.Pass());
	}
};

template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, true, true, 3> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param3_type		param3_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	typedef typename call_traits<add_const<param3_type>::type>::param_type		cparam3_type;
	
	func_ptr_type					mFunction;
	class_type						*mObject;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	ParameterBinder<param3_type>	mParamBinder3;
	
public:
	FunctionObjectTemplate(class_type *inObject, func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3)
									: mFunction(callThis), mObject(inObject), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3) {}
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3)
									: mFunction(callThis), mObject(0), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3) {}

	void operator()() {
		ASSERT(mObject != 0);
		mResult = (mObject ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass());
	}
	void CallWithThis(void *inObject) {
		ASSERT(inObject != 0);
		mResult = (static_cast<class_type *>(inObject) ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass());
	}
};

template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, true, true, 2> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	
	func_ptr_type					mFunction;
	class_type						*mObject;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	
public:
	FunctionObjectTemplate(class_type *inObject, func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2)
									: mFunction(callThis), mObject(inObject), mParamBinder1(inParam1), mParamBinder2(inParam2) {}
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2)
									: mFunction(callThis), mObject(0), mParamBinder1(inParam1), mParamBinder2(inParam2) {}
	
	void operator()() {
		ASSERT(mObject != 0);
		mResult = (mObject ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass());
	}
	void CallWithThis(void *inObject) {
		ASSERT(inObject != 0);
		mResult = (static_cast<class_type *>(inObject) ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass());
	}
};

template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, true, true, 1> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	
	func_ptr_type					mFunction;
	class_type						*mObject;
	ParameterBinder<param1_type>	mParamBinder1;
	
public:
	FunctionObjectTemplate(class_type *inObject, func_ptr_type callThis, cparam1_type inParam1)
									: mFunction(callThis), mObject(inObject), mParamBinder1(inParam1) {}
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1)
									: mFunction(callThis), mObject(0), mParamBinder1(inParam1) {}
	
	void operator()() {
		ASSERT(mObject != 0);
		mResult = (mObject ->* mFunction)(mParamBinder1.Pass());
	}
	void CallWithThis(void *inObject) {
		ASSERT(inObject != 0);
		mResult = (static_cast<class_type *>(inObject) ->* mFunction)(mParamBinder1.Pass());
	}
};

template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, true, true, 0> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	
	func_ptr_type					mFunction;
	class_type						*mObject;
	
public:
	FunctionObjectTemplate(class_type *inObject, func_ptr_type callThis)
									: mFunction(callThis), mObject(inObject) {}
	FunctionObjectTemplate(func_ptr_type callThis)
									: mFunction(callThis), mObject(0) {}
	
	void operator()() {
		ASSERT(mObject != 0);
		mResult = (mObject ->* mFunction)();
	}
	void CallWithThis(void *inObject) {
		ASSERT(inObject != 0);
		mResult = (static_cast<class_type *>(inObject) ->* mFunction)();
	}
};



//
//	For member functions with void return value
//

// ---
template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, false, true, 5> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param3_type		param3_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param4_type		param4_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param5_type		param5_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	typedef typename call_traits<add_const<param3_type>::type>::param_type		cparam3_type;
	typedef typename call_traits<add_const<param4_type>::type>::param_type		cparam4_type;
	typedef typename call_traits<add_const<param5_type>::type>::param_type		cparam5_type;
	
	func_ptr_type					mFunction;
	class_type						*mObject;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	ParameterBinder<param3_type>	mParamBinder3;
	ParameterBinder<param4_type>	mParamBinder4;
	ParameterBinder<param4_type>	mParamBinder5;
	
public:
	FunctionObjectTemplate(class_type *inObject, func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3, cparam4_type inParam4, cparam5_type inParam5)
									: mFunction(callThis), mObject(inObject), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3), mParamBinder4(inParam4), mParamBinder5(inParam5) {}
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3, cparam4_type inParam4, cparam5_type inParam5)
									: mFunction(callThis), mObject(0), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3), mParamBinder4(inParam4), mParamBinder5(inParam5) {}
	
	void operator()() {
		ASSERT(mObject != 0);
		(mObject ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass(), mParamBinder4.Pass(), mParamBinder5.Pass());
	}
	void CallWithThis(void *inObject) {
		ASSERT(inObject != 0);
		(static_cast<class_type *>(inObject) ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass(), mParamBinder4.Pass(), mParamBinder5.Pass());
	}
};

// ---

template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, false, true, 4> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param3_type		param3_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param4_type		param4_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	typedef typename call_traits<add_const<param3_type>::type>::param_type		cparam3_type;
	typedef typename call_traits<add_const<param4_type>::type>::param_type		cparam4_type;
	
	func_ptr_type					mFunction;
	class_type						*mObject;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	ParameterBinder<param3_type>	mParamBinder3;
	ParameterBinder<param4_type>	mParamBinder4;
	
public:
	FunctionObjectTemplate(class_type *inObject, func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3, cparam4_type inParam4)
									: mFunction(callThis), mObject(inObject), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3), mParamBinder4(inParam4) {}
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3, cparam4_type inParam4)
									: mFunction(callThis), mObject(0), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3), mParamBinder4(inParam4) {}
	
	void operator()() {
		ASSERT(mObject != 0);
		(mObject ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass(), mParamBinder4.Pass());
	}
	void CallWithThis(void *inObject) {
		ASSERT(inObject != 0);
		(static_cast<class_type *>(inObject) ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass(), mParamBinder4.Pass());
	}
};

template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, false, true, 3> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param3_type		param3_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	typedef typename call_traits<add_const<param3_type>::type>::param_type		cparam3_type;
	
	func_ptr_type					mFunction;
	class_type						*mObject;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	ParameterBinder<param3_type>	mParamBinder3;
	
public:
	FunctionObjectTemplate(class_type *inObject, func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3)
									: mFunction(callThis), mObject(inObject), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3) {}
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3)
									: mFunction(callThis), mObject(0), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3) {}
	
	void operator()() {
		ASSERT(mObject != 0);
		(mObject ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass());
	}
	void CallWithThis(void *inObject) {
		ASSERT(inObject != 0);
		(static_cast<class_type *>(inObject) ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass());
	}
};

template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, false, true, 2> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	
	func_ptr_type					mFunction;
	class_type						*mObject;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	
public:
	FunctionObjectTemplate(class_type *inObject, func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2)
									: mFunction(callThis), mObject(inObject), mParamBinder1(inParam1), mParamBinder2(inParam2) {}
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2)
									: mFunction(callThis), mObject(0), mParamBinder1(inParam1), mParamBinder2(inParam2) {}
	
	void operator()() {
		ASSERT(mObject != 0);
		(mObject ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass());
	}
	void CallWithThis(void *inObject) {
		ASSERT(inObject != 0);
		(static_cast<class_type *>(inObject) ->* mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass());
	}
};

template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, false, true, 1> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	
	func_ptr_type					mFunction;
	class_type						*mObject;
	ParameterBinder<param1_type>	mParamBinder1;
	
public:
	FunctionObjectTemplate(class_type *inObject, func_ptr_type callThis, cparam1_type inParam1)
									: mFunction(callThis), mObject(inObject), mParamBinder1(inParam1) {}
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1)
									: mFunction(callThis), mObject(0), mParamBinder1(inParam1) {}
	
	void operator()() {
		ASSERT(mObject != 0);
		(mObject ->* mFunction)(mParamBinder1.Pass());
	}
	void CallWithThis(void *inObject) {
		ASSERT(inObject != 0);
		(static_cast<class_type *>(inObject) ->* mFunction)(mParamBinder1.Pass());
	}
};

template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, false, true, 0> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	
	func_ptr_type					mFunction;
	class_type						*mObject;
	
public:
	FunctionObjectTemplate(class_type *inObject, func_ptr_type callThis) : mFunction(callThis), mObject(inObject) {}
	FunctionObjectTemplate(func_ptr_type callThis) : mFunction(callThis), mObject(0) {}
	
	void operator()() {
		ASSERT(mObject != 0);
		(mObject ->* mFunction)();
	}
	void CallWithThis(void *inObject) {
		ASSERT(inObject != 0);
		(static_cast<class_type *>(inObject) ->* mFunction)();
	}
};


template <typename func_ptr_type, typename base_type>
class LockingFunctionObjectTemplate<func_ptr_type, base_type, false, true, 0> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::class_type		class_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	
	func_ptr_type					mFunction;
	BMessenger						mMessenger;
	
public:
	LockingFunctionObjectTemplate(class_type *inObject, func_ptr_type callThis)
									: mFunction(callThis), mMessenger(inObject) {}
	
	void operator()() {
		class_type *target = dynamic_cast<class_type *>(mMessenger.Target(NULL));
		if (!target || !mMessenger.LockTarget())
			return;
		(target ->* mFunction)();
		target -> Looper() -> Unlock();
	}
};


#if defined(TM_NEED_PLAIN_C_FUNCTION_SUPPORT)


//
//	FunctionPtrTraits for plain C functions
//
template <typename ReturnType, typename ParamType1, typename ParamType2, typename ParamType3, typename ParamType4>
struct FunctionPtrTraits<ReturnType (*)(ParamType1, ParamType2, ParamType3, ParamType4)> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef typename call_traits<ParamType2>::param_type	param2_type;
	typedef typename call_traits<ParamType3>::param_type	param3_type;
	typedef typename call_traits<ParamType4>::param_type	param4_type;
	typedef ReturnType		return_type;
	static const bool		memb	= false;
	static const int		params	= 4;
};


template <typename ReturnType, typename ParamType1, typename ParamType2, typename ParamType3>
struct FunctionPtrTraits<ReturnType (*)(ParamType1, ParamType2, ParamType3)> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef typename call_traits<ParamType2>::param_type	param2_type;
	typedef typename call_traits<ParamType3>::param_type	param3_type;
	typedef ReturnType		return_type;
	static const bool		memb	= false;
	static const int		params	= 3;
};


template <typename ReturnType, typename ParamType1, typename ParamType2>
struct FunctionPtrTraits<ReturnType (*)(ParamType1, ParamType2)> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef typename call_traits<ParamType2>::param_type	param2_type;
	typedef ReturnType		return_type;
	static const bool		memb	= false;
	static const int		params	= 2;
};

template <typename ReturnType, typename ParamType1>
struct FunctionPtrTraits<ReturnType (*)(ParamType1)> {
	typedef typename call_traits<ParamType1>::param_type	param1_type;
	typedef ReturnType		return_type;
	static const bool		memb	= false;
	static const int		params	= 1;
};


template <typename ReturnType>
struct FunctionPtrTraits<ReturnType (*)()> {
	typedef ReturnType		return_type;
	static const bool		memb	= false;
	static const int		params	= 0;
};

//	=====================================
//	FunctionObjects for plain C functions
//	=====================================
template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, true, false, 4> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param3_type		param3_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param4_type		param4_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	typedef typename call_traits<add_const<param3_type>::type>::param_type		cparam3_type;
	typedef typename call_traits<add_const<param4_type>::type>::param_type		cparam4_type;
	
	func_ptr_type					mFunction;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	ParameterBinder<param3_type>	mParamBinder3;
	ParameterBinder<param4_type>	mParamBinder4;
	
public:
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3, cparam4_type inParam4)
									: mFunction(callThis), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3), mParamBinder4(inParam4) {}
	
	void operator()() {
		mResult = (*mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass(), mParamBinder4.Pass());
	}
};


template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, true, false, 3> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param3_type		param3_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	typedef typename call_traits<add_const<param3_type>::type>::param_type		cparam3_type;
	
	func_ptr_type					mFunction;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	ParameterBinder<param3_type>	mParamBinder3;
	
public:
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3)
									: mFunction(callThis), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3) {}
	
	void operator()() {
		mResult = (*mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass());
	}
};


template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, true, false, 2> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	
	func_ptr_type					mFunction;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	
public:
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2)
									: mFunction(callThis), mParamBinder1(inParam1), mParamBinder2(inParam2) {}
	
	void operator()() {
		mResult = (*mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass());
	}
};


template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, true, false, 1> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	
	func_ptr_type					mFunction;
	ParameterBinder<param1_type>	mParamBinder1;
	
public:
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1)
									: mFunction(callThis), mParamBinder1(inParam1) {}
	
	void operator()() {
		mResult = (*mFunction)(mParamBinder1.Pass());
	}
};


template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, true, false, 0> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	
	func_ptr_type					mFunction;
	
public:
	FunctionObjectTemplate(func_ptr_type callThis) : mFunction(callThis) {}
	
	void operator()() {
		mResult = (*mFunction)();
	}
};


// plain c functions, void return

template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, false, false, 4> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param3_type		param3_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param4_type		param4_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	typedef typename call_traits<add_const<param3_type>::type>::param_type		cparam3_type;
	typedef typename call_traits<add_const<param4_type>::type>::param_type		cparam4_type;
	
	func_ptr_type					mFunction;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	ParameterBinder<param3_type>	mParamBinder3;
	ParameterBinder<param4_type>	mParamBinder4;
	
public:
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3, cparam4_type inParam4)
									: mFunction(callThis), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3), mParamBinder4(inParam4) {}
	
	void operator()() {
		(*mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass(), mParamBinder4.Pass());
	}
};


template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, false, false, 3> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param3_type		param3_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	typedef typename call_traits<add_const<param3_type>::type>::param_type		cparam3_type;
	
	func_ptr_type					mFunction;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	ParameterBinder<param3_type>	mParamBinder3;
	
public:
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2, cparam3_type inParam3)
									: mFunction(callThis), mParamBinder1(inParam1), mParamBinder2(inParam2), mParamBinder3(inParam3) {}
	
	void operator()() {
		(*mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass(), mParamBinder3.Pass());
	}
};


template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, false, false, 2> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param2_type		param2_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	typedef typename call_traits<add_const<param2_type>::type>::param_type		cparam2_type;
	
	func_ptr_type					mFunction;
	ParameterBinder<param1_type>	mParamBinder1;
	ParameterBinder<param2_type>	mParamBinder2;
	
public:
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1, cparam2_type inParam2)
									: mFunction(callThis), mParamBinder1(inParam1), mParamBinder2(inParam2) {}
	
	void operator()() {
		(*mFunction)(mParamBinder1.Pass(), mParamBinder2.Pass());
	}
};


template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, false, false, 1> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	typedef typename FunctionPtrTraits<func_ptr_type>::param1_type		param1_type;
	typedef typename call_traits<add_const<param1_type>::type>::param_type		cparam1_type;
	
	func_ptr_type					mFunction;
	ParameterBinder<param1_type>	mParamBinder1;
	
public:
	FunctionObjectTemplate(func_ptr_type callThis, cparam1_type inParam1)
									: mFunction(callThis), mParamBinder1(inParam1) {}
	
	void operator()() {
		(*mFunction)(mParamBinder1.Pass());
	}
};


template <typename func_ptr_type, typename base_type>
class FunctionObjectTemplate<func_ptr_type, base_type, false, false, 0> : public base_type {
	typedef typename FunctionPtrTraits<func_ptr_type>::return_type		return_type;
	
	func_ptr_type					mFunction;
	
public:
	FunctionObjectTemplate(func_ptr_type callThis) : mFunction(callThis) {}
	
	void operator()() {
		(*mFunction)();
	}
};

#endif	// TM_NEED_PLAIN_C_FUNCTION_SUPPORT

//
//	If NewFunctionObject<> fails (which it quite often does) use
//	new FunctionObejctTemplate<typeof(&func)>(...)
//
template <typename T>
struct _nfo_param {
	typedef typename call_traits<add_const<T>::type>::param_type type;
};

template<typename RetType>
FOBaseSelector<RetType>::type *
NewFunctionObject(RetType (*func)()) {
	return new FunctionObjectTemplate<RetType (*)()>(func);
}

template<typename RetType, typename Param1>
FOBaseSelector<RetType>::type *
NewFunctionObject(RetType (*func)(Param1), _nfo_param<Param1>::type p1) {
	return new FunctionObjectTemplate<RetType (*)(Param1)>(func, p1);
}

template<typename RetType, typename Param1, typename Param2>
FOBaseSelector<RetType>::type *
NewFunctionObject(RetType (*func)(Param1, Param2), _nfo_param<Param1>::type p1, _nfo_param<Param2>::type p2) {
	return new FunctionObjectTemplate<RetType (*)(Param1, Param2)>(func, p1, p2);
}

template<typename RetType, typename Param1, typename Param2, typename Param3>
FOBaseSelector<RetType>::type *
NewFunctionObject(RetType (*func)(Param1, Param2, Param3), _nfo_param<Param1>::type p1, _nfo_param<Param2>::type p2, _nfo_param<Param3>::type p3) {
	return new FunctionObjectTemplate<RetType (*)(Param1, Param2, Param3)>(func, p1, p2, p3);
}

template<typename RetType, typename Param1, typename Param2, typename Param3, typename Param4>
FOBaseSelector<RetType>::type *
NewFunctionObject(RetType (*func)(Param1, Param2, Param3, Param4), _nfo_param<Param1>::type p1, _nfo_param<Param2>::type p2, _nfo_param<Param3>::type p3, _nfo_param<Param4>::type p4) {
	return new FunctionObjectTemplate<RetType (*)(Param1, Param2, Param3, Param4)>(func, p1, p2, p3, p4);
}



// For locking version
template<typename RetType, typename T>
FOBaseSelector<RetType>::type *
NewLockingFunctionObject(T *onThis, RetType (T::*func)()) {
	return new LockingFunctionObjectTemplate<RetType (T::*)()>(onThis, func);
}
template<typename RetType, typename T>
FOBaseSelector<RetType>::type *
NewLockingFunctionObject(T *onThis, RetType (T::*func)() const) {
	return new LockingFunctionObjectTemplate<RetType (T::*)() const>(onThis, func);
}

// For C++ member functions
template<typename RetType, typename T>
FOBaseSelector<RetType>::type *
NewFunctionObject(T *onThis, RetType (T::*func)()) {
	return new FunctionObjectTemplate<RetType (T::*)()>(onThis, func);
}
template<typename RetType, typename T>
FOBaseSelector<RetType>::type *
NewFunctionObject(T *onThis, RetType (T::*func)() const) {
	return new FunctionObjectTemplate<RetType (T::*)() const>(onThis, func);
}


template<typename RetType, typename T, typename Param1>
FOBaseSelector<RetType>::type *
NewFunctionObject(T *onThis, RetType (T::*func)(Param1), _nfo_param<Param1>::type p1) {
	return new FunctionObjectTemplate<RetType (T::*)(Param1)>(onThis, func, p1);
}
template<typename RetType, typename T, typename Param1>
FOBaseSelector<RetType>::type *
NewFunctionObject(T *onThis, RetType (T::*func)(Param1) const, _nfo_param<Param1>::type p1) {
	return new FunctionObjectTemplate<RetType (T::*)(Param1) const>(onThis, func, p1);
}


template<typename RetType, typename T, typename Param1, typename Param2>
FOBaseSelector<RetType>::type *
NewFunctionObject(T *onThis, RetType (T::*func)(Param1, Param2), _nfo_param<Param1>::type p1, _nfo_param<Param2>::type p2) {
	return new FunctionObjectTemplate<RetType (T::*)(Param1, Param2)>(onThis, func, p1, p2);
}
template<typename RetType, typename T, typename Param1, typename Param2>
FOBaseSelector<RetType>::type *
NewFunctionObject(T *onThis, RetType (T::*func)(Param1, Param2) const, _nfo_param<Param1>::type p1, _nfo_param<Param2>::type p2) {
	return new FunctionObjectTemplate<RetType (T::*)(Param1, Param2) const>(onThis, func, p1, p2);
}


template<typename RetType, typename T, typename Param1, typename Param2, typename Param3>
FOBaseSelector<RetType>::type *
NewFunctionObject(T *onThis, RetType (T::*func)(Param1, Param2, Param3), _nfo_param<Param1>::type p1, _nfo_param<Param2>::type p2, _nfo_param<Param3>::type p3) {
	return new FunctionObjectTemplate<RetType (T::*)(Param1, Param2, Param3)>(onThis, func, p1, p2, p3);
}
template<typename RetType, typename T, typename Param1, typename Param2, typename Param3>
FOBaseSelector<RetType>::type *
NewFunctionObject(T *onThis, RetType (T::*func)(Param1, Param2, Param3) const, _nfo_param<Param1>::type p1, _nfo_param<Param2>::type p2, _nfo_param<Param3>::type p3) {
	return new FunctionObjectTemplate<RetType (T::*)(Param1, Param2, Param3) const>(onThis, func, p1, p2, p3);
}


template<typename RetType, typename T, typename Param1, typename Param2, typename Param3, typename Param4>
FOBaseSelector<RetType>::type *
NewFunctionObject(T *onThis, RetType (T::*func)(Param1, Param2, Param3, Param4), _nfo_param<Param1>::type p1, _nfo_param<Param2>::type p2, _nfo_param<Param3>::type p3, _nfo_param<Param4>::type p4) {
	return new FunctionObjectTemplate<RetType (T::*)(Param1, Param2, Param3, Param4)>(onThis, func, p1, p2, p3, p4);
}
template<typename RetType, typename T, typename Param1, typename Param2, typename Param3, typename Param4>
FOBaseSelector<RetType>::type *
NewFunctionObject(T *onThis, RetType (T::*func)(Param1, Param2, Param3, Param4) const, _nfo_param<Param1>::type p1, _nfo_param<Param2>::type p2, _nfo_param<Param3>::type p3, _nfo_param<Param4>::type p4) {
	return new FunctionObjectTemplate<RetType (T::*)(Param1, Param2, Param3, Param4) const>(onThis, func, p1, p2, p3, p4);
}


}		// namespace end

#endif	// _FUNCTIONOBJECT_H_

