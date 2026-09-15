// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <set>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>

namespace vsomeip_v3 {

class boardnet_endpoint;
class eventgroupinfo;

namespace sd {

enum class subscription_state_e : uint8_t { ST_ACKNOWLEDGED, ST_NOT_ACKNOWLEDGED, ST_RESUBSCRIBING, ST_RESUBSCRIBING_NOT_ACKNOWLEDGED };

/**
 * @brief Represents a subscription in the service discovery module
 *
 * **Thread Safety:**
 * Not thread safe. Any usage has to be synchronized externally.
 */
class subscription {
public:
    explicit subscription(major_version_t _major, bool _is_selective, ttl_t _ttl, std::shared_ptr<boardnet_endpoint> _reliable,
                          std::shared_ptr<boardnet_endpoint> _unreliable);
    ~subscription() = default;

    major_version_t get_major() const;
    bool is_selective() const;

    ttl_t get_ttl() const;
    void set_ttl(ttl_t _ttl);

    std::shared_ptr<boardnet_endpoint> get_endpoint(bool _reliable) const;
    void set_endpoint(const std::shared_ptr<boardnet_endpoint>& _endpoint, bool _reliable);

    bool is_tcp_connection_established() const;
    void set_tcp_connection_established(bool _is_established);

    bool is_udp_connection_established() const;
    void set_udp_connection_established(bool _is_established);

    bool add_client(const client_t _client, subscription_state_e _state);
    bool remove_client(const client_t _client);

    /**
     * @brief Get subscription clients and their states
     *
     * @return Mutable reference to the map of clients and their subscription states
     */
    std::map<client_t, subscription_state_e>& get_clients();

    bool has_client() const;
    bool has_client(const client_t _client) const;

    void set_eventgroupinfo(const std::shared_ptr<eventgroupinfo> _info);
    std::weak_ptr<eventgroupinfo> get_eventgroupinfo() const;

private:
    const major_version_t major_;
    const bool is_selective_;

    ttl_t ttl_;

    std::shared_ptr<boardnet_endpoint> reliable_;
    std::shared_ptr<boardnet_endpoint> unreliable_;

    bool tcp_connection_established_;
    bool udp_connection_established_;

    std::map<client_t, subscription_state_e> clients_; // client-> is acknowledged?
    std::weak_ptr<eventgroupinfo> eg_info_;
};

} // namespace sd
} // namespace vsomeip_v3
