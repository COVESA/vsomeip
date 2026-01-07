// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_GLOBAL_STATE_HPP_
#define VSOMEIP_V3_GLOBAL_STATE_HPP_

#include <atomic>
#include <map>
#include <memory>
#include <string>

#include "../../logger/include/logger_impl.hpp"

namespace vsomeip_v3 {

class runtime_impl;

/**
 * @brief Global state container for vsomeip singletons
 *
 * This structure ensures proper destruction order of global vsomeip state.
 * By bundling all singletons together, we guarantee that:
 * 1. Logger strings remain valid during all destructor calls
 * 2. Logger impl remains valid during runtime_impl destruction
 *
 * See https://isocpp.org/wiki/faq/dtors#order-dtors-for-members
 *
 * Applications hold weak_ptr to runtime, so static applications will simply
 * find a null runtime during destruction rather than creating circular dependencies.
 */
struct global_state {
    std::string app_name_;
    std::string android_app_;
    std::string android_prefix_;

    // Runtime properties - must be initialized before application_impl
    std::map<std::string, std::string> properties_;
    std::mutex properties_mutex_;
    // Logger implementation - destroyed AFTER runtime (declared before runtime_)
    logger::logger_impl logger_;
    // Runtime implementation - destroyed FIRST (applications safely handle null via weak_ptr)
    std::shared_ptr<runtime_impl> runtime_;

    global_state();
    ~global_state() = default;

    // Get the global state singleton
    static global_state& get();
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_GLOBAL_STATE_HPP_
