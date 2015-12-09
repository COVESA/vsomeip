// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/subscription.hpp"

namespace vsomeip {
namespace sd {

subscription::subscription(major_version_t _major, ttl_t _ttl,
        std::shared_ptr<endpoint> _reliable,
        std::shared_ptr<endpoint> _unreliable,
        subscription_type_e _subscription_type)
        : major_(_major), ttl_(_ttl),
          reliable_(_reliable), unreliable_(_unreliable),
          is_acknowledged_(true),
          subscription_type_(_subscription_type) {
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

subscription_type_e subscription::get_subscription_type() const {
    return subscription_type_;
}

} // namespace sd
} // namespace vsomeip
