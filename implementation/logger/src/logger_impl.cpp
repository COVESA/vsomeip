// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <ios>
#include <vsomeip/runtime.hpp>

#include "../include/logger_impl.hpp"
#include "../../configuration/include/configuration.hpp"

namespace vsomeip_v3::logger {

#ifdef USE_DLT

constexpr const char* VSOMEIP_LOG_DEFAULT_CONTEXT_ID = "VSIP";
constexpr const char* VSOMEIP_LOG_DEFAULT_CONTEXT_NAME = "vSomeIP context";

namespace {

// Unregisters the eternal logger's DLT context at exit, decoupled from the immortal logger instance.
// This sets an internal pointer to null, which will cause libdlt to discard any subsequent attempt to log,
// mitigating any potential libdlt-side issues in log-after-main scenarios.
struct context_unregistrar {
    DltContext* context;
    ~context_unregistrar() { DLT_UNREGISTER_CONTEXT(*context); }
};

} // namespace

#endif

logger_impl::logger_impl() : config_{{false, false, false, level_e::LL_NONE}} {
#ifdef USE_DLT
    auto context_id = runtime::get_property("LogContext");
    if (context_id == "") {
        context_id = VSOMEIP_LOG_DEFAULT_CONTEXT_ID;
    }
    DLT_REGISTER_CONTEXT(dlt_context_, context_id.c_str(), VSOMEIP_LOG_DEFAULT_CONTEXT_NAME);

    // Registered after libdlt's own atexit (via DLT_REGISTER_CONTEXT above), so it runs first and
    // unregisters before libdlt tears itself down.
    static context_unregistrar const unregistrar{&dlt_context_};
#endif
}

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
                log_file_ = std::ofstream{_configuration->get_logfile(), std::ios_base::out | std::ios_base::app};
            } else {
                // Release the OS handle when file logging is turned off.
                log_file_ = std::ofstream{};
            }
        }
        config_.store(cfg, std::memory_order_release);
    }
}

void logger_impl::log_to_file(std::string_view _msg) {
    std::scoped_lock its_lock{log_file_mutex_};
    if (log_file_.is_open()) {
        log_file_ << _msg << std::flush;
    }
}

#ifdef USE_DLT

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
    DLT_LOG(dlt_context_, its_level, DLT_STRING_PUBLIC(_msg.data()));
#elif defined(DLT_SIZED_CSTRING)
    // Some versions of libdlt provide support for sized strings, which is more optimal than
    // DLT_LOG_STRING as it saves a call to strlen().
    DLT_LOG(dlt_context_, its_level, DLT_SIZED_CSTRING(_msg.data(), static_cast<std::uint16_t>(_msg.size())));
#else
    // Fallback to legacy log macro
    DLT_LOG_STRING(dlt_context_, its_level, _msg.data());
#endif
}

#endif

logger_impl* logger_impl::get() {
    // Immortal singleton: never destroyed, so detached threads may keep logging after main() with no shutdown races.
    // The static pointer keeps it reachable, so LSan stays quiet.
    static auto* const instance = new logger_impl(); // NOSONAR: False positive S6018, also 'new' and "leak" is intentional
    return instance;
}

} // namespace vsomeip_v3::logger
