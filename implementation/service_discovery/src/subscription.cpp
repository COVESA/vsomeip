// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/subscription.hpp"

#include <vsomeip/internal/logger.hpp>

namespace vsomeip_v3 {
namespace sd {

subscription::subscription(major_version_t _major, bool _is_selective, ttl_t _ttl, std::shared_ptr<boardnet_endpoint> _reliable,
                           std::shared_ptr<boardnet_endpoint> _unreliable) :
    major_(_major), is_selective_(_is_selective), ttl_(_ttl), reliable_(_reliable), unreliable_(_unreliable),
    tcp_connection_established_(false), udp_connection_established_(false) { }

major_version_t subscription::get_major() const {
    return major_;
}

ttl_t subscription::get_ttl() const {
    return ttl_;
}

void subscription::set_ttl(ttl_t _ttl) {
    ttl_ = _ttl;
}

std::shared_ptr<boardnet_endpoint> subscription::get_endpoint(bool _reliable) const {
    return _reliable ? reliable_ : unreliable_;
}

void subscription::set_endpoint(const std::shared_ptr<boardnet_endpoint>& _endpoint, bool _reliable) {
    if (_reliable) {
        reliable_ = _endpoint;
    } else {
        unreliable_ = _endpoint;
    }
}

bool subscription::is_selective() const {
    return is_selective_;
}

bool subscription::is_tcp_connection_established() const {
    return tcp_connection_established_;
}
void subscription::set_tcp_connection_established(bool _is_established) {
    tcp_connection_established_ = _is_established;
}

bool subscription::is_udp_connection_established() const {
    return udp_connection_established_;
}
void subscription::set_udp_connection_established(bool _is_established) {
    udp_connection_established_ = _is_established;
}

bool subscription::add_client(const client_t _client, subscription_state_e _state) {
    auto find_client = clients_.find(_client);
    if (find_client != clients_.end()) {
        return false;
    }

    clients_[_client] = _state;
    return true;
}

bool subscription::remove_client(const client_t _client) {
    auto its_size = clients_.size();
    clients_.erase(_client);
    return its_size > clients_.size();
}

std::map<client_t, subscription_state_e>& subscription::get_clients() {
    return clients_;
}

bool subscription::has_client() const {
    return !clients_.empty();
}

bool subscription::has_client(const client_t _client) const {
    return clients_.count(_client) > 0;
}

void subscription::set_eventgroupinfo(const std::shared_ptr<eventgroupinfo> _info) {
    eg_info_ = _info;
}
std::weak_ptr<eventgroupinfo> subscription::get_eventgroupinfo() const {
    return eg_info_;
}

} // namespace sd
} // namespace vsomeip_v3
