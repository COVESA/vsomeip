// Copyright (C) 2020-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOGGER_HPP_
#define VSOMEIP_V3_LOGGER_HPP_

#include <chrono>
#include <cstdint>
#include <ostream>
#include <streambuf>
#include <string_view>
#include <vector>

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

#endif // VSOMEIP_V3_LOGGER_HPP_
