//  boost cast.hpp header file  ----------------------------------------------//

//  (C) Copyright boost.org 1999. Permission to copy, use, modify, sell
//  and distribute this software is granted provided this copyright
//  notice appears in all copies. This software is provided "as is" without
//  express or implied warranty, and with no claim as to its suitability for
//  any purpose.

//  See http://www.boost.org for most recent version including documentation.

//  Revision History
//   2 Feb 00  Remove bad_numeric_cast ";" syntax error (Doncho Angelov)
//  26 Jan 00  Add missing throw() to bad_numeric_cast::what(0 (Adam Levar)
//  29 Dec 99  Change using declarations so usages in other namespaces work
//             correctly (Dave Abrahams)
//  23 Sep 99  Change polymorphic_downcast assert to also detect M.I. errors
//             as suggested Darin Adler and improved by Valentin Bonnard.  
//   2 Sep 99  Remove controversial asserts, simplify, rename.
//  30 Aug 99  Move to cast.hpp, replace value_cast with numeric_cast,
//             place in nested namespace.
//   3 Aug 99  Initial version

#ifndef BOOST_CAST_HPP
#define BOOST_CAST_HPP

#include <cassert>
#include <typeinfo> 
#include <limits>

namespace boost
{
  namespace cast
  {

//  See the documentation for descriptions of how to choose between
//  static_cast<>, dynamic_cast<>, polymorphic_cast<>. and down_cast<>

//  polymorphic_cast  --------------------------------------------------------//

    //  Runtime checked polymorphic downcasts and crosscasts.
    //  Suggested in The C++ Programming Language, 3rd Ed, Bjarne Stroustrup, 
    //  section 15.8 exercise 1, page 425.

    template <class Derived, class Base>
    inline Derived polymorphic_cast(Base* x)
    {
        Derived tmp = dynamic_cast<Derived>(x);
        if ( tmp == 0 ) throw std::bad_cast();
        return tmp;
    }

//  polymorphic_downcast  ----------------------------------------------------//

    //  assert() checked polymorphic downcast.  Crosscasts prohibited.

    //  WARNING: Because this cast uses assert(), it violates the One Definition
    //  Rule if NDEBUG is inconsistently defined across translation units.

    //  Contributed by Dave Abrahams

    template <class Derived, class Base>
    inline Derived polymorphic_downcast(Base* x)
    {
        assert( dynamic_cast<Derived>(x) == x );  // detect logic error
        return static_cast<Derived>(x);
    }

//  implicit_cast  -----------------------------------------------------------//

    //  Suggested in The C++ Programming Language, Bjarne Stroustrup, 3rd Ed.,
    //  section 13.3.1, page 335. 

    template< typename Target, typename Source >
    inline Target implicit_cast( Source s );  // see implementation below
    //  Returns: s

//  numeric_cast and related exception  --------------------------------------//

//  Contributed by Kevlin Henney

//  bad_numeric_cast  --------------------------------------------------------//

    // exception used to indicate runtime numeric_cast failure
    class bad_numeric_cast : public std::bad_cast
    {
    public:
        // constructors, destructors and assignment operator defaulted

        // function inlined for brevity and consistency with rest of library
        virtual const char *what() const throw()
        {
            return "bad numeric cast: loss of range in numeric_cast";
        }
    };

//  numeric_cast  ------------------------------------------------------------//

    template<typename Target, typename Source>
    inline Target numeric_cast(Source arg)
    {
        // typedefs abbreviating respective trait classes
        typedef std::numeric_limits<Source> arg_traits;
        typedef std::numeric_limits<Target> result_traits;

        // typedefs that act as compile time assertions
        // (to be replaced by boost compile time assertions
        // as and when they become available and are stable)
        typedef bool argument_must_be_numeric[arg_traits::is_specialized];
        typedef bool result_must_be_numeric[result_traits::is_specialized];

        if( (arg < 0 && !result_traits::is_signed) ||  // loss of negative range
              (arg_traits::is_signed &&
                 arg < result_traits::min()) ||        // underflow
               arg > result_traits::max() )            // overflow
            throw bad_numeric_cast();
        return static_cast<Target>(arg);
    }

//  Implementation details we wish were hidden  ------------------------------//

//  Compilers often warn on implicit conversions.  The point of implicit_cast
//  is to document that an implicit converstion is intended, so any implicit
//  conversion warning needs to be turned off. Note that correct code is
//  generated even if the compiler is not recognized.

//  Metrowerks CodeWarrior
//  (As of CW 5.0, this didn't have the desired effect.  Bug report submitted.) 
#   if defined __MWERKS__
#       pragma warn_implicitconv off
        template< typename Target, typename Source >
        inline Target implicit_cast( Source s ) { return s; }
#       pragma warn_implicitconv reset

//  Microsoft Visual C++
//  THIS MUST BE THE LAST COMPILER ENTRY.  Metrowerks, Intel, and possibly
//  others, define _MSC_VER.  Thus testing for _MSC_VER should be done after
//  testing for other compilers.
#   elif defined _MSC_VER
#       pragma warning( push )
#       pragma warning( disable : 4244 )
        template< typename Target, typename Source >
        inline Target implicit_cast( Source s ) { return s; }
#       pragma warning( pop )
      
//  Default
#   else 
        template< typename Target, typename Source >
        inline Target implicit_cast( Source s ) { return s; }
#   endif        
    
  } // namespace cast
  
  using ::boost::cast::polymorphic_cast;
  using ::boost::cast::polymorphic_downcast;
  using ::boost::cast::implicit_cast;
  using ::boost::cast::bad_numeric_cast;
  using ::boost::cast::numeric_cast;
  using ::boost::cast::implicit_cast;
  
} // namespace boost

#endif  // BOOST_CAST_HPP

