#ifndef BOOST_EXT_PROCESS_HPP
#define BOOST_EXT_PROCESS_HPP

#if defined(_MSC_VER)&&(_MSC_VER>=1200)
#pragma once
#endif

#include <boost/config.hpp>

#if defined(BOOST_WINDOWS)&&!defined(BOOST_DISABLE_WIN32)

#if defined(BOOST_USE_WINDOWS_H)
#include <windows.h>
#else
namespace boost _ext {
namespace process {

extern "C" __declspec(dllimport)
unsigned long __stdcall GetCurrentProcessId(void);

} /* namespace process */
} /* namespace boost_ext */
#endif

namespace boost{
namespace flyweights{

typedef unsigned long process_id_t;

inline process_id_t process_id()
{
	return GetCurrentProcessId();
}

} /* namespace flyweights::detail */

} /* namespace flyweights */

} /* namespace boost */

#elif defined(BOOST_HAS_UNISTD_H)

#include <unistd.h>

namespace boost_ext {
namespace process {

typedef pid_t process_id_t;

inline process_id_t process_id()
{
	return ::getpid();
}

} /* namespace process */
} /* namespace boost_ext */

#else
#error Unknown platform
#endif

#endif // BOOST_EXT_PROCESS_HPP
