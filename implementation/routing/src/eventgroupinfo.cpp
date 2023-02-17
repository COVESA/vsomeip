// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <iomanip>

#include <vsomeip/constants.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/eventgroupinfo.hpp"
#include "../include/event.hpp"
#include "../include/remote_subscription.hpp"
#include "../../endpoints/include/endpoint_definition.hpp"

namespace vsomeip_v3 {

eventgroupinfo::eventgroupinfo()
    : service_(0),
      instance_(0),
      eventgroup_(0),
      major_(DEFAULT_MAJOR),
      ttl_(DEFAULT_TTL),
      port_(ILLEGAL_PORT),
      threshold_(0),
      id_(PENDING_SUBSCRIPTION_ID),
      reliability_(reliability_type_e::RT_UNKNOWN),
      reliability_auto_mode_(false) {
}

eventgroupinfo::eventgroupinfo(
        const service_t _service, const instance_t _instance,
        const eventgroup_t _eventgroup, const major_version_t _major,
        const ttl_t _ttl)
    : service_(_service),
      instance_(_instance),
      eventgroup_(_eventgroup),
      major_(_major),
      ttl_(_ttl),
      port_(ILLEGAL_PORT),
      threshold_(0),
      id_(PENDING_SUBSCRIPTION_ID),
      reliability_(reliability_type_e::RT_UNKNOWN),
      reliability_auto_mode_(false) {
}

eventgroupinfo::~eventgroupinfo() {
}

service_t eventgroupinfo::get_service() const {
    return service_;
}

void eventgroupinfo::set_service(const service_t _service) {
    service_ = _service;
}

instance_t eventgroupinfo::get_instance() const {
    return instance_;
}

void eventgroupinfo::set_instance(const instance_t _instance) {
    instance_ = _instance;
}

eventgroup_t eventgroupinfo::get_eventgroup() const {
    return eventgroup_;
}

void eventgroupinfo::set_eventgroup(const eventgroup_t _eventgroup) {
    eventgroup_ = _eventgroup;
}

major_version_t eventgroupinfo::get_major() const {
    return major_;
}

void eventgroupinfo::set_major(const major_version_t _major) {
    major_ = _major;
}

ttl_t eventgroupinfo::get_ttl() const {
    return ttl_;
}

void eventgroupinfo::set_ttl(const ttl_t _ttl) {
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

void eventgroupinfo::add_event(const std::shared_ptr<event>& _event) {
    std::lock_guard<std::mutex> its_lock(events_mutex_);
    events_.insert(_event);

    if (!reliability_auto_mode_ &&
            _event->get_reliability() == reliability_type_e::RT_UNKNOWN) {
        reliability_auto_mode_ = true;
        return;
    }

    switch (_event->get_reliability()) {
    case reliability_type_e::RT_RELIABLE:
        if (reliability_ == reliability_type_e::RT_UNRELIABLE) {
            reliability_ = reliability_type_e::RT_BOTH;
        } else if (reliability_ != reliability_type_e::RT_BOTH) {
            reliability_ = reliability_type_e::RT_RELIABLE;
        }
        break;
    case reliability_type_e::RT_UNRELIABLE:
        if (reliability_ == reliability_type_e::RT_RELIABLE) {
            reliability_ = reliability_type_e::RT_BOTH;
        } else if (reliability_ != reliability_type_e::RT_BOTH) {
            reliability_ = reliability_type_e::RT_UNRELIABLE;
        }
        break;
    case reliability_type_e::RT_BOTH:
        reliability_ = reliability_type_e::RT_BOTH;
        break;
    default:
        ;
    }
}

void eventgroupinfo::remove_event(const std::shared_ptr<event>& _event) {
    std::lock_guard<std::mutex> its_lock(events_mutex_);
    events_.erase(_event);
}

reliability_type_e eventgroupinfo::get_reliability() const {
    return reliability_;
}

void eventgroupinfo::set_reliability(reliability_type_e _reliability) {
    reliability_ = _reliability;
}

bool eventgroupinfo::is_reliability_auto_mode() const {
    return reliability_auto_mode_;
}

uint32_t
eventgroupinfo::get_unreliable_target_count() const {
    uint32_t its_count(0);

    std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);
    for (const auto &s : subscriptions_) {
        auto its_subscription = s.second;
        if (!its_subscription->get_parent()
                && its_subscription->get_unreliable()) {
            its_count++;
        }
    }

    return its_count;
}

uint8_t eventgroupinfo::get_threshold() const {
    return threshold_;
}

void eventgroupinfo::set_threshold(uint8_t _threshold) {
    threshold_ = _threshold;
}

std::set<std::shared_ptr<remote_subscription> >
eventgroupinfo::get_remote_subscriptions() const {
    std::set<std::shared_ptr<remote_subscription> > its_subscriptions;

    std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);
    for (const auto &i : subscriptions_)
        its_subscriptions.insert(i.second);

    return its_subscriptions;
}

bool
eventgroupinfo::update_remote_subscription(
        const std::shared_ptr<remote_subscription> &_subscription,
        const std::chrono::steady_clock::time_point &_expiration,
        std::set<client_t> &_changed, remote_subscription_id_t &_id,
        const bool _is_subscribe) {
    std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);

    for (const auto& its_item : subscriptions_) {
        if (its_item.second->equals(_subscription)) {
            // update existing subscription
            _changed = its_item.second->update(
                _subscription->get_clients(), _expiration, _is_subscribe);
            _id = its_item.second->get_id();

            // Copy acknowledgment states from existing subscription
            for (const auto& its_client : _subscription->get_clients()) {
                _subscription->set_client_state(its_client,
                        its_item.second->get_client_state(its_client));
            }

            if (_is_subscribe) {
                if (!_changed.empty()) {
                    // New clients:
                    // Let this be a child subscription
                    _subscription->set_parent(its_item.second);
                    update_id();
                    _subscription->set_id(id_);
                    subscriptions_[id_] = _subscription;
                } else {
                    if (!_subscription->is_pending()) {
                        if (!_subscription->force_initial_events()) {
                            _subscription->set_initial(false);
                        }
                    } else {
                        its_item.second->set_answers(
                                its_item.second->get_answers() + 1);
                        _subscription->set_parent(its_item.second);
                        _subscription->set_answers(0);
                    }
                }
            } else {
                if (its_item.second->is_pending()) {
                    for (const auto &its_event : events_)
                        its_event->remove_pending(
                                its_item.second->get_subscriber());
                }
            }

            return true;
        }
    }

    return false;
}

remote_subscription_id_t
eventgroupinfo::add_remote_subscription(
        const std::shared_ptr<remote_subscription> &_subscription) {
    std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);
    update_id();

    _subscription->set_id(id_);
    subscriptions_[id_] = _subscription;

    return id_;
}

std::shared_ptr<remote_subscription>
eventgroupinfo::get_remote_subscription(
        const remote_subscription_id_t _id) {
    std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);

    auto find_subscription = subscriptions_.find(_id);
    if (find_subscription != subscriptions_.end())
        return find_subscription->second;

    return nullptr;
}

void
eventgroupinfo::remove_remote_subscription(
        const remote_subscription_id_t _id) {
    std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);
    subscriptions_.erase(_id);
}

void
eventgroupinfo::clear_remote_subscriptions() {
    std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);
    subscriptions_.clear();
}

std::set<std::shared_ptr<endpoint_definition> >
eventgroupinfo::get_unicast_targets() const {
    std::set<std::shared_ptr<endpoint_definition>> its_targets;

    std::lock_guard<std::mutex> its_lock(subscriptions_mutex_);
    for (const auto &s : subscriptions_) {
        const auto its_reliable = s.second->get_reliable();
        if (its_reliable)
            its_targets.insert(its_reliable);
        const auto its_unreliable = s.second->get_unreliable();
        if (its_unreliable)
            its_targets.insert(its_unreliable);
    }

    return its_targets;
}

std::set<std::shared_ptr<endpoint_definition> >
eventgroupinfo::get_multicast_targets() const {
    std::set<std::shared_ptr<endpoint_definition>> its_targets;
    return its_targets;
}

bool eventgroupinfo::is_selective() const {
    // Selective eventgroups always contain a single event
    std::lock_guard<std::mutex> its_lock(events_mutex_);
    if (events_.size() != 1)
        return false;

    return ((*events_.begin())->get_type()
            == event_type_e::ET_SELECTIVE_EVENT);
}

void
eventgroupinfo::update_id() {
    id_++;
    if (id_ == PENDING_SUBSCRIPTION_ID)
        id_ = 1;
}

void
eventgroupinfo::send_initial_events(
        const std::shared_ptr<endpoint_definition> &_reliable,
        const std::shared_ptr<endpoint_definition> &_unreliable) const {

    std::set<std::shared_ptr<event> > its_reliable_events, its_unreliable_events;

    // Build sets of reliable/unreliable events first to avoid having to
    // hold the "events_mutex_" in parallel to the internal event mutexes.
    {
        std::lock_guard<std::mutex> its_lock(events_mutex_);
        for (const auto &its_event : events_) {
            if (its_event && its_event->get_type() == event_type_e::ET_FIELD) {
                auto its_reliability = its_event->get_reliability();
#ifdef VSOMEIP_ENABLE_COMPAT
                if (its_reliability == reliability_type_e::RT_UNKNOWN) {
                    if (_reliable) {
                        if (_unreliable) {
                            its_reliability = reliability_type_e::RT_BOTH;
                        } else {
                            its_reliability = reliability_type_e::RT_RELIABLE;
                        }
                    } else if (_unreliable) {
                        its_reliability = reliability_type_e::RT_UNRELIABLE;
                    }
                }
#endif
                switch (its_reliability) {
                case reliability_type_e::RT_RELIABLE:
                    its_reliable_events.insert(its_event);
                    break;
                case reliability_type_e::RT_UNRELIABLE:
                    its_unreliable_events.insert(its_event);
                    break;
                case reliability_type_e::RT_BOTH:
                    its_reliable_events.insert(its_event);
                    its_unreliable_events.insert(its_event);
                    break;
                default:
                    VSOMEIP_WARNING << __func__ << "Event reliability unknown: ["
                        << std::hex << std::setw(4) << std::setfill('0') << service_ << "."
                        << std::hex << std::setw(4) << std::setfill('0') << instance_ << "."
                        << std::hex << std::setw(4) << std::setfill('0') << eventgroup_ << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_event->get_event() << "]";
                }
            }
        }
    }

    // Send events
    for (const auto& its_event : its_reliable_events)
        its_event->notify_one(VSOMEIP_ROUTING_CLIENT, _reliable);

    for (const auto& its_event : its_unreliable_events)
        its_event->notify_one(VSOMEIP_ROUTING_CLIENT, _unreliable);
}

}  // namespace vsomeip_v3
