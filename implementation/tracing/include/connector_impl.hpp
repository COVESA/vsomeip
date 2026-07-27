// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#ifdef USE_DLT
#include <dlt/dlt.h>
#endif

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {

namespace cfg {
struct trace;
}

namespace trace {

class channel_impl;

class connector_impl {
public:
    static std::shared_ptr<connector_impl> get();

    connector_impl();
    ~connector_impl();

    void configure(const std::shared_ptr<cfg::trace>& _configuration);
    void reset();

    void set_enabled(bool _enabled);
    bool is_enabled() const;

    void set_sd_enabled(bool _sd_enabled);
    bool is_sd_enabled() const;

    bool is_sd_message(const byte_t* _data, uint16_t _data_size) const;

    std::shared_ptr<channel_impl> add_channel(const std::string& _id, const std::string& _description);
    bool remove_channel(const std::string& _id);
    std::shared_ptr<channel_impl> get_channel(const std::string& _id) const;

    void trace(const byte_t* _header, uint16_t _header_size, const byte_t* _data, uint32_t _data_size);

private:
    std::atomic<bool> is_enabled_;
    bool is_sd_enabled_;

    std::map<std::string, std::shared_ptr<channel_impl>> channels_;
    mutable std::mutex channels_mutex_;

    std::shared_ptr<channel_impl> get_channel_impl(const std::string& _id) const;

    // Full-logging size threshold in bytes (see VSOMEIP_TC_DEFAULT_FULL_LOGGING_THRESHOLD).
    // Guarded by configure_mutex_.
    uint32_t full_logging_threshold_;

    mutable std::mutex configure_mutex_;

#ifdef USE_DLT
    std::map<std::string, std::shared_ptr<DltContext>> contexts_;
    mutable std::mutex contexts_mutex_;
#endif
};

} // namespace trace
} // namespace vsomeip_v3
