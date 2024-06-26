// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <utils/logger.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>

namespace vsomeip_utilities {
namespace utils {
namespace logger {

constexpr char COLOR_RESET[]    { "\033[0m" };
constexpr char COLOR_RED[]      { "\033[31m" };
constexpr char COLOR_GREEN[]    { "\033[32m" };
constexpr char COLOR_YELLOW[]   { "\033[33m" };
constexpr char COLOR_BLUE[]     { "\033[34m" };
constexpr char COLOR_PURPLE[]   { "\033[35m" };
constexpr char COLOR_WHITE[]    { "\033[37m" };

std::mutex message::mutex__;

message::message(level_e _level)
    : std::ostream(&buffer_),
    level_(_level) {
    when_ = std::chrono::system_clock::now();
}

message::~message() {
    std::lock_guard<std::mutex> its_lock(mutex__);

    // Prepare time stamp
    const auto nowAsTimeT = std::chrono::system_clock::to_time_t(when_);
    const auto nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(when_.time_since_epoch()) % 1000;

    // Store original format
    std::ios_base::fmtflags prevFmt(std::cout.flags());

    switch (level_) {
    case level_e::LL_FATAL:
        std::cout << COLOR_RED;
        break;
    case level_e::LL_ERROR:
        std::cout << COLOR_PURPLE;
        break;
    case level_e::LL_WARNING:
        std::cout << COLOR_YELLOW;
        break;
    case level_e::LL_INFO:
        std::cout << COLOR_GREEN;
        break;
    case level_e::LL_VERBOSE:
        std::cout << COLOR_BLUE;
        break;
    case level_e::LL_DEBUG:
        std::cout << COLOR_WHITE;
        break;
    default:
        break;
    }

    std::cout << '[' << std::put_time(std::localtime(&nowAsTimeT), "%T") << '.'
            << std::setfill('0') << std::setw(3) << nowMs.count()
            << "] " << COLOR_RESET << buffer_.data_.str() << '\n';

    // Restore original format
    std::cout.flags(prevFmt);
}

std::streambuf::int_type
message::buffer::overflow(std::streambuf::int_type c) {
    if (c != EOF) {
        data_ << char(c);
    }
    return c;
}

std::streamsize
message::buffer::xsputn(const char *s, std::streamsize n) {
    data_.write(s, n);
    return n;
}

} // logger
} // utils
} // vsomeip_utilities
