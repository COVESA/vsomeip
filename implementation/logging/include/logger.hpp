// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_LOGGER_HPP
#define VSOMEIP_LOGGER_HPP

#include <string>

#ifdef _WIN32
#include <iostream>
#endif

#include <vsomeip/export.hpp>

#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>

namespace vsomeip {

class VSOMEIP_IMPORT_EXPORT logger {
public:
    static std::shared_ptr<logger> get();

    virtual ~logger() {
    }

    virtual boost::log::sources::severity_logger_mt<
            boost::log::trivial::severity_level> & get_internal() = 0;
};

#define VSOMEIP_FATAL BOOST_LOG_SEV(vsomeip::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::fatal)
#define VSOMEIP_ERROR BOOST_LOG_SEV(vsomeip::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::error)
#define VSOMEIP_WARNING BOOST_LOG_SEV(vsomeip::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::warning)
#define VSOMEIP_INFO BOOST_LOG_SEV(vsomeip::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::info)
#define VSOMEIP_DEBUG BOOST_LOG_SEV(vsomeip::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::debug)
#define VSOMEIP_TRACE BOOST_LOG_SEV(vsomeip::logger::get()->get_internal(), \
                boost::log::trivial::severity_level::trace)

} // namespace vsomeip

#endif // VSOMEIP_LOGGER_HPP
