// Copyright (C) 2020-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "../../configuration/include/configuration.hpp"

#if defined(ANDROID) && !defined(ANDROID_CI_BUILD)
#include <utils/Log.h>

#ifdef ALOGE
#undef ALOGE
#endif

#define ALOGE(LOG_TAG, ...) ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__))
#ifndef LOGE
#define LOGE ALOGE
#endif

#ifdef ALOGW
#undef ALOGW
#endif

#define ALOGW(LOG_TAG, ...) ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__))
#ifndef LOGW
#define LOGW ALOGW
#endif

#ifdef ALOGI
#undef ALOGI
#endif

#define ALOGI(LOG_TAG, ...) ((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__))
#ifndef LOGI
#define LOGI ALOGI
#endif

#ifdef ALOGD
#undef ALOGD
#endif

#define ALOGD(LOG_TAG, ...) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#ifndef LOGD
#define LOGD ALOGD
#endif

#ifdef ALOGV
#undef ALOGV
#endif

#define ALOGV(LOG_TAG, ...) ((void)ALOG(LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#ifndef LOGV
#define LOGV ALOGV
#endif

#endif

#include <vsomeip/internal/logger.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/logger_impl.hpp"

namespace vsomeip_v3 {
namespace logger {

message::message(level_e _level) : std::ostream(&buffer_), level_{_level} {
    const auto* its_logger = logger_impl::get();
    if (!its_logger) {
        // May happen when trying to log during program termination
        return;
    }

    // Store logger config locally to avoid repeated access to atomics
    // TODO: Simplify once access to logger config no longer needs to be threadsafe
    auto its_cfg = its_logger->get_configuration();
    console_enabled_ = its_cfg.console_enabled;
    dlt_enabled_ = its_cfg.dlt_enabled;
    file_enabled_ = its_cfg.file_enabled;

    // Check whether this message should be logged at all
    if ((console_enabled_ || dlt_enabled_ || file_enabled_) && level_ <= its_cfg.loglevel) {
        buffer_.activate();
        when_ = std::chrono::system_clock::now();
    }
}

message::~message() try {
    if (!buffer_.is_active()) {
        return;
    }

    auto* its_logger = logger_impl::get();
    if (!its_logger) {
        // Trying to log at static deinit time, after logger has already been destroyed.
        // As this should never happen, log to std::cerr so it becomes visible if it does.
        // We do not expect any threads running at this time, so do not bother to avoid
        // interleaving.
        std::cerr << timestamp() << app_name() << " [VSIP] Trying to log after program termination: " << buffer_as_view() << std::endl;
        return;
    }

    if (console_enabled_) {
#ifndef ANDROID
        // std::cout is threadsafe, but output may be interleaved if multiple things are
        // streamed. To avoid a lock, build the full logline first and stream as a
        // single argument. In C++20, could use std::osyncstream and/or std::format to simplify
        // this.
        // Unfortunately, building the string is a bit awkward - freely concatenating
        // string_views and strings is a C++26 feature.
        const std::string_view ts = timestamp();
        const std::string_view app = app_name();
        const std::string_view lvl = level_as_view();
        const std::string_view msg = buffer_as_view();
        std::string output;
        output.reserve(ts.size() + app.size() + lvl.size() + msg.size() + 1);
        output += ts;
        output += app;
        output += lvl;
        output += msg;
        output += '\n';
        std::cout << output;

#elif !defined(ANDROID_CI_BUILD)
        static std::string app = runtime::get_property("LogApplication");

        // Note: Adding this prefix is not really optimal in terms of memory allocation/copying.
        // Could we set the prefix as separate arg instead? This would change the current
        // message structure through, so leave it for now.
        const static std::string prefix = "VSIP: ";
        const std::string_view view = buffer_as_view();
        std::string output;
        output.reserve(prefix.size() + view.size());
        output += prefix;
        output += view;

        switch (level_) {
        case level_e::LL_FATAL:
            ALOGE(app.c_str(), output.c_str());
            break;
        case level_e::LL_ERROR:
            ALOGE(app.c_str(), output.c_str());
            break;
        case level_e::LL_WARNING:
            ALOGW(app.c_str(), output.c_str());
            break;
        case level_e::LL_INFO:
            ALOGI(app.c_str(), output.c_str());
            break;
        case level_e::LL_DEBUG:
            ALOGD(app.c_str(), output.c_str());
            break;
        case level_e::LL_VERBOSE:
            ALOGV(app.c_str(), output.c_str());
            break;
        default:
            ALOGI(app.c_str(), output.c_str());
        };
#endif // !ANDROID
    }

    if (file_enabled_) {
        // Delegate logging of the message the logger, which ensures that log_to_file() is
        // thread-safe. To keep the API simple, construct the string here, where we have all
        // the information readily available.
        // Like above, we unfortunately have to use somewhat awkward code, as freely mixing of
        // strings and string_view only becomes available in C++26.
        const std::string_view ts = timestamp();
        const std::string_view lvl = level_as_view();
        const std::string_view msg = buffer_as_view();
        std::string output;
        output.reserve(ts.size() + lvl.size() + msg.size() + 1);
        output += ts;
        output += lvl;
        output += msg;
        output += '\n';
        its_logger->log_to_file(output);
    }

    if (dlt_enabled_) {
#ifdef USE_DLT
#ifndef ANDROID
        // Some versions of DLT require a null-terminated string and std::string_view does not
        // provide that, so explicitly add a null byte
        buffer_.sputc('\0');
        its_logger->log_to_dlt(level_, buffer_as_view());
#endif
#endif // USE_DLT
    }
} catch (const std::exception& e) {
    std::cerr << "\nVSIP: Error destroying message class: " << e.what() << '\n';
    return;
}

std::string_view message::timestamp() const {
    if (!timestamp_.empty()) {
        return timestamp_;
    }

    auto its_time_t = std::chrono::system_clock::to_time_t(when_);
    struct tm its_time { };
#ifdef _WIN32
    localtime_s(&its_time, &its_time_t);
#else
    localtime_r(&its_time_t, &its_time);
#endif
    auto its_us = std::chrono::duration_cast<std::chrono::microseconds>(when_.time_since_epoch()).count() % 1000000;

    // With C++20, could use std::format instead
    std::stringstream str;
    // clang-format off
    str << std::dec
        << std::setw(4) << its_time.tm_year + 1900 << "-"
        << std::setfill('0')
        << std::setw(2) << its_time.tm_mon + 1 << "-"
        << std::setw(2) << its_time.tm_mday << " "
        << std::setw(2) << its_time.tm_hour << ":"
        << std::setw(2) << its_time.tm_min << ":"
        << std::setw(2) << its_time.tm_sec << "."
        << std::setw(6) << its_us;
    // clang-format on
    timestamp_ = std::move(str).str();
    return timestamp_;
}

std::string_view message::app_name() const {
    static std::string its_name = [] {
        // Only read the env var once, on first use. This is also threadsafe.
        // NOLINTNEXTLINE(concurrency-mt-unsafe): False positve since C++11
        const char* name = std::getenv(VSOMEIP_ENV_APPLICATION_NAME);
        return name ? std::string{" "} + name : "";
    }();
    return its_name;
}

std::string_view message::level_as_view() const {
    switch (level_) {
    case level_e::LL_FATAL:
        return " [fatal] ";
    case level_e::LL_ERROR:
        return " [error] ";
    case level_e::LL_WARNING:
        return " [warning] ";
    case level_e::LL_INFO:
        return " [info] ";
    case level_e::LL_DEBUG:
        return " [debug] ";
    case level_e::LL_VERBOSE:
        return " [verbose] ";
    default:
        return "none";
    };
}

// Get a view directly on the internal buffer. Note: This is NOT null-terminated.
std::string_view message::buffer_as_view() const {
    return std::string_view{buffer_.data_.data(), buffer_.data_.size()};
}

// --------------------------------------------------------------------------------------------------

// We would like to avoid unnecessary vector resizes, but at the same not allocate
// too much upfront if not needed. Based on a preliminary analysis of the current
// log messages, the majority of messages are around 80-95 chars.
static constexpr size_t RESERVED_BLOCK_SIZE = 128;

void message::buffer::activate() {
    data_.reserve(RESERVED_BLOCK_SIZE);
    active_ = true;
}

bool message::buffer::is_active() const {
    return active_;
}

std::streambuf::int_type message::buffer::overflow(std::streambuf::int_type c) {
    if (active_ && c != EOF) {
        data_.emplace_back(static_cast<char>(c));
    }
    return c;
}
// Needed when inheriting from streambuf.
std::streamsize message::buffer::xsputn(const char* s, std::streamsize n) {
    if (active_) {
        // This is yet another heuristic, based on current DLT data.
        // Longer messages (above the 100 char mark) are usually still
        // below 200 chars, so reserving another block should avoid
        // too many resizes. Note that vector::insert() seems to only
        // raise capacity to the exact size needed, but we would like
        // to increase in larger blocks.
        if (data_.size() + static_cast<size_t>(n) > data_.capacity()) {
            data_.reserve(data_.size() + RESERVED_BLOCK_SIZE);
        }

        data_.insert(data_.end(), s, s + n);
    }
    return n;
}

} // namespace logger
} // namespace vsomeip_v3
