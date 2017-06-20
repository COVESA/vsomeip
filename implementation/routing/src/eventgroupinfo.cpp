// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>

#include <vsomeip/constants.hpp>


#include "../include/eventgroupinfo.hpp"
#include "../../endpoints/include/endpoint_definition.hpp"

namespace vsomeip {

eventgroupinfo::eventgroupinfo()
        : major_(DEFAULT_MAJOR), ttl_(DEFAULT_TTL), port_(ILLEGAL_PORT), threshold_(0) {
}

eventgroupinfo::eventgroupinfo(major_version_t _major, ttl_t _ttl)
        : major_(_major), ttl_(_ttl), port_(ILLEGAL_PORT), threshold_(0) {
}

eventgroupinfo::~eventgroupinfo() {
}

major_version_t eventgroupinfo::get_major() const {
    return major_;
}

void eventgroupinfo::set_major(major_version_t _major) {
    major_ = _major;
}

ttl_t eventgroupinfo::get_ttl() const {
    return ttl_;
}

void eventgroupinfo::set_ttl(ttl_t _ttl) {
    ttl_ = _ttl;
}

bool eventgroupinfo::is_multicast() const {
    std::lock_guard<std::mutex> its_lock(address_mutex_);
    return address_.is_multicast();
}

bool eventgroupinfo::is_sending_multicast() const {
    return (is_multicast() &&
            threshold_ != 0 &&
            get_unreliable_target_count() >= threshold_);
}

bool eventgroupinfo::get_multicast(boost::asio::ip::address &_address,
        uint16_t &_port) const {
    std::lock_guard<std::mutex> its_lock(address_mutex_);
    if (address_.is_multicast()) {
        _address = address_;
        _port = port_;
        return true;
    }
    return false;
}

void eventgroupinfo::set_multicast(const boost::asio::ip::address &_address,
        uint16_t _port) {
    std::lock_guard<std::mutex> its_lock(address_mutex_);
    address_ = _address;
    port_ = _port;
}

const std::set<std::shared_ptr<event> > eventgroupinfo::get_events() const {
    std::lock_guard<std::mutex> its_lock(events_mutex_);
    return events_;
}

void eventgroupinfo::add_event(std::shared_ptr<event> _event) {
    std::lock_guard<std::mutex> its_lock(events_mutex_);
    events_.insert(_event);
}

void eventgroupinfo::remove_event(std::shared_ptr<event> _event) {
    std::lock_guard<std::mutex> its_lock(events_mutex_);
    events_.erase(_event);
}

const std::list<eventgroupinfo::target_t> eventgroupinfo::get_targets() const {
    std::lock_guard<std::mutex> its_lock(targets_mutex_);
    return targets_;
}

uint32_t eventgroupinfo::get_unreliable_target_count() const {
    uint32_t _count(0);
    std::lock_guard<std::mutex> its_lock(targets_mutex_);
    for (auto i = targets_.begin(); i != targets_.end(); i++) {
       if (!i->endpoint_->is_reliable()) {
           _count++;
       }
    }
    return _count;
}

void eventgroupinfo::add_multicast_target(const eventgroupinfo::target_t &_multicast_target) {
    std::lock_guard<std::mutex> its_lock(multicast_targets_mutex_);
    if (std::find(multicast_targets_.begin(), multicast_targets_.end(), _multicast_target)
            == multicast_targets_.end()) {
        multicast_targets_.push_back(_multicast_target);
    }
}

void eventgroupinfo::clear_multicast_targets() {
    std::lock_guard<std::mutex> its_lock(multicast_targets_mutex_);
    multicast_targets_.clear();
}

const std::list<eventgroupinfo::target_t> eventgroupinfo::get_multicast_targets() const {
    std::lock_guard<std::mutex> its_lock(multicast_targets_mutex_);
    return multicast_targets_;
}

bool eventgroupinfo::add_target(const eventgroupinfo::target_t &_target) {
    std::lock_guard<std::mutex> its_lock(targets_mutex_);
    std::size_t its_size = targets_.size();
    if (std::find(targets_.begin(), targets_.end(), _target) == targets_.end()) {
        targets_.push_back(_target);
    }
    return (its_size != targets_.size());
}

bool eventgroupinfo::add_target(const eventgroupinfo::target_t &_target,
                                const eventgroupinfo::target_t &_subscriber) {
    bool add(false);
    bool ret(false);
    {
        std::lock_guard<std::mutex> its_lock(targets_mutex_);
        std::size_t its_size = targets_.size();
        if (std::find(targets_.begin(), targets_.end(), _subscriber)
                == targets_.end()) {
            targets_.push_back(_subscriber);
            add = true;
        }
        ret = (its_size != targets_.size());
    }
    if (add) {
        add_multicast_target(_target);
    }
    return ret;
}

bool eventgroupinfo::update_target(
        const std::shared_ptr<endpoint_definition> &_target,
        const std::chrono::steady_clock::time_point &_expiration) {
    std::lock_guard<std::mutex> its_lock(targets_mutex_);
     for (auto i = targets_.begin(); i != targets_.end(); i++) {
         if (i->endpoint_->get_address() == _target->get_address() &&
                 i->endpoint_->get_port() == _target->get_port()) {
             i->expiration_ = _expiration;
             return true;
         }
     }
     return false;
}

bool eventgroupinfo::remove_target(
        const std::shared_ptr<endpoint_definition> &_target) {
    std::lock_guard<std::mutex> its_lock(targets_mutex_);
    std::size_t its_size = targets_.size();

    for (auto i = targets_.begin(); i != targets_.end(); i++) {
        if (i->endpoint_ == _target) {
            targets_.erase(i);
            break;
        }
    }

    return (its_size != targets_.size());
}

void eventgroupinfo::clear_targets() {
    std::lock_guard<std::mutex> its_lock(targets_mutex_);
    targets_.clear();
}

uint8_t eventgroupinfo::get_threshold() const {
    return threshold_;
}

void eventgroupinfo::set_threshold(uint8_t _threshold) {
    threshold_ = _threshold;
}

std::unique_lock<std::mutex> eventgroupinfo::get_subscription_lock() {
    return std::unique_lock<std::mutex>(subscription_mutex_);
}

}  // namespace vsomeip
