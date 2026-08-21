// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {

namespace cfg {
struct trace;
}

namespace trace {

class channel_impl;

class connector_impl final {
public:
    explicit connector_impl(const std::shared_ptr<cfg::trace>& _configuration);

    void trace(const byte_t* _header, uint16_t _header_size, const byte_t* _data, uint32_t _data_size) const;

    // Read-only state accessors; used by routing_manager_impl and safe to expose.
    bool is_enabled() const;
    bool is_sd_enabled() const;
    bool is_sd_message(const byte_t* _data, uint16_t _data_size) const;

private:
    void set_enabled(bool _enabled);
    void set_sd_enabled(bool _sd_enabled);

    bool add_channel(const std::string& _id, const std::string& _description);
    std::shared_ptr<channel_impl> get_channel(const std::string& _id) const;

private:
    bool is_enabled_{false};
    bool is_sd_enabled_{false};

    std::map<std::string, std::shared_ptr<channel_impl>> channels_;

    // Full-logging size threshold in bytes (see VSOMEIP_TC_DEFAULT_FULL_LOGGING_THRESHOLD).
    uint32_t full_logging_threshold_{};

    // Wrapper for allowing connector tests to access private members
    friend struct connector_impl_test_peer;
};

} // namespace trace
} // namespace vsomeip_v3
