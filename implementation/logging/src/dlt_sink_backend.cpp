// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/dlt_sink_backend.hpp"

#ifdef USE_DLT
#include <dlt/dlt.h>
#endif

#include <boost/log/expressions.hpp>

namespace expressions = boost::log::expressions;

namespace vsomeip
{

dlt_sink_backend::dlt_sink_backend() {
#ifdef USE_DLT
    DLT_REGISTER_APP("vSIP", "vSomeIP application");
    DLT_REGISTER_CONTEXT(dlt_, "vSIP", "vSomeIP context");
#endif
}

dlt_sink_backend::~dlt_sink_backend() {
#ifdef USE_DLT
    DLT_UNREGISTER_CONTEXT(dlt_);
    DLT_UNREGISTER_APP();
#endif
}

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level)

void dlt_sink_backend::consume(const logging::record_view &rec) {
#ifdef USE_DLT
    std::string message = *rec[expressions::smessage];
    logging::trivial::severity_level severity_level = *rec[severity];
    DLT_LOG_STRING(dlt_, level_as_dlt(severity_level), message.c_str());
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
