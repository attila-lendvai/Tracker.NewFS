//  Boost config.hpp configuration header file  ------------------------------//

//  (C) Copyright Boost.org 1999. Permission to copy, use, modify, sell and
//  distribute this software is granted provided this copyright notice appears
//  in all copies. This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.

//  See http://www.boost.org for most recent version.

//  This header is used to pass configuration information to other Boost files,
//  allowing them to cope with platform dependencies such as arithmetic byte
//  ordering, compiler pragmas, or compiler shortcomings.  Without such
//  configuration information, many current compilers would not work with the
//  Boost libraries.
//
//  Centralizing configuration information is this header reduces the number of
//  files which must be modified  when porting libraries to new platforms,
//  or when compilers are updated. Ideally, no other files would have to be
//  modified when porting to a new platform.
//
//  Configuration headers are controversial because some view them as condoning
//  broken compilers and encouraging non-standard subsets.  Adding settings
//  for additional platforms and maintaining existing settings can also be
//  a problem. In other words, configuration headers are a necessary evil rather
//  than a desirable feature. The Boost config.hpp policy is designed to
//  minimize the problems and maximize the benefits of a configuration header.

//  The Boost config.hpp policy:
//
//     For Library Users
//
//     *  Boost library uses are never required to #include config.hpp, and
//        are to be discouraged from including it on their own.
//     *  Boost library users can request support for additional platforms be
//        added to config.hpp by emailing config@boost.org describing their
//        request.
//
//     For Library Implementors
//
//     *  Boost library implementors are not required to #include config.hpp,
//        and are not required in any way to support compilers which do not
//        comply with the C++ Standard (ISO/IEC 14882).
//     *  If a Boost library implementor wishes to support some nonconforming
//        compiler, or to support some platform specific feature, #include of 
//        config.hpp is the preferred way to obtain configuration information
//        not available from the standard headers such as <climits>, etc.
//     *  If configuration information can be deduced from standard headers
//        such as <climits>, use those standard headers rather than config.hpp.
//     *  Boost files that use macros normally defined in config.hpp should 
//        have sensible, standard conforming, default behavior if the macro
//        is not defined. This means that the starting point for porting
//        config.hpp to a new platform is simply to define nothing at all
//        specific to that platform. In the rare case where there is no sensible
//        default behavior, an #error message should describe the problem.
//     *  If a Boost library implementor wants something added to config.hpp,
//        post a request on the Boost mailing list.  There is no guarantee such
//        a request will be honored; the intent is to limit the complexity of
//        config.hpp.
//
//     General
//
//     *  The intent is to support only compilers which appear on their way to
//        becoming C++ Standard compliant, and only recent releases of those
//        compilers at that.
//     *  The intent is not to disable mainstream features now well-supported
//        by the majority of compilers, such as namespaces, exceptions,
//        RTTI, or templates.

//  Revision History (excluding changes for specific compilers)
//   18 Feb 00  BOOST_NO_INCLASS_MEMBER_INITIALIZATION added (Jens Maurer)
//   26 Jan 00  Borland compiler support added (John Maddock)
//   26 Jan 00  Sun compiler support added (Jörg Schaible)
//   30 Dec 99  BOOST_NMEMBER_TEMPLATES compatibility moved here from
//              smart_ptr.hpp. (Dave Abrahams)
//   15 Nov 99  BOOST_NO_OPERATORS_IN_NAMESPACE,
//              BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION added (Beman Dawes)
//   11 Oct 99  BOOST_NO_STDC_NAMESPACE refined; <cstddef> supplied
//   29 Sep 99  BOOST_NO_STDC_NAMESPACE added (Ed Brey)
//   24 Sep 99  BOOST_DECL added (Ed Brey)
//   10 Aug 99  Endedness flags added, GNU CC support added
//   22 Jul 99  Initial version
 

#ifndef BOOST_CONFIG_HPP
#define BOOST_CONFIG_HPP

//  Conformance Flag Macros  -------------------------------------------------//
//
//  Conformance flag macros should identify the absence of C++ Standard 
//  conformance rather than its presence.  This ensures that standard conforming
//  compilers do not require a lot of configuration flag macros.  It places the
//  burden where it should be, on non-conforming compilers.  In the future,
//  hopefully, less rather than more conformance flags will have to be defined.

//  BOOST_NO_INCLASS_MEMBER_INITIALIZER: Compiler violates std::9.4.2/4. 

//  BOOST_NO_OPERATORS_IN_NAMESPACE: Compiler requires inherited operator
//  friend functions to be defined at namespace scope, then using'ed to boost.
//  Probably GCC specific.  See boost/operators.hpp for example.

//  BOOST_NO_MEMBER_TEMPLATES: Member templates not supported at all.

//  BOOST_NO_MEMBER_TEMPLATE_FRIENDS: Member template friend syntax
//  ("template<class P> friend class frd;") described in the C++ Standard,
//  14.5.3, not supported.

//  BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION. Class template partial
//  specialization (14.5.4 [temp.class.spec]) not supported.

//  BOOST_NO_STDC_NAMESPACE: The contents of C++ standard headers for C library
//  functions (the <c...> headers) have not been placed in namespace std.
//  Because the use of std::size_t is so common, a specific workaround for
//  <cstddef> (and thus std::size_t) is provided in this header (see below).
//  For other <c...> headers, a workaround must be provided in the boost header:
//
//    #include <cstdlib>  // for abs
//    #ifdef BOOST_NO_STDC_NAMESPACE
//      namespace std { using ::abs; }
//    #endif

//  Compiler Control or Information Macros  ----------------------------------//
//
//  Compilers often supply features outside of the C++ Standard which need to be
//  controlled or detected. As usual, reasonable default behavior should occur
//  if any of these macros are not defined.

//  BOOST_DECL:  Certain compilers for Microsoft operating systems require
//  non-standard class and function decoration if dynamic load library linking
//  is desired.  BOOST_DECL supplies that decoration, defaulting to a nul string
//  so that it is harmless when not required.  Boost does not encourage the use
//  of BOOST_DECL - it is non-standard and to be avoided if practical to do so.

//  BOOST_DECL_EXPORTS:  User defined, BOOST_DECL_EXPORTS causes BOOST_DECL to
//  be defined as __declspec(dllexport) rather than __declspec(dllimport).

//  BOOST_SYSTEM_HAS_STDINT_H: There are no 1998 C++ Standard headers <stdint.h> 
//  or <cstdint>, although the 1999 C Standard does include <stdint.h>. 
//  If <stdint.h> is present, <boost/stdint.h> can make good use of it,
//  so a flag is supplied (signalling presence; thus the default is not
//  present, conforming to the current C++ standard).

//  Compilers are listed in alphabetic order (except VC++ last - see below)---//

//  GNU CC (also known as GCC and G++)  --------------------------------------//

# if defined __GNUC__
#   if __GNUC__ == 2 && __GNUC_MINOR__ <= 95
#     define BOOST_NO_MEMBER_TEMPLATE_FRIENDS
#     define BOOST_NO_OPERATORS_IN_NAMESPACE
#   endif
#   if __GNUC__ == 2 && __GNUC_MINOR__ <= 8
#     define BOOST_NO_MEMBER_TEMPLATES
#   endif

#elif defined __BORLANDC__
// C++ builder 4:
#   define BOOST_NO_MEMBER_TEMPLATE_FRIENDS
#   if defined BOOST_DECL_EXPORTS
#     define BOOST_DECL __declspec(dllexport)
#   else
#     define BOOST_DECL __declspec(dllimport)
#   endif

//  Metrowerks CodeWarrior  --------------------------------------------------//

# elif defined  __MWERKS__
#   if __MWERKS__ <= 0x2301
#     define BOOST_NO_MEMBER_TEMPLATE_FRIENDS
#   endif
#   if __MWERKS__ >= 0x2300
#     define BOOST_SYSTEM_HAS_STDINT_H
#   endif
#   if defined BOOST_DECL_EXPORTS
#     define BOOST_DECL __declspec(dllexport)
#   else
#     define BOOST_DECL __declspec(dllimport)
#   endif

//  Sun Workshop Compiler C++ ------------------------------------------------//

# elif defined  __SUNPRO_CC
#    if __SUNPRO_CC <= 0x500
#      define BOOST_NO_MEMBER_TEMPLATES
#      define BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION
#    endif

//  Microsoft Visual C++ (excluding Intel/EDG front end)  --------------------//
//
//  Must remain the last #elif since some other vendors (Metrowerks, for
//  example) also #define _MSC_VER

# elif defined _MSC_VER && !defined __ICL
#   if _MSC_VER <= 1200  // 1200 == VC++ 6.0
#     define BOOST_NO_INCLASS_MEMBER_INITIALIZATION
#     define BOOST_NO_MEMBER_TEMPLATES
#     define BOOST_NO_MEMBER_TEMPLATE_FRIENDS
#     define BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION
#     define BOOST_NO_STDC_NAMESPACE
#   endif
#   if defined BOOST_DECL_EXPORTS
#     define BOOST_DECL __declspec(dllexport)
#   else
#     define BOOST_DECL __declspec(dllimport)
#   endif
# endif // Microsoft (excluding Intel/EDG frontend) 

# ifndef BOOST_DECL
#   define BOOST_DECL  // default for compilers not needing this decoration.
# endif

//  end of compiler specific portion  ----------------------------------------//

// Check for old name "BOOST_NMEMBER_TEMPLATES" for compatibility  -----------//
// Don't use BOOST_NMEMBER_TEMPLATES. It is deprecated and will be removed soon.
#if defined( BOOST_NMEMBER_TEMPLATES ) && !defined( BOOST_NO_MEMBER_TEMPLATES )
  #define BOOST_NO_MEMBER_TEMPLATES
#endif

//  BOOST_NO_STDC_NAMESPACE workaround  --------------------------------------//
//
//  Because std::size_t usage is so common, even in boost headers which do not
//  otherwise use the C library, the <cstddef> workaround is included here so
//  that ugly workaround code need not appear in many other boost headers.
//  NOTE WELL: This is a workaround for non-conforming compilers; <cstddef> 
//  must still be #included in the usual places so that <cstddef> inclusion
//  works as expected with standard conforming compilers.  The resulting
//  double inclusion of <cstddef> is harmless.

# ifdef BOOST_NO_STDC_NAMESPACE
#   include <cstddef>
    namespace std { using ::ptrdiff_t; using ::size_t; using ::wchar_t; }
# endif


#endif  // BOOST_CONFIG_HPP
