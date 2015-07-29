// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

#include <vsomeip/configuration.hpp>
#include "../include/logger_impl.hpp"

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
        : loglevel_(debug) {
    logging::add_common_attributes();
}

boost::log::sources::severity_logger<boost::log::trivial::severity_level> &
logger_impl::get_internal() {
    return logger_;
}

void logger_impl::init(const std::string &_path) {
    configuration *its_configuration = configuration::get(_path);
    get()->loglevel_ = its_configuration->get_loglevel();
    logging::core::get()->set_filter(
            logging::trivial::severity >= get()->loglevel_);

    if (its_configuration->has_console_log())
        get()->enable_console();

    if (its_configuration->has_file_log())
        get()->enable_file(its_configuration->get_logfile());

    if (its_configuration->has_dlt_log())
        get()->enable_dlt();
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

void logger_impl::enable_dlt() {
    // TODO: implement
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

