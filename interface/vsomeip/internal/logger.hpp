// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <chrono>
#include <cstdint>
#include <ostream>
#include <streambuf>
#include <string_view>
#include <vector>
#include <thread>

#include <vsomeip/export.hpp>

namespace vsomeip_v3 {
namespace logger {

enum class VSOMEIP_IMPORT_EXPORT level_e : std::uint8_t {
    LL_NONE = 0,
    LL_FATAL = 1,
    LL_ERROR = 2,
    LL_WARNING = 3,
    LL_INFO = 4,
    LL_DEBUG = 5,
    LL_VERBOSE = 6
};

class message : public std::ostream {
public:
    VSOMEIP_IMPORT_EXPORT explicit message(level_e _level);
    VSOMEIP_IMPORT_EXPORT ~message() override;

private:
    std::string_view timestamp() const;
    std::string_view app_name() const;
    std::string_view level_as_view() const;
    std::string_view buffer_as_view() const;

    struct buffer : public std::streambuf {
        void activate();
        bool is_active() const;

        std::streambuf::int_type overflow(std::streambuf::int_type c) override;
        std::streamsize xsputn(const char* s, std::streamsize n) override;

        // The internal storage for the streambuffer. We use this for being able to access data
        // directly as a string_view. This saving having to allocate a new std::string. The
        // main use-case is being able to pass data to DLT without unnecessary copying.
        std::vector<char> data_;
        bool active_{false};
    };

    buffer buffer_;
    const level_e level_;
    bool console_enabled_{false};
    bool dlt_enabled_{false};
    bool file_enabled_{false};
    std::chrono::system_clock::time_point when_;
    mutable std::string timestamp_;
};

} // namespace logger
} // namespace vsomeip_v3

#define VSOMEIP_FATAL   vsomeip_v3::logger::message(vsomeip_v3::logger::level_e::LL_FATAL)
#define VSOMEIP_ERROR   vsomeip_v3::logger::message(vsomeip_v3::logger::level_e::LL_ERROR)
#define VSOMEIP_WARNING vsomeip_v3::logger::message(vsomeip_v3::logger::level_e::LL_WARNING)
#define VSOMEIP_INFO    vsomeip_v3::logger::message(vsomeip_v3::logger::level_e::LL_INFO)
#define VSOMEIP_DEBUG   vsomeip_v3::logger::message(vsomeip_v3::logger::level_e::LL_DEBUG)
#define VSOMEIP_TRACE   vsomeip_v3::logger::message(vsomeip_v3::logger::level_e::LL_VERBOSE)

#define VSOMEIP_LOG_DEFAULT_APPLICATION_ID      "VSIP"
#define VSOMEIP_LOG_DEFAULT_APPLICATION_NAME    "vSomeIP application|SysInfra|IPC"

#define VSOMEIP_LOG_WITH_PREFIX(level_macro) \
    level_macro << VSOMEIP_LOG_PREFIX << "::" << __func__ << ": "

#define VSOMEIP_FATAL_P   VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_FATAL)
#define VSOMEIP_ERROR_P   VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_ERROR)
#define VSOMEIP_WARNING_P VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_WARNING)
#define VSOMEIP_INFO_P    VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_INFO)
#define VSOMEIP_DEBUG_P   VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_DEBUG)
#define VSOMEIP_TRACE_P   VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_TRACE)

/**
 * @brief Flush DLTs and abort process
 *
 * Write also to stderr - guarantees that _something_ appears, even if the DLT
 * infrastructure is somewhat broken
 */
#define VSOMEIP_TERMINATE(reason)                             \
    do {                                                      \
        auto r = (reason);                                    \
        VSOMEIP_FATAL << "TERMINATING DUE TO '" << r << "'";  \
        fprintf(stderr, "TERMINATING DUE TO '%s'", r);        \
        fflush(stderr);                                       \
        ; /* no better way to flush DLTs than to wait */      \
        std::this_thread::sleep_for(std::chrono::seconds(2)); \
        VSOMEIP_FATAL << "TERMINATING";                       \
        fprintf(stderr, "TERMINATING");                       \
        fflush(stderr);                                       \
        std::abort();                                         \
    } while (0)
