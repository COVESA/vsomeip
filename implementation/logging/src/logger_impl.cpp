// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <fstream>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/exception_handler.hpp>
#include <boost/phoenix/bind/bind_member_function.hpp>
#include <boost/shared_ptr.hpp>

// The "empty_deleter"-struct was moved from the log-package
// to the more generic "utility"-package in V1.55. If we'd
// use the "old" include, we get a "deprecation" warning
// when compiling with the newer boost version. Therefore a
// version dependent include handling is done here, which
// can/should be removed in case GPT is updating Boost to V1.55.
#if BOOST_VERSION < 105500
#include <boost/log/utility/empty_deleter.hpp>
#elif BOOST_VERSION < 105600
#include <boost/utility/empty_deleter.hpp>
#else
#include <boost/core/null_deleter.hpp>
#endif

#include <vsomeip/runtime.hpp>

#include "../include/logger_impl.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../include/defines.hpp"

namespace logging = boost::log;
namespace sources = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace expressions = boost::log::expressions;
namespace attributes = boost::log::attributes;

using namespace boost::log::trivial;

namespace vsomeip {

std::shared_ptr<logger_impl> & logger_impl::get() {
    static std::shared_ptr<logger_impl> the_logger__ = std::make_shared<
            logger_impl>();
    return the_logger__;
}

logger_impl::logger_impl()
        : loglevel_(debug),
          log_core_(logging::core::get()) {
    logging::add_common_attributes();
}

boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> &
logger_impl::get_internal() {
    return logger_;
}

void logger_impl::init(const std::shared_ptr<configuration> &_configuration) {
    get()->loglevel_ = _configuration->get_loglevel();
    logging::core::get()->set_exception_handler(boost::log::make_exception_suppressor());
    logging::core::get()->set_filter(
            logging::trivial::severity >= get()->loglevel_);

    if (_configuration->has_console_log())
        get()->enable_console();
    else
        get()->disable_console();

    if (_configuration->has_file_log())
        get()->enable_file(_configuration->get_logfile());
    else
        get()->disable_file();

    if (_configuration->has_dlt_log()) {
        std::string app_id = runtime::get_property("LogApplication");
        if (app_id == "") app_id = VSOMEIP_LOG_DEFAULT_APPLICATION_ID;
        std::string context_id = runtime::get_property("LogContext");
        if (context_id == "") context_id = VSOMEIP_LOG_DEFAULT_CONTEXT_ID;
        get()->enable_dlt(app_id, context_id);
    } else
        get()->disable_dlt();

    if (!_configuration->has_console_log() &&
           !_configuration->has_file_log() &&
           !_configuration->has_dlt_log()) {
        get()->use_null_logger();
    }
}

void logger_impl::enable_console() {
    if (console_sink_)
        return;

    auto vsomeip_log_format = expressions::stream
            << expressions::format_date_time<boost::posix_time::ptime>(
                    "TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << " ["
            << expressions::attr<severity_level>("Severity") << "] "
            << expressions::smessage;

    boost::shared_ptr<sinks::text_ostream_backend> backend = boost::make_shared<
            sinks::text_ostream_backend>();
    backend->add_stream(boost::shared_ptr<std::ostream>(&std::clog,
#if BOOST_VERSION < 105500
            boost::log::empty_deleter()
#elif BOOST_VERSION < 105600
            boost::empty_deleter()
#else
            boost::null_deleter()
#endif
            ));

    console_sink_ = boost::make_shared<sink_t>(backend);
    console_sink_->set_formatter(vsomeip_log_format);
    logging::core::get()->add_sink(console_sink_);
}

void logger_impl::disable_console() {
    if (console_sink_)
        logging::core::get()->remove_sink(console_sink_);
}

void logger_impl::enable_file(const std::string &_path) {
    if (file_sink_)
        return;

    auto vsomeip_log_format = expressions::stream
            << expressions::format_date_time<boost::posix_time::ptime>(
                    "TimeStamp", "%Y-%m-%d %H:%M:%S.%f") << " ["
            << expressions::attr<severity_level>("Severity") << "] "
            << expressions::smessage;

    boost::shared_ptr<sinks::text_ostream_backend> backend = boost::make_shared<
            sinks::text_ostream_backend>();
    backend->add_stream(
            boost::shared_ptr<std::ostream>(
                    boost::make_shared<std::ofstream>(_path)));

    file_sink_ = boost::make_shared<sink_t>(backend);
    file_sink_->set_formatter(vsomeip_log_format);
    logging::core::get()->add_sink(file_sink_);
}

void logger_impl::disable_file() {
    if (file_sink_)
        logging::core::get()->remove_sink(file_sink_);
}


void logger_impl::enable_dlt(const std::string &_app_id,
                             const std::string &_context_id) {
#ifdef USE_DLT
    if (dlt_sink_)
        return;

    boost::shared_ptr<dlt_sink_backend> backend = boost::make_shared<dlt_sink_backend>(_app_id,
                                                                                       _context_id);
    dlt_sink_ = boost::make_shared<dlt_sink_t>(backend);
    logging::core::get()->add_sink(dlt_sink_);
#else
    (void)_app_id;
    (void)_context_id;
#endif
}

void logger_impl::disable_dlt() {
    if (dlt_sink_)
        logging::core::get()->remove_sink(dlt_sink_);
}

void logger_impl::use_null_logger() {
    boost::shared_ptr<sinks::text_ostream_backend> backend = boost::make_shared<
            sinks::text_ostream_backend>();
    backend->add_stream(
            boost::shared_ptr<std::ostream>(new std::ofstream("/dev/null") // TODO: how to call this on windows
                    ));

    file_sink_ = boost::make_shared<sink_t>(backend);
    logging::core::get()->add_sink(file_sink_);
}

} // namespace vsomeip

