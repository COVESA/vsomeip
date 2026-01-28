// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOGGER_CONFIGURATION_HPP_
#define VSOMEIP_V3_LOGGER_CONFIGURATION_HPP_

#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <string_view>

#ifdef USE_DLT
#include <dlt/dlt.h>
#endif

#include <vsomeip/internal/logger.hpp>

namespace vsomeip_v3 {

class configuration;

namespace logger {

class logger_impl {
public:
    logger_impl();
    VSOMEIP_IMPORT_EXPORT static void init(const std::shared_ptr<configuration>& _configuration);
    static logger_impl* get();

    // Note: struct must be trivially copyable and thus cannot contain the log file name.
    // alignas(4) to work around a bug in ancient MSVC15 which for some reason we still support...
    struct alignas(4) config {
        bool console_enabled;
        bool slog2_enabled;
        bool dlt_enabled;
        bool file_enabled;
        level_e loglevel;
    };

    void set_configuration(const std::shared_ptr<configuration>& _configuration);
    config get_configuration() const;

    void log_to_file(std::string_view _msg);
    void log_to_slog2(level_e _level, std::string_view _msg);

#ifdef USE_DLT
    static DltContext& dlt_context();
    void log_to_dlt(level_e _level, std::string_view _msg);
#endif

private:
    std::atomic<config> config_;

    std::mutex log_file_mutex_;
    std::ofstream log_file_;
    std::string app_name_;

#ifdef __QNX__
    // Flag whether slog2 was successfully initialized.
    bool slog2_is_initialized_ = false;

    static slog2_buffer_set_config_t   buffer_config;
    static slog2_buffer_t              buffer_handle[1];
    static std::uint8_t levelAsSlog2(level_e const _level);
#endif
    auto init_slog2(const std::shared_ptr<configuration>& _configuration) -> void;
};

} // namespace logger
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_LOGGER_CONFIGURATION_HPP_
