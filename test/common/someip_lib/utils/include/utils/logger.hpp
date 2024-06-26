// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <iostream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <streambuf>

namespace vsomeip_utilities {
namespace utils {
namespace logger {

#define LOG_FATAL   vsomeip_utilities::utils::logger::message(vsomeip_utilities::utils::logger::level_e::LL_FATAL)
#define LOG_ERROR   vsomeip_utilities::utils::logger::message(vsomeip_utilities::utils::logger::level_e::LL_ERROR)
#define LOG_WARNING vsomeip_utilities::utils::logger::message(vsomeip_utilities::utils::logger::level_e::LL_WARNING)
#define LOG_INFO    vsomeip_utilities::utils::logger::message(vsomeip_utilities::utils::logger::level_e::LL_INFO)
#define LOG_DEBUG   vsomeip_utilities::utils::logger::message(vsomeip_utilities::utils::logger::level_e::LL_DEBUG)
#define LOG_TRACE   vsomeip_utilities::utils::logger::message(vsomeip_utilities::utils::logger::level_e::LL_VERBOSE)

enum level_e { LL_FATAL, LL_ERROR, LL_WARNING, LL_INFO, LL_DEBUG, LL_VERBOSE };

class message
    : public std::ostream {

public:
    message(level_e _level);
    ~message();

private:
    class buffer : public std::streambuf {
    public:
        int_type overflow(int_type);
        std::streamsize xsputn(const char *, std::streamsize);
        std::stringstream data_;
    };

    std::chrono::system_clock::time_point when_;
    buffer buffer_;
    level_e level_;
    static std::mutex mutex__;
};

} // logger
} // utils
} // vsomeip_utilities

#endif // __LOGGER_H__
