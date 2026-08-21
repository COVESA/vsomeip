// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/log_context.hpp"

#include <forward_list>
#include <mutex>
#include <string>
#include <unordered_map>

namespace vsomeip_v3::trace {

#ifdef USE_DLT

namespace {

// Unregisters an eternal context at exit.
struct context_unregistrar {
    explicit context_unregistrar(DltContext* _context) : context(_context) { }
    ~context_unregistrar() { DLT_UNREGISTER_CONTEXT(*context); }

    context_unregistrar(const context_unregistrar&) = delete;
    context_unregistrar& operator=(const context_unregistrar&) = delete;
    context_unregistrar(context_unregistrar&&) = delete;
    context_unregistrar& operator=(context_unregistrar&&) = delete;

    DltContext* context;
};

} // namespace

#endif

// dlt-daemon does not like the same context name being registered twice
// (it will overwrite configuration of earlier incarnations), thus it is necessary
// to globally sync creation (and thus, registration) of contexts.
//
// Contexts are immortal - allocated once, registered, and then kept alive for the rest of the process's lifetime.
// This is because libdlt can hand a DltContext* off to its own background thread for large (segmented)
// trace messages, which might outlive the thread owning the context, so destruction could cause a use-after-free
// scenario and subsequent crash. The context is nevertheless unregistered at exit.
// See also logger_impl::get_log_context() for a similar rationale.
DltContext* get_log_context([[maybe_unused]] const char* _id, [[maybe_unused]] const char* _desc) {
#ifdef USE_DLT
    static std::mutex mutex;
    static std::unordered_map<std::string, DltContext*> contexts;
    static std::forward_list<context_unregistrar> unregistrars;

    std::scoped_lock lock(mutex);

    if (auto it = contexts.find(_id); it != contexts.end()) {
        return it->second;
    }

    auto* context = new DltContext{}; // NOSONAR: S6018, intentional leak - see rationale above
    DLT_REGISTER_CONTEXT_LL_TS(*context, _id, _desc, DLT_LOG_INFO, DLT_TRACE_STATUS_ON);
    contexts[_id] = context;
    unregistrars.emplace_front(context);
    return context;
#else
    return nullptr;
#endif
}

} // namespace vsomeip_v3::trace
