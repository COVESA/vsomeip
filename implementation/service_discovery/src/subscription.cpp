// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/subscription.hpp"

namespace vsomeip {
namespace sd {

subscription::subscription(major_version_t _major, ttl_t _ttl,
        std::shared_ptr<endpoint> _reliable,
        std::shared_ptr<endpoint> _unreliable,
        subscription_type_e _subscription_type,
        uint8_t _counter)
        : major_(_major), ttl_(_ttl),
          reliable_(_reliable), unreliable_(_unreliable),
          is_acknowledged_(true),
          tcp_connection_established_(false),
          udp_connection_established_(false),
          subscription_type_(_subscription_type),
          counter_(_counter) {
}

subscription::~subscription() {
}

major_version_t subscription::get_major() const {
    return major_;
}

ttl_t subscription::get_ttl() const {
    return ttl_;
}

void subscription::set_ttl(ttl_t _ttl) {
    ttl_ = _ttl;
}

std::shared_ptr<endpoint> subscription::get_endpoint(bool _reliable) const {
    return (_reliable ? reliable_ : unreliable_);
}

void subscription::set_endpoint(std::shared_ptr<endpoint> _endpoint,
        bool _reliable) {
    if (_reliable)
        reliable_ = _endpoint;
    else
        unreliable_ = _endpoint;
}

bool subscription::is_acknowledged() const {
    return is_acknowledged_;
}

void subscription::set_acknowledged(bool _is_acknowledged) {
    is_acknowledged_ = _is_acknowledged;
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

subscription_type_e subscription::get_subscription_type() const {
    return subscription_type_;
}

uint8_t subscription::get_counter() const {
    return counter_;
}

} // namespace sd
} // namespace vsomeip
