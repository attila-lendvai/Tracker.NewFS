//  Boost operators.hpp header file  -----------------------------------------//

//  (C) Copyright David Abrahams 1999. Permission to copy, use, modify, sell and
//  distribute this software is granted provided this copyright notice appears
//  in all copies. This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.

//  (C) Copyright Jeremy Siek 1999. Permission to copy, use, modify,
//  sell and distribute this software is granted provided this
//  copyright notice appears in all copies. This software is provided
//  "as is" without express or implied warranty, and with no claim as
//  to its suitability for any purpose.

//  See http://www.boost.org for most recent version including documentation.

//  Revision History
//  12 Dec 99 Initial version with iterator operators (Jeremy Siek)
//  18 Nov 99 Change name "divideable" to "dividable", remove unnecessary
//            specializations of dividable, subtractable, modable (Ed Brey) 
//  17 Nov 99 Add comments (Beman Dawes)
//            Remove unnecessary specialization of operators<> (Ed Brey)
//  15 Nov 99 Fix less_than_comparable<T,U> second operand type for first two
//            operators.(Beman Dawes)
//  12 Nov 99 Add operators templates (Ed Brey)
//  11 Nov 99 Add single template parameter version for compilers without
//            partial specialization (Beman Dawes)
//  10 Nov 99 Initial version

#ifndef BOOST_OPERATORS_HPP
#define BOOST_OPERATORS_HPP

#include <boost/config.hpp>
#include <iterator>

#if defined(__sgi) && !defined(__GNUC__)
#pragma set woff 1234
#endif

#ifndef BOOST_NO_OPERATORS_IN_NAMESPACE
namespace boost
{
#endif

//  Basic operator classes (contributed by Dave Abrahams ) -------------------//

//  Preferred version  -------------------------------------------------------//

//  Note that friend functions defined in a class are implicitly inline.
//  See the C++ std, 11.4 [class.friend] paragraph 5

#ifndef BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION

// The two-type version requires that T implement
// bool operator<( const U& ) const, bool operator>( const U& ) const
template <class T, class U = T>  // = T really is needed; not a coding error
class less_than_comparable
{
     friend bool operator<=( const T& x, const U& y ) { return !(x > y); }
     friend bool operator>=( const T& x, const U& y ) { return !(x < y); }
     friend bool operator>( const U& x, const T& y )  { return y < x; }
     friend bool operator<( const U& x, const T& y )  { return y > x; }
     friend bool operator<=( const U& x, const T& y ) { return !(y < x); }
     friend bool operator>=( const U& x, const T& y ) { return !(y > x); }
};

// This partial specialization requires only that T implement
// bool operator<( const T& ) const
template <class T>
class less_than_comparable<T, T>
{
     friend bool operator>( const T& x, const T& y )  { return y < x; }
     friend bool operator<=( const T& x, const T& y ) { return !(y < x); }
     friend bool operator>=( const T& x, const T& y ) { return !(x < y); }
};

template <class T, class U = T>
class equality_comparable
{
     friend bool operator==( const U& y, const T& x ) { return x == y; }
     friend bool operator!=( const U& y, const T& x ) { return !(x == y); }
     friend bool operator!=( const T& y, const U& x ) { return !(y == x); }
};

template <class T>
class equality_comparable<T, T>
{
     friend bool operator!=( const T& x, const T& y ) { return !(x == y); }
};


template <class T, class U = T>
class multipliable
{
     friend T operator*( T x, const U& y ) { return x *= y; }
     friend T operator*( const U& y, T x ) { return x *= y; }
};

template <class T>
class multipliable<T, T>
{
     friend T operator*( T x, const T& y ) { return x *= y; }
};

template <class T, class U = T>
class addable
{
     friend T operator+( T x, const U& y ) { return x += y; }
     friend T operator+( const U& y, T x ) { return x += y; }
};

template <class T>
class addable<T, T>
{
     friend T operator+( T x, const T& y ) { return x += y; }
};

template <class T, class U = T>
class subtractable
{
     friend T operator-( T x, const U& y ) { return x -= y; }
};

template <class T, class U = T>
class dividable
{
     friend T operator/( T x, const U& y ) { return x /= y; }
};

template <class T, class U = T>
class modable
{
     friend T operator%( T x, const U& y ) { return x %= y; }
};

template <class T, class U = T>
class xorable
{
     friend T operator^( T x, const U& y ) { return x ^= y; }
     friend T operator^( const U& y, T x ) { return x ^= y; }
};

template <class T>
class xorable<T, T>
{
     friend T operator^( T x, const T& y ) { return x ^= y; }
};

template <class T, class U = T>
class andable
{
     friend T operator&( T x, const U& y ) { return x &= y; }
     friend T operator&( const U& y, T x ) { return x &= y; }
};

template <class T>
class andable<T, T>
{
     friend T operator&( T x, const T& y ) { return x &= y; }
};

template <class T, class U = T>
class orable
{
     friend T operator|( T x, const U& y ) { return x |= y; }
     friend T operator|( const U& y, T x ) { return x |= y; }
};

template <class T>
class orable<T, T>
{
     friend T operator|( T x, const T& y ) { return x |= y; }
};

//  incrementable and decrementable contributed by Jeremy Siek)
template <class T>
struct incrementable
{
  friend T operator++(T& x, int)
  {
    T tmp(x);
    ++x;
    return tmp;
  }
};
  

template <class T>
struct decrementable
{
  friend T operator--(T& x, int)
  {
    T tmp(x);
    --x;
    return tmp;                      
  }
};

template <class T, class U = T>
class operators : 
    less_than_comparable<T,U>,
    equality_comparable<T,U>, 
    addable<T,U>,             
    subtractable<T,U>,        
    multipliable<T,U>,        
    dividable<T,U>,          
    modable<T,U>,             
    orable<T,U>,              
    andable<T,U>,             
    xorable<T,U>,
    incrementable<T>,
    decrementable<T> {};          

//  BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION  --------------------------------//

#else  // BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION

// Requires only that T implement bool operator<( const T& ) const
template <class T>
class less_than_comparable
{
     friend bool operator>( const T& x, const T& y ) { return y < x; }
     friend bool operator<=( const T& x, const T& y ) { return !(y < x); }
     friend bool operator>=( const T& x, const T& y ) { return !(x < y); }
};

template <class T>
class equality_comparable
{
     friend bool operator!=( const T& x, const T& y ) { return !(x == y); }
};

template <class T>
class multipliable
{
     friend T operator*( T x, const T& y ) { return x *= y; }
};

template <class T>
class addable
{
     friend T operator+( T x, const T& y ) { return x += y; }
};

template <class T>
class subtractable
{
     friend T operator-( T x, const T& y ) { return x -= y; }
};

// Two argument version needed for random_access_iterator_helper
template <class T, class U>
class addable2
{
     friend T operator+( T x, const U& y ) { return x += y; }
     friend T operator+( const U& y, T x ) { return x += y; }
};

// Two argument version needed for random_access_iterator_helper
template <class T, class U>
class subtractable2
{
     friend T operator-( T x, const U& y ) { return x -= y; }
};

template <class T>
class dividable
{
     friend T operator/( T x, const T& y ) { return x /= y; }
};

template <class T>
class modable
{
     friend T operator%( T x, const T& y ) { return x %= y; }
};

template <class T>
class xorable
{
     friend T operator^( T x, const T& y ) { return x ^= y; }
};

template <class T>
class andable
{
     friend T operator&( T x, const T& y ) { return x &= y; }
};

template <class T>
class orable
{
     friend T operator|( T x, const T& y ) { return x |= y; }
};

//  incrementable and decrementable contributed by Jeremy Siek)
template <class T>
struct incrementable
{
  friend T operator++(T& x, int)
  {
    T tmp(x);
    ++x;
    return tmp;
  }
};
  

template <class T>
struct decrementable
{
  friend T operator--(T& x, int)
  {
    T tmp(x);
    --x;
    return tmp;                      
  }
};

template <class T>
class operators :
    less_than_comparable<T>,
    equality_comparable<T>,
    addable<T>,
    subtractable<T>,
    multipliable<T>,
    dividable<T>,
    modable<T>,
    orable<T>,
    andable<T>,
    xorable<T>,
    incrementable<T>,
    decrementable<T> {};          

#endif // BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION

#ifndef BOOST_NO_OPERATORS_IN_NAMESPACE
} // namespace boost
#else
namespace boost {
  using ::orable;
  using ::andable;
  using ::less_than_comparable;
  using ::equality_comparable;
  using ::multipliable;
  using ::addable;
  using ::subtractable;
  using ::dividable;
  using ::modable;
  using ::xorable;
  using ::incrementable;
  using ::decrementable;
  using ::operators;
}
#endif

//  Iterator operator classes (contributed by Jeremy Siek ) ------------------//

#ifndef BOOST_NO_OPERATORS_IN_NAMESPACE
namespace boost
{
#endif

template <class T, class V>
struct dereferenceable
{
  V* operator->() const
  { 
    return &*static_cast<const T&>(*this); 
  }
};


template <class T, class D, class R>
struct indexable
{
  R operator[](D n) const
  {
    return *(static_cast<const T&>(*this) + n);
  }
};

//  Iterator helper classes (contributed by Jeremy Siek ) --------------------//

template <class T,
          class V,
          class D = std::iterator_traits<char*>::difference_type,
          class P = V*,
          class R = V&>
struct forward_iterator_helper :
  equality_comparable<T>,
  incrementable<T>,
  dereferenceable<T,V>,
  std::iterator<std::forward_iterator_tag, V, D> { };


template <class T,
          class V,
          class D = std::iterator_traits<char*>::difference_type,
          class P = V*,
          class R = V&>
struct bidirectional_iterator_helper :
  equality_comparable<T>,
  incrementable<T>,
  decrementable<T>,
  dereferenceable<T,V>,
  std::iterator<std::bidirectional_iterator_tag, V, D> { };


#ifndef BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION
template <class T,
          class V, 
          class D = std::iterator_traits<char*>::difference_type, 
          class P = V*,
          class R = V&>
struct random_access_iterator_helper :
  equality_comparable<T>,
  less_than_comparable<T>,
  incrementable<T>,
  decrementable<T>,
  dereferenceable<T,V>,
  addable<T,D>,
  subtractable<T,D>,
  indexable<T,D,R>,
  std::iterator<std::random_access_iterator_tag, V, D>
{
  friend D requires_difference_operator(const T& x, const T& y) {
    return x - y;
  }
}; // random_access_iterator_helper
#else
template <class T,
          class V, 
          class D = ptrdiff_t, 
          class P = V*,
          class R = V&>
struct random_access_iterator_helper :
  equality_comparable<T>,
  less_than_comparable<T>,
  incrementable<T>,
  decrementable<T>,
  dereferenceable<T,V>,
  addable2<T,D>,
  subtractable2<T,D>,
  indexable<T,D,R>,
  std::iterator<std::random_access_iterator_tag, V, D>
{
  friend D requires_difference_operator(const T& x, const T& y) {
    return x - y;
  }
}; // random_access_iterator_helper
#endif // BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION


#ifndef BOOST_NO_OPERATORS_IN_NAMESPACE
} // namespace boost
#else

namespace boost {
  using ::incrementable;
  using ::decrementable;
  using ::dereferenceable;
  using ::indexable;
  using ::forward_iterator_helper;
  using ::bidirectional_iterator_helper;
  using ::random_access_iterator_helper;
}
#endif

#if defined(__sgi) && !defined(__GNUC__)
#pragma reset woff 1234
#endif

#endif // BOOST_OPERATORS_HPP
