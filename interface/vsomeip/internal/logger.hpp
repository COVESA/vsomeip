// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOGGER_HPP_
#define VSOMEIP_V3_LOGGER_HPP_

#include <string>

#ifdef _WIN32
#include <iostream>
#endif

#include <vsomeip/export.hpp>

#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>

namespace vsomeip_v3 {

class VSOMEIP_IMPORT_EXPORT logger {
public:
    static std::shared_ptr<logger> get();

    virtual ~logger() {
    }

    virtual boost::log::sources::severity_logger_mt<
            boost::log::trivial::severity_level> & get_internal() = 0;
};

#define VSOMEIP_LOG_DEFAULT_APPLICATION_ID   "VSIP"
#define VSOMEIP_LOG_DEFAULT_APPLICATION_NAME  "vSomeIP application|SysInfra|IPC"

#define VSOMEIP_FATAL BOOST_LOG_SEV(vsomeip_v3::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::fatal)
#define VSOMEIP_ERROR BOOST_LOG_SEV(vsomeip_v3::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::error)
#define VSOMEIP_WARNING BOOST_LOG_SEV(vsomeip_v3::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::warning)
#define VSOMEIP_INFO BOOST_LOG_SEV(vsomeip_v3::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::info)
#define VSOMEIP_DEBUG BOOST_LOG_SEV(vsomeip_v3::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::debug)
#define VSOMEIP_TRACE BOOST_LOG_SEV(vsomeip_v3::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::trace)

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_LOGGER_HPP_
