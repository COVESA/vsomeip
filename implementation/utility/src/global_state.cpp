// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/global_state.hpp"

#include <cstdlib>

#include <vsomeip/runtime.hpp>

#include "../../runtime/include/runtime_impl.hpp"
#include "../../logger/include/logger_impl.hpp"

#ifndef VSOMEIP_ENV_APPLICATION_NAME
#define VSOMEIP_ENV_APPLICATION_NAME "VSOMEIP_APPLICATION_NAME"
#endif

namespace vsomeip_v3 {

global_state::global_state() :
    app_name_(""), android_app_(""), android_prefix_(""), properties_() // Initialize properties map first
    ,
    runtime_(new runtime_impl()) {
    // Initialize logger static strings
    // NOLINTNEXTLINE(concurrency-mt-unsafe): False positive since this runs once before any threads
    const char* name = std::getenv(VSOMEIP_ENV_APPLICATION_NAME);
    app_name_ = name ? std::string{" "} + name : "";

#if defined(ANDROID)
    auto found = properties_.find("LogApplication");
    if (found != properties_.end()) {
        android_app_ = found->second;
    }
    android_prefix_ = "VSIP: ";
#endif
}

global_state& global_state::get() {
    static global_state instance;
    return instance;
}

} // namespace vsomeip_v3
