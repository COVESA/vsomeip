// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <atomic>
#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include <boost/asio/ip/address.hpp>

#include <vsomeip/primitive_types.hpp>

#include "remote_subscription.hpp"
#include "types.hpp"

#if defined(__QNX__)
#include "../../utility/include/qnx_helper.hpp"
#endif

namespace vsomeip_v3 {

class endpoint_definition;
class event;

class eventgroupinfo {
public:
    struct subscription_t {
        std::shared_ptr<remote_subscription> subscription_;
        std::chrono::steady_clock::time_point expiration_;

        bool operator==(const subscription_t& _other) const { return (subscription_ == _other.subscription_); }
    };

    eventgroupinfo();
    eventgroupinfo(const service_t _service, const service_t _instance, const eventgroup_t _eventgroup, const major_version_t _major,
                   const ttl_t _ttl, const uint8_t _max_remote_subscribers);
    ~eventgroupinfo();

    service_t get_service() const;
    void set_service(const service_t _service);

    instance_t get_instance() const;
    void set_instance(const instance_t _instance);

    eventgroup_t get_eventgroup() const;
    void set_eventgroup(const eventgroup_t _eventgroup);

    major_version_t get_major() const;
    void set_major(const major_version_t _major);

    ttl_t get_ttl() const;
    void set_ttl(const ttl_t _ttl);

    bool is_multicast() const;
    bool get_multicast(boost::asio::ip::address& _address, uint16_t& _port) const;
    void set_multicast(const boost::asio::ip::address& _address, uint16_t _port);
    bool is_sending_multicast() const;

    std::set<std::shared_ptr<event>> get_events() const;
    void add_event(const std::shared_ptr<event>& _event);
    void remove_event(const std::shared_ptr<event>& _event);
    reliability_type_e get_reliability() const;
    void set_reliability(reliability_type_e _reliability);
    bool is_reliability_auto_mode() const;

    /// Returns the `reliability_type_e` of events that are provided by this host.
    ///
    /// This is used when checking if a subscription should be accepted or not.
    reliability_type_e get_provider_reliability() const;

    std::set<std::shared_ptr<remote_subscription>> get_remote_subscriptions() const;

    std::shared_ptr<remote_subscription> get_remote_subscription(const remote_subscription_id_t _id);

    bool update_remote_subscription(const std::shared_ptr<remote_subscription>& _subscription,
                                    const std::chrono::steady_clock::time_point& _expiration, std::set<client_t>& _changed,
                                    remote_subscription_id_t& _id, const bool _is_subscribe);

    bool is_remote_subscription_limit_reached(const std::shared_ptr<remote_subscription>& _subscription);

    remote_subscription_id_t add_remote_subscription(const std::shared_ptr<remote_subscription>& _subscription);

    void remove_remote_subscription(const remote_subscription_id_t _id);

    void clear_remote_subscriptions();

    std::set<std::shared_ptr<endpoint_definition>> get_unicast_targets() const;
    std::set<std::shared_ptr<endpoint_definition>> get_multicast_targets() const;

    uint8_t get_threshold() const;
    void set_threshold(uint8_t _threshold);

    bool is_selective() const;

    void send_initial_events(const std::shared_ptr<endpoint_definition>& _reliable,
                             const std::shared_ptr<endpoint_definition>& _unreliable) const;

    uint8_t get_max_remote_subscribers() const;
    void set_max_remote_subscribers(uint8_t _max_remote_subscribers);

private:
    void update_id();
    uint32_t get_unreliable_target_count() const;

    std::atomic<service_t> service_;
    std::atomic<instance_t> instance_;
    std::atomic<eventgroup_t> eventgroup_;
    std::atomic<major_version_t> major_;
    std::atomic<ttl_t> ttl_;

    mutable std::mutex address_mutex_;
    boost::asio::ip::address address_;
    uint16_t port_;

    mutable std::mutex events_mutex_;
    std::set<std::shared_ptr<event>> events_;

    std::atomic<uint8_t> threshold_;

    mutable std::mutex subscriptions_mutex_;
    std::map<remote_subscription_id_t, std::shared_ptr<remote_subscription>> subscriptions_;
    remote_subscription_id_t id_;
    std::map<boost::asio::ip::address, uint8_t> remote_subscribers_count_;

    std::atomic<reliability_type_e> reliability_;
    std::atomic<bool> reliability_auto_mode_;

    uint8_t max_remote_subscribers_;
};

} // namespace vsomeip_v3
