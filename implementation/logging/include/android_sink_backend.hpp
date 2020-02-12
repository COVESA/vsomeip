// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_ANDROID_SINK_BACKEND_HPP_
#define VSOMEIP_V3_ANDROID_SINK_BACKEND_HPP_

#include <boost/log/core.hpp>
#include <boost/log/sinks/basic_sink_backend.hpp>
#include <boost/log/trivial.hpp>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;

namespace vsomeip_v3 {

class android_sink_backend :
    public sinks::basic_sink_backend<
        sinks::combine_requirements<
            sinks::synchronized_feeding
        >::type
    > {
public:
    android_sink_backend();
    virtual ~android_sink_backend();

    void consume(const logging::record_view &rec);
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_ANDROID_SINK_BACKEND_HPP_
