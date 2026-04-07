// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/serviceinfo.hpp"

namespace vsomeip_v3 {

serviceinfo::serviceinfo(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor, ttl_t _ttl,
                         bool _is_local) :
    service_(_service), instance_(_instance), major_(_major), minor_(_minor), is_local_(_is_local), ttl_(0), reliable_(nullptr),
    unreliable_(nullptr), is_in_preparation_(true), is_in_mainphase_(false) {

    std::chrono::seconds ttl = static_cast<std::chrono::seconds>(_ttl);
    ttl_ = std::chrono::duration_cast<std::chrono::milliseconds>(ttl);
}

serviceinfo::serviceinfo(const serviceinfo& _other) :
    service_(_other.service_), instance_(_other.instance_), major_(_other.major_), minor_(_other.minor_), is_local_(_other.is_local_),
    ttl_(_other.ttl_), reliable_(_other.reliable_), unreliable_(_other.unreliable_), requesters_(_other.requesters_),
    is_in_mainphase_(_other.is_in_mainphase_.load()) { }

serviceinfo::~serviceinfo() { }

service_t serviceinfo::get_service() const {
    return service_;
}

instance_t serviceinfo::get_instance() const {
    return instance_;
}

major_version_t serviceinfo::get_major() const {
    return major_;
}

minor_version_t serviceinfo::get_minor() const {
    return minor_;
}

ttl_t serviceinfo::get_ttl() const {
    std::scoped_lock its_lock(mutex_);
    ttl_t ttl = static_cast<ttl_t>(std::chrono::duration_cast<std::chrono::seconds>(ttl_).count());
    return ttl;
}

void serviceinfo::set_ttl(ttl_t _ttl) {
    std::scoped_lock its_lock(mutex_);
    std::chrono::seconds ttl = static_cast<std::chrono::seconds>(_ttl);
    ttl_ = std::chrono::duration_cast<std::chrono::milliseconds>(ttl);
}

std::chrono::milliseconds serviceinfo::get_precise_ttl() const {
    std::scoped_lock its_lock(mutex_);
    return ttl_;
}

void serviceinfo::set_precise_ttl(std::chrono::milliseconds _precise_ttl) {
    std::scoped_lock its_lock(mutex_);
    ttl_ = _precise_ttl;
}

std::shared_ptr<boardnet_endpoint> serviceinfo::get_endpoint(bool _reliable) const {
    std::scoped_lock its_lock(mutex_);
    return _reliable ? reliable_ : unreliable_;
}

void serviceinfo::set_endpoint(const std::shared_ptr<boardnet_endpoint>& _endpoint, bool _reliable) {
    // used in case the shared counter of reliable_/unreliable_ goes to 0
    // with the re-assignment, so not to call its dtors while holding mutex_
    std::shared_ptr<boardnet_endpoint> temp_ep;
    {
        std::scoped_lock its_lock(mutex_);
        if (_reliable) {
            temp_ep = reliable_;
            reliable_ = _endpoint;
        } else {
            temp_ep = unreliable_;
            unreliable_ = _endpoint;
        }
    }
}

void serviceinfo::add_client(client_t _client) {
    std::scoped_lock its_lock(mutex_);
    requesters_.insert(_client);
}

void serviceinfo::remove_client(client_t _client) {
    std::scoped_lock its_lock(mutex_);
    requesters_.erase(_client);
}

uint32_t serviceinfo::get_requesters_size() {
    std::scoped_lock its_lock(mutex_);
    return static_cast<std::uint32_t>(requesters_.size());
}

bool serviceinfo::is_local() const {
    return is_local_;
}

bool serviceinfo::is_in_preparation() const {
    return is_in_preparation_;
}

void serviceinfo::set_is_in_preparation(bool _in_preparation) {
    is_in_preparation_ = _in_preparation;
}

bool serviceinfo::is_in_mainphase() const {
    return is_in_mainphase_;
}

void serviceinfo::set_is_in_mainphase(bool _in_mainphase) {
    is_in_mainphase_ = _in_mainphase;
}

} // namespace vsomeip_v3
