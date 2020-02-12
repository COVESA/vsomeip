// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/android_sink_backend.hpp"

#define LOG_TAG "vSomeIP"

#ifdef ANDROID
#include <utils/Log.h>
#endif

#include <boost/log/expressions.hpp>
#include "../include/defines.hpp"

namespace expressions = boost::log::expressions;

namespace vsomeip_v3
{

android_sink_backend::android_sink_backend() {
    
}

android_sink_backend::~android_sink_backend() {

}

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", logging::trivial::severity_level)

void android_sink_backend::consume(const logging::record_view &rec) {
#ifdef ANDROID
    auto message = rec[expressions::smessage].get<std::string>().c_str();
    auto severity_level = rec[severity]; 

    switch (*severity_level) {
       case logging::trivial::fatal:
           ALOGE(message);
           break;
       case logging::trivial::error:
           ALOGE(message);
           break;
       case logging::trivial::warning:
           ALOGW(message);
           break;
       case logging::trivial::info:
           ALOGI(message);
           break;
       case logging::trivial::debug:
           ALOGD(message);
           break;
       case logging::trivial::trace:
           ALOGV(message);
           break;
       default:
           ALOGI(message);
           break;
       }

#else
    (void)rec;
#endif
}

} // namespace vsomeip_v3
