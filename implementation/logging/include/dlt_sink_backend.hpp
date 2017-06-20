// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef LOGGING_INCLUDE_DLT_SINK_BACKEND_HPP_
#define LOGGING_INCLUDE_DLT_SINK_BACKEND_HPP_

#ifdef USE_DLT
#include <dlt/dlt.h>
#endif

#include <boost/log/core.hpp>
#include <boost/log/sinks/basic_sink_backend.hpp>
#include <boost/log/trivial.hpp>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;

namespace vsomeip
{

class dlt_sink_backend :
    public sinks::basic_sink_backend<
        sinks::combine_requirements<
            sinks::synchronized_feeding
        >::type
    > {
public:
    dlt_sink_backend(const std::string &_app_id,
                     const std::string &_context_id);
    virtual ~dlt_sink_backend();

    void consume(const logging::record_view &rec);

private:

#ifdef USE_DLT
    DltLogLevelType level_as_dlt(logging::trivial::severity_level _level);
    DLT_DECLARE_CONTEXT(dlt_)
#endif
};

} /* namespace vsomeip */

#endif /* LOGGING_INCLUDE_DLT_SINK_BACKEND_HPP_ */
