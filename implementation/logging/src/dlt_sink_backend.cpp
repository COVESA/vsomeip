// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/dlt_sink_backend.hpp"

#ifdef USE_DLT
#include <dlt/dlt.h>
#endif

#include <boost/log/expressions.hpp>
#include "../include/defines.hpp"

namespace expressions = boost::log::expressions;

namespace vsomeip
{

dlt_sink_backend::dlt_sink_backend(const std::string &_app_id,
                                   const std::string &_context_id) {
    (void)_app_id;
#ifdef USE_DLT
    DLT_REGISTER_CONTEXT(dlt_, _context_id.c_str(), VSOMEIP_LOG_DEFAULT_CONTEXT_NAME);
#else
    (void)_context_id;
#endif
}

dlt_sink_backend::~dlt_sink_backend() {
#ifdef USE_DLT
    DLT_UNREGISTER_CONTEXT(dlt_);
#endif
}

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level)

void dlt_sink_backend::consume(const logging::record_view &rec) {
#ifdef USE_DLT
    auto message = rec[expressions::smessage];
    auto severity_level = rec[severity];
    DLT_LOG_STRING(dlt_, (severity_level)?level_as_dlt(*severity_level):DLT_LOG_WARN,
                   (message)?message.get<std::string>().c_str():"consume() w/o message");
#else
    (void)rec;
#endif
}

#ifdef USE_DLT
DltLogLevelType dlt_sink_backend::level_as_dlt(logging::trivial::severity_level _level) {
    switch (_level) {
       case logging::trivial::fatal:
           return DLT_LOG_FATAL;
       case logging::trivial::error:
           return DLT_LOG_ERROR;
       case logging::trivial::warning:
           return DLT_LOG_WARN;
       case logging::trivial::info:
           return DLT_LOG_INFO;
       case logging::trivial::debug:
           return DLT_LOG_DEBUG;
       case logging::trivial::trace:
           return DLT_LOG_VERBOSE;
       default:
           return DLT_LOG_DEFAULT;
       }
}
#endif

} /* namespace vsomeip */
