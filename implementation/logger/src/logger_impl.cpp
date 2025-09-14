// Copyright (C) 2020-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/runtime.hpp>

#include "../include/logger_impl.hpp"
#include "../../configuration/include/configuration.hpp"

namespace vsomeip_v3 {
namespace logger {

logger_impl::logger_impl() : config_{{false, false, false, level_e::LL_NONE}} { }

void logger_impl::init(const std::shared_ptr<configuration>& _configuration) {
    logger_impl::get()->set_configuration(_configuration);
}

logger_impl::config logger_impl::get_configuration() const {
    return config_.load(std::memory_order_acquire);
}

void logger_impl::set_configuration(const std::shared_ptr<configuration>& _configuration) {
    if (_configuration) {
        // GCC < 10 hates member initializers in atomic structs
        config cfg; // NOLINT(cppcoreguidelines-pro-type-member-init)
        cfg.loglevel = _configuration->get_loglevel();
        cfg.console_enabled = _configuration->has_console_log();
        cfg.dlt_enabled = _configuration->has_dlt_log();
        {
            std::scoped_lock its_lock{log_file_mutex_};
            cfg.file_enabled = _configuration->has_file_log();
            if (cfg.file_enabled) {
                log_file_ = std::ofstream{_configuration->get_logfile()};
            }
        }
        config_.store(cfg, std::memory_order_release);
    }
}

void logger_impl::log_to_file(std::string_view _msg) {
    std::scoped_lock its_lock{log_file_mutex_};
    if (log_file_.is_open()) {
        log_file_ << _msg;
    }
}

#ifdef USE_DLT
#ifndef ANDROID

#define VSOMEIP_LOG_DEFAULT_CONTEXT_ID   "VSIP"
#define VSOMEIP_LOG_DEFAULT_CONTEXT_NAME "vSomeIP context"

DltContext& logger_impl::dlt_context() {
    // Initialize and register DLT context on first use, and unregister on destruction.
    // This is threadsafe.
    static auto deleter = [](DltContext* ctx) {
        DLT_UNREGISTER_CONTEXT(*ctx);
        delete ctx;
    };
    static auto context = [] {
        auto context_id = runtime::get_property("LogContext");
        if (context_id == "") {
            context_id = VSOMEIP_LOG_DEFAULT_CONTEXT_ID;
        }

        std::unique_ptr<DltContext, decltype(deleter)> ctxptr{new DltContext, deleter};
        DLT_REGISTER_CONTEXT(*ctxptr, context_id.c_str(), VSOMEIP_LOG_DEFAULT_CONTEXT_NAME);
        return ctxptr;
    }();

    return *context;
}

// Note: _msg is expected to include a terminating null byte
void logger_impl::log_to_dlt(level_e _level, std::string_view _msg) {
    // Prepare log level
    DltLogLevelType its_level{};
    switch (_level) {
    case level_e::LL_FATAL:
        its_level = DLT_LOG_FATAL;
        break;
    case level_e::LL_ERROR:
        its_level = DLT_LOG_ERROR;
        break;
    case level_e::LL_WARNING:
        its_level = DLT_LOG_WARN;
        break;
    case level_e::LL_INFO:
        its_level = DLT_LOG_INFO;
        break;
    case level_e::LL_DEBUG:
        its_level = DLT_LOG_DEBUG;
        break;
    case level_e::LL_VERBOSE:
        its_level = DLT_LOG_VERBOSE;
        break;
    default:
        its_level = DLT_LOG_DEFAULT;
    };

#if defined(DLT_STRING_PUBLIC)
    // If libdlt supports privacy-aware logging, ensure that all messages are marked
    // as public. Trace data containing message payloads (and thus, potentially sensitive
    // data) is logged through a different code path and remains private.
    DLT_LOG(dlt_context(), its_level, DLT_STRING_PUBLIC(_msg.data()));
#elif defined(DLT_SIZED_CSTRING)
    // Some versions of libdlt provide support for sized strings, which is more optimal than
    // DLT_LOG_STRING as it saves a call to strlen().
    // No need to log the terminating null byte in this case.
    DLT_LOG(dlt_context(), its_level, DLT_SIZED_CSTRING(_msg.data(), static_cast<std::uint16_t>(_msg.size() - 1)));
#else
    // Fallback to legacy log macro
    DLT_LOG_STRING(dlt_context(), its_level, _msg.data());
#endif
}

#endif
#endif

logger_impl* logger_impl::get() {
    // Use flag set by a custom deleter as a safety check in case something tries to log during
    // static deinitialization time, after logger is gone already. Should not happen, but one never
    // knows... We don't expect any threads still rnning at this point, so no need to make this
    // atomic.
    static bool is_destroyed{false};
    static auto deleter = [](logger_impl* ptr) {
        is_destroyed = true;
        delete ptr;
    };
    static std::unique_ptr<logger_impl, decltype(deleter)> instance{new logger_impl, deleter};
    return is_destroyed ? nullptr : instance.get();
}

} // namespace logger
} // namespace vsomeip_v3
