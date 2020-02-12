// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOGGER_IMPL_HPP
#define VSOMEIP_V3_LOGGER_IMPL_HPP

#include <memory>
#include <string>

#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>

#include <vsomeip/internal/logger.hpp>
#include "dlt_sink_backend.hpp"
#include "android_sink_backend.hpp"

namespace vsomeip_v3 {

class configuration;

BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity",
        boost::log::trivial::severity_level)

typedef boost::log::sinks::synchronous_sink<
        boost::log::sinks::text_ostream_backend> sink_t;
typedef boost::log::sinks::synchronous_sink<
        dlt_sink_backend> dlt_sink_t;
typedef boost::log::sinks::synchronous_sink<
        android_sink_backend> android_sink_t;

class logger_impl: public logger {
public:
    static std::shared_ptr<logger_impl> & get();
    VSOMEIP_IMPORT_EXPORT static void init(const std::shared_ptr<configuration> &_configuration);

    logger_impl();

    boost::log::sources::severity_logger_mt<
            boost::log::trivial::severity_level> & get_internal();

private:
    void enable_console();
    void disable_console();

    void enable_file(const std::string &_path);
    void disable_file();

    void enable_dlt(const std::string &_app_id,
                    const std::string &_context_id);
    void disable_dlt();

    void enable_android();
    void disable_android();

private:
    boost::log::sources::severity_logger_mt<
            boost::log::trivial::severity_level> logger_;
    boost::log::trivial::severity_level loglevel_;

    boost::shared_ptr<sink_t> console_sink_;
    boost::shared_ptr<sink_t> file_sink_;
    boost::shared_ptr<dlt_sink_t> dlt_sink_;
    boost::shared_ptr<android_sink_t> android_sink_;
    boost::log::core_ptr log_core_;

private:
    void use_null_logger();
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_LOGGER_IMPL_HPP
