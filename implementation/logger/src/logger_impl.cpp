// Copyright (C) 2020-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/runtime.hpp>

#ifdef __QNX__
#include <sys/slog2.h>
extern char * __progname;
#elif __linux__
extern char * __progname;
#endif

#include "../include/logger_impl.hpp"
#include "../../configuration/include/configuration.hpp"

namespace vsomeip_v3 {
namespace logger {

logger_impl::logger_impl() : config_{{false, false, false, false, level_e::LL_NONE}} { }

#ifdef __QNX__
slog2_buffer_set_config_t   logger_impl::buffer_config = {0, "main", SLOG2_INFO, {"main", 4}, 1};
slog2_buffer_t              logger_impl::buffer_handle[1] = {0};
#endif

void logger_impl::init(const std::shared_ptr<configuration>& _configuration) {
    logger_impl::get()->set_configuration(_configuration);

    logger_impl::get()->init_slog2(_configuration);
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
        cfg.slog2_enabled = _configuration->has_slog2_log();
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

auto logger_impl::init_slog2(const std::shared_ptr<configuration>& _configuration) -> void {
#ifdef __QNX__
    if (slog2_is_initialized_ || !_configuration)
        return;

    logger_impl::buffer_config.buffer_set_name = __progname;
    logger_impl::buffer_config.num_buffers = 1;
    logger_impl::buffer_config.verbosity_level = log_level_as_slog2(_configuration->get_loglevel());

    // Use a 16kB log buffer by default
    // Override with a size specified by environment variable
    int num_pages = 4;
    auto s = getenv("VSOMEIP_SLOG2_NUM_PAGES");
    if (s != nullptr) {
        char* endptr = nullptr;
        errno = 0;
        auto const tmp_num_pages = strtoul(s, &endptr, 0);

        // Safety checks
        auto const at_least_one_digit_matched = endptr != s;
        auto const input_is_terminated = *endptr == '\0';
        auto const no_error = errno == 0;
        auto const within_range = tmp_num_pages > 0 && tmp_num_pages < 1024; // 1024 pages = 4MB, hard to imagine this not being enough

        if (at_least_one_digit_matched && input_is_terminated && no_error && within_range) {
            num_pages = static_cast<decltype(num_pages)>(tmp_num_pages);
        }
    }

    logger_impl::buffer_config.buffer_config[0].buffer_name = "vsomeip";
    logger_impl::buffer_config.buffer_config[0].num_pages = num_pages;

    // Register the buffer set.
    if (-1 == slog2_register(&logger_impl::buffer_config, logger_impl::buffer_handle, 0)) {
        std::fprintf(stderr, "Error registering slogger2 buffer!\n");
        return;
    } else {
        slog2_is_initialized_ = true;
    }
#else
    static_cast<void>(_configuration);
#endif
}

void logger_impl::log_to_slog2(level_e _level, std::string_view _msg) {
#ifdef __QNX__
    auto const handle = logger_impl::buffer_handle[0];
    auto const user_code = 0;

    slog2c(handle, user_code, log_level_as_slog2(_level), _msg.data());
#else
    static_cast<void>(_level);
    static_cast<void>(_msg);
#endif
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
    // knows... We don't expect any threads still running at this point, so no need to make this
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
