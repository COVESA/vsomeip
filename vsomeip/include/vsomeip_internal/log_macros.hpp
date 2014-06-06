//
// log_macros.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#ifndef LOG_MACROS_HPP_
#define VSOMEIP_INTERNAL_LOG_MACROS_HPPL_LOG_MACROS_HPP

#define VSOMEIP_FATAL BOOST_LOG_SEV(this->logger_, boost::log::trivial::severity_level::fatal)
#define VSOMEIP_ERROR BOOST_LOG_SEV(this->logger_, boost::log::trivial::severity_level::error)
#define VSOMEIP_WARNING BOOST_LOG_SEV(this->logger_, boost::log::trivial::severity_level::warning)
#define VSOMEIP_INFO BOOST_LOG_SEV(this->logger_, boost::log::trivial::severity_level::info)
#define VSOMEIP_DEBUG BOOST_LOG_SEV(this->logger_, boost::log::trivial::severity_level::debug)
#define VSOMEIP_TRACE BOOST_LOG_SEV(this->logger_, boost::log::trivial::severity_level::trace)

#endif // VSOMEIP_INTERNAL_LOG_MACROS_HPP
