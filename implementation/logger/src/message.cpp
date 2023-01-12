// Copyright (C) 2020 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <ctime>
#include <thread>
#include <fstream>
#include <iomanip>
#include <iostream>

#ifdef ANDROID
#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG NULL
#endif
#endif

#include <vsomeip/internal/logger.hpp>

#include "../include/logger_impl.hpp"
#include "../../configuration/include/configuration.hpp"

namespace {
void logToOstrem(std::ostream& stream, const struct tm* its_time, unsigned long its_ms, const char *its_level,
    const std::string& payload) {
    stream
        << std::dec << std::setw(4) << its_time->tm_year + 1900 << "-"
        << std::dec << std::setw(2) << std::setfill('0') << its_time->tm_mon << "-"
        << std::dec << std::setw(2) << std::setfill('0') << its_time->tm_mday << " "
        << std::dec << std::setw(2) << std::setfill('0') << its_time->tm_hour << ":"
        << std::dec << std::setw(2) << std::setfill('0') << its_time->tm_min << ":"
        << std::dec << std::setw(2) << std::setfill('0') << its_time->tm_sec << "."
        << std::dec << std::setw(6) << std::setfill('0') << its_ms << std::setfill(' ')<< 
        " [" << std::setw(17)<< std::this_thread::get_id() << "] "
        "[" << std::setw(7) << its_level << "] "
        << payload
        << std::endl;
}

#ifdef ANDROID
    const char* androidTag = LOG_TAG == NULL ? "vsomeip" :  LOG_TAG + ".vsomeip";
#endif
}

namespace vsomeip_v3 {
namespace logger {

std::mutex message::mutex__;

message::message(level_e _level)
    : std::ostream(&buffer_),
      level_(_level) {

    when_ = std::chrono::system_clock::now();
}

message::~message() {
    std::lock_guard<std::mutex> its_lock(mutex__);
    auto its_logger = logger_impl::get();
    auto its_configuration = its_logger->get_configuration();

    if (!its_configuration)
        return;

    if (level_ > its_configuration->get_loglevel())
        return;

    if (its_configuration->has_console_log()
            || its_configuration->has_file_log()) {

        // Prepare log level
        const char *its_level;
        switch (level_) {
        case level_e::LL_FATAL:
            its_level = "fatal";
            break;
        case level_e::LL_ERROR:
            its_level = "error";
            break;
        case level_e::LL_WARNING:
            its_level = "warning";
            break;
        case level_e::LL_INFO:
            its_level = "info";
            break;
        case level_e::LL_DEBUG:
            its_level = "debug";
            break;
        case level_e::LL_VERBOSE:
            its_level = "verbose";
            break;
        default:
            its_level = "none";
        };

        // Prepare time stamp
        auto its_time_t = std::chrono::system_clock::to_time_t(when_);
        auto its_time = std::localtime(&its_time_t);
        auto its_ms = (when_.time_since_epoch().count() / 100) % 1000000;

        if (its_configuration->has_console_log()) {
#ifndef ANDROID
            logToOstrem(std::cout, its_time, its_ms, its_level, buffer_.data_.str());
#else
            switch (level_) {
            case level_e::LL_FATAL:
                (void)__android_log_print(ANDROID_LOG_ERROR, androidTag, "%s", buffer_.data_.str().c_str());
                break;
            case level_e::LL_ERROR:
                (void)__android_log_print(ANDROID_LOG_ERROR, androidTag, "%s", buffer_.data_.str().c_str());
                break;
            case level_e::LL_WARNING:
                (void)__android_log_print(ANDROID_LOG_WARN, androidTag, "%s", buffer_.data_.str().c_str());
                break;
            case level_e::LL_INFO:
                (void)__android_log_print(ANDROID_LOG_INFO, androidTag, "%s", buffer_.data_.str().c_str());
                break;
            case level_e::LL_DEBUG:
                (void)__android_log_print(ANDROID_LOG_DEBUG, androidTag, "%s", buffer_.data_.str().c_str());
                break;
            case level_e::LL_VERBOSE:
                (void)__android_log_print(ANDROID_LOG_VERBOSE, androidTag, "%s", buffer_.data_.str().c_str());
                break;
            default:
                (void)__android_log_print(ANDROID_LOG_INFO, androidTag, "%s", buffer_.data_.str().c_str());
            };
#endif // !ANDROID
        }

        if (its_configuration->has_file_log()) {
            std::ofstream its_logfile(
                    its_configuration->get_logfile(),
                    std::ios_base::app);
            if (its_logfile.is_open()) {
                logToOstrem(its_logfile, its_time, its_ms, its_level, buffer_.data_.str());
            }
        }
    } else if (its_configuration->has_dlt_log()) {
#ifdef USE_DLT
        its_logger->log(level_, buffer_.data_.str().c_str());
#endif // USE_DLT
    }
}

std::streambuf::int_type
message::buffer::overflow(std::streambuf::int_type c) {
    if (c != EOF) {
        data_ << (char)c;
    }

    return (c);
}

std::streamsize
message::buffer::xsputn(const char *s, std::streamsize n) {
    data_.write(s, n);
    return (n);
}

} // namespace logger
} // namespace vsomeip_v3
