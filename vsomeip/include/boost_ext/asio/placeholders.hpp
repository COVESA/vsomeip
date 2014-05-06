#ifndef BOOST_EXT_ASIO_MQ_PLACEHOLDERS_HPP
#define BOOST_EXT_ASIO_MQ_PLACEHOLDERS_HPP

#include <boost/asio/placeholders.hpp>

namespace boost_ext {
namespace asio {
namespace placeholders {

#if defined(GENERATING_DOCUMENTATION)

unspecified priority;

#elif defined(BOOST_ASIO_HAS_BOOST_BIND)
# if defined(__BORLANDC__) || defined(__GNUC__)

inline boost::arg<2> priority()
{
  return boost::arg<2>();
}

# else

namespace detail
{
  template <int Number>
  struct placeholder
  {
    static boost::arg<Number>& get()
    {
      static boost::arg<Number> result;
      return result;
    }
  };
}

#  if defined(BOOST_ASIO_MSVC) && (BOOST_ASIO_MSVC < 1400)

static boost::arg<2>& priority
  = boost::asio::placeholders::detail::placeholder<2>::get();

#  else

namespace
{
  boost::arg<2>& priority
    = boost::asio::placeholders::detail::placeholder<2>::get();
} // namespace

#  endif
# endif
#endif

} // namespace placeholders
} // namespace asio
} // namespace boost_ext

#endif // BOOST_EXT_ASIO_MQ_PLACEHOLDERS_HPP

