// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>

#include <vsomeip/constants.hpp>


#include "../include/eventgroupinfo.hpp"
#include "../include/event.hpp"
#include "../../endpoints/include/endpoint_definition.hpp"
#include "../../logging/include/logger.hpp"
#include "../../configuration/include/internal.hpp"

namespace vsomeip {

eventgroupinfo::eventgroupinfo() :
        major_(DEFAULT_MAJOR),
        ttl_(DEFAULT_TTL),
        port_(ILLEGAL_PORT),
        threshold_(0),
        has_reliable_(false),
        has_unreliable_(false),
        subscription_id_(DEFAULT_SUBSCRIPTION) {
}

eventgroupinfo::eventgroupinfo(major_version_t _major, ttl_t _ttl) :
        major_(_major),
        ttl_(_ttl),
        port_(ILLEGAL_PORT),
        threshold_(0),
        has_reliable_(false),
        has_unreliable_(false),
        subscription_id_(DEFAULT_SUBSCRIPTION) {
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
    _event->is_reliable() ? has_reliable_ = true : has_unreliable_ = true;
}

void eventgroupinfo::remove_event(std::shared_ptr<event> _event) {
    std::lock_guard<std::mutex> its_lock(events_mutex_);
    events_.erase(_event);
}

void eventgroupinfo::get_reliability(bool& _has_reliable, bool& _has_unreliable) const {
    _has_reliable = has_reliable_;
    _has_unreliable = has_unreliable_;
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
    bool found(false);
    bool add(false);
    bool ret(false);
    {
        std::lock_guard<std::mutex> its_lock(targets_mutex_);
        std::size_t its_size = targets_.size();

        for (auto i = targets_.begin(); i != targets_.end(); i++) {
            if (i->endpoint_->get_address() == _subscriber.endpoint_->get_address() &&
                    i->endpoint_->get_port() == _subscriber.endpoint_->get_port() &&
                    i->endpoint_->is_reliable() == _subscriber.endpoint_->is_reliable()) {
                found = true;
                break;
            }
        }

        if (!found) {
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
    bool updated_target(false);

    for (auto i = targets_.begin(); i != targets_.end(); i++) {
        if (i->endpoint_->get_address() == _target->get_address() &&
                i->endpoint_->get_port() == _target->get_port() &&
                i->endpoint_->is_reliable() == _target->is_reliable() ) {
            i->expiration_ = _expiration;
            updated_target = true;
            break;
        }
    }
    return updated_target;
}

bool eventgroupinfo::remove_target(
        const std::shared_ptr<endpoint_definition> &_target) {
    std::lock_guard<std::mutex> its_lock(targets_mutex_);
    std::size_t its_size = targets_.size();

    for (auto i = targets_.begin(); i != targets_.end(); i++) {
        if (i->endpoint_->get_address() == _target->get_address() &&
                      i->endpoint_->get_port() == _target->get_port() &&
                      i->endpoint_->is_reliable() == _target->is_reliable()) {
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

pending_subscription_id_t eventgroupinfo::add_pending_subscription(
        pending_subscription_t _pending_subscription) {
    std::lock_guard<std::mutex> its_lock(pending_subscriptions_mutex_);
    if (++subscription_id_ == DEFAULT_SUBSCRIPTION) {
        subscription_id_++;
    }
    _pending_subscription.pending_subscription_id_ = subscription_id_;
    pending_subscriptions_[subscription_id_] = _pending_subscription;

    const auto remote_address_port = std::make_tuple(
            _pending_subscription.subscriber_->get_address(),
            _pending_subscription.subscriber_->get_port(),
            _pending_subscription.subscriber_->is_reliable());

    auto found_address = pending_subscriptions_by_remote_.find(remote_address_port);
    if (found_address != pending_subscriptions_by_remote_.end()) {
        found_address->second.push_back(subscription_id_);
        VSOMEIP_WARNING << __func__ << " num pending subscriptions: "
                << std::dec << found_address->second.size();
        return DEFAULT_SUBSCRIPTION;
    } else {
        pending_subscriptions_by_remote_[remote_address_port].push_back(subscription_id_);
    }
    return subscription_id_;
}

std::vector<pending_subscription_t> eventgroupinfo::remove_pending_subscription(
        pending_subscription_id_t _subscription_id) {
    std::vector<pending_subscription_t> its_pending_subscriptions;
    std::lock_guard<std::mutex> its_lock(pending_subscriptions_mutex_);
    const auto found_pending_subscription = pending_subscriptions_.find(
            _subscription_id);
    if (found_pending_subscription != pending_subscriptions_.end()) {
        pending_subscription_t its_pending_sub = found_pending_subscription->second;
        const auto remote_address_port = std::make_tuple(
                its_pending_sub.subscriber_->get_address(),
                its_pending_sub.subscriber_->get_port(),
                its_pending_sub.subscriber_->is_reliable());
        const bool removed_is_subscribe = (found_pending_subscription->second.ttl_ > 0);

        // check if more (un)subscriptions to this eventgroup arrived from the
        //  same remote during the time the current pending subscription was processed
        auto found_remote = pending_subscriptions_by_remote_.find(remote_address_port);
        if (found_remote != pending_subscriptions_by_remote_.end()) {
            if (found_remote->second.size()
                    && found_remote->second.front() == _subscription_id) {
                pending_subscriptions_.erase(found_pending_subscription);
                found_remote->second.erase(found_remote->second.begin());

                // return removed (un)subscription as first element
                its_pending_subscriptions.push_back(its_pending_sub);

                // retrieve all pending (un)subscriptions which arrived during
                // the time the rm_proxy answered the currently processed subscription
                for (auto iter = found_remote->second.begin();
                        iter != found_remote->second.end();) {
                    const auto other_pen_sub = pending_subscriptions_.find(*iter);
                    if (other_pen_sub != pending_subscriptions_.end()) {
                        const bool queued_is_subscribe = (other_pen_sub->second.ttl_ > 0);
                        if (removed_is_subscribe) {
                            its_pending_subscriptions.push_back(other_pen_sub->second);
                            if (!queued_is_subscribe) {
                                // unsubscribe was queued and needs to be sent to
                                // rm_proxy first before continuing processing
                                // following queued (un)subscriptions
                                break;
                            } else {
                                iter = found_remote->second.erase(iter);
                                pending_subscriptions_.erase(other_pen_sub);
                            }
                        } else {
                            if (queued_is_subscribe) {
                                // subscribe was queued and needs to be sent to
                                // rm_proxy first before continuing processing
                                // following queued (un)subscriptions
                                its_pending_subscriptions.push_back(other_pen_sub->second);
                                break;
                            } else {
                                // further queued unsubscriptions can be ignored
                                iter = found_remote->second.erase(iter);
                                pending_subscriptions_.erase(other_pen_sub);
                            }
                        }
                    } else {
                        VSOMEIP_ERROR << __func__ << " didn't find queued subscription: "
                                << *iter;
                        ++iter;
                    }
                }

                if (found_remote->second.empty()) {
                    pending_subscriptions_by_remote_.erase(found_remote);
                }
            } else {
                boost::system::error_code ec;
                VSOMEIP_WARNING << __func__ << " Subscriptions were answered in "
                        << " in wrong order by rm_proxy! ["
                        << " subscriber: " << std::get<0>(remote_address_port).to_string(ec)
                        << ":" << std::dec << std::get<1>(remote_address_port);
                // found_pending_subscription isn't deleted from
                // pending_subscriptions_ map in this case to ensure answer
                // sequence of SD messages.
                its_pending_subscriptions.clear();
            }
        }
    } else {
        VSOMEIP_ERROR << __func__ << " didn't find pending_subscription: "
                << _subscription_id;
    }
    return its_pending_subscriptions;
}


void eventgroupinfo::clear_pending_subscriptions() {
    std::lock_guard<std::mutex> its_lock(pending_subscriptions_mutex_);
    pending_subscriptions_.clear();
    pending_subscriptions_by_remote_.clear();
}

}  // namespace vsomeip
