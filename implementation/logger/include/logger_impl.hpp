// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOGGER_CONFIGURATION_HPP_
#define VSOMEIP_V3_LOGGER_CONFIGURATION_HPP_

#include <memory>
#include <mutex>

#ifdef __QNX__
#include <sys/slog2.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif
#ifdef USE_DLT
#ifndef ANDROID
#include <dlt/dlt.h>
#endif
#endif

#include <vsomeip/internal/logger.hpp>

namespace vsomeip_v3 {

class configuration;

namespace logger {

class logger_impl {
public:
    VSOMEIP_IMPORT_EXPORT static void init(const std::shared_ptr<configuration> &_configuration);
    static std::shared_ptr<logger_impl> get();

    logger_impl() = default;
    ~logger_impl();

    std::shared_ptr<configuration> get_configuration() const;
    void set_configuration(const std::shared_ptr<configuration> &_configuration);

#ifdef __QNX__
    static slog2_buffer_set_config_t   buffer_config;
    static slog2_buffer_t              buffer_handle[1];

    auto slog2_is_initialized() const -> bool {
        return slog2_is_initialized_;
    }

    auto set_slog2_initialized(bool initialized) -> void {
        slog2_is_initialized_ = initialized;
    }
#endif

#ifdef USE_DLT
    void log(level_e _level, const char *_data);

private:
    void enable_dlt(const std::string &_application, const std::string &_context);
#endif

    static auto constexpr levelAsString(level_e const _level) -> const char *
    {
        const char* its_level = nullptr;
        switch (_level) {
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
        }

        return its_level;
    }

#ifdef __QNX__
    static auto constexpr levelAsSlog2(level_e const _level) -> std::uint8_t
    {
        uint8_t severity = 0;
        switch (_level) {
        case level_e::LL_FATAL:
            severity = SLOG2_CRITICAL;
            break;
        case level_e::LL_ERROR:
            severity = SLOG2_ERROR;
            break;
        case level_e::LL_WARNING:
            severity = SLOG2_WARNING;
            break;
        case level_e::LL_INFO:
            severity = SLOG2_INFO;
            break;
        case level_e::LL_DEBUG:
            severity = SLOG2_DEBUG1;
            break;
        case level_e::LL_VERBOSE:
        default:
            severity = SLOG2_DEBUG2;
            break;
        }
        return severity;
    }
#endif

#ifdef ANDROID
    static constexpr auto levelAsAospLevel(level_e _level) -> android_LogPriority {
        switch (_level) {
        case level_e::LL_FATAL:
            return ANDROID_LOG_ERROR;
        case level_e::LL_ERROR:
            return ANDROID_LOG_ERROR;
        case level_e::LL_WARNING:
            return ANDROID_LOG_WARN;
        case level_e::LL_INFO:
            return ANDROID_LOG_INFO;
        case level_e::LL_DEBUG:
            return ANDROID_LOG_DEBUG;
        case level_e::LL_VERBOSE:
            return ANDROID_LOG_VERBOSE;
        default:
            return ANDROID_LOG_INFO;
        }
    }
#endif // !ANDROID

private:
    static std::mutex mutex__;

    std::shared_ptr<configuration> configuration_;
    mutable std::mutex configuration_mutex_;

#ifdef __QNX__
    // Flag whether slog2 was successfully initialized.
    bool slog2_is_initialized_ = false;
#endif

#ifdef USE_DLT
#ifndef ANDROID
    DLT_DECLARE_CONTEXT(dlt_)
#endif
#endif
};

} // namespace logger
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_LOGGER_CONFIGURATION_HPP_
