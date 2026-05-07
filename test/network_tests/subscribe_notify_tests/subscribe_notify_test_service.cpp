// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <algorithm>
#include <atomic>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "subscribe_notify_test_globals.hpp"
#include "../someip_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>
#include "common/test_main.hpp"

class subscribe_notify_test_service {
public:
    subscribe_notify_test_service(struct subscribe_notify_test::service_info _service_info,
                                  std::array<subscribe_notify_test::service_info, 7> _service_infos,
                                  vsomeip::reliability_type_e _reliability_type, bool _use_tcp) :
        service_info_(_service_info), service_infos_(_service_infos), app_(vsomeip::runtime::get()->create_application()),
        wait_until_registered_(true), wait_until_other_services_available_(true), wait_until_notified_from_other_services_(true),
        offer_thread_(std::bind(&subscribe_notify_test_service::run, this)), wait_for_stop_(true),
        stop_thread_(std::bind(&subscribe_notify_test_service::wait_for_stop, this)), all_requests_received_(false),
        notify_thread_(std::bind(&subscribe_notify_test_service::notify, this)), expected_requests_(0), requests_received_(0),
        subscription_error_occured_(false), reliability_type_(_reliability_type), use_tcp_(_use_tcp) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }

        app_->register_state_handler(std::bind(&subscribe_notify_test_service::on_state, this, std::placeholders::_1));
        app_->register_message_handler(service_info_.service_id, service_info_.instance_id, service_info_.method_id,
                                       std::bind(&subscribe_notify_test_service::on_request, this, std::placeholders::_1));
        app_->register_message_handler(vsomeip::ANY_SERVICE, vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD,
                                       std::bind(&subscribe_notify_test_service::on_message, this, std::placeholders::_1));

        // offer event
        std::set<vsomeip::eventgroup_t> its_eventgroups;
        its_eventgroups.insert(service_info_.eventgroup_id);
        app_->offer_event(service_info_.service_id, service_info_.instance_id, service_info_.event_id, its_eventgroups,
                          vsomeip::event_type_e::ET_FIELD, std::chrono::milliseconds::zero(), false, true, nullptr, reliability_type_);

        // register availability for all other services and request their event.
        for (const auto& i : service_infos_) {
            if ((i.service_id == service_info_.service_id && i.instance_id == service_info_.instance_id)
                || (i.service_id == 0xFFFF && i.instance_id == 0xFFFF)) {
                continue;
            }
            app_->request_service(i.service_id, i.instance_id);
            app_->register_availability_handler(i.service_id, i.instance_id,
                                                std::bind(&subscribe_notify_test_service::on_availability, this, std::placeholders::_1,
                                                          std::placeholders::_2, std::placeholders::_3));

            auto handler = std::bind(&subscribe_notify_test_service::on_subscription_state_change, this, std::placeholders::_1,
                                     std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
            app_->register_subscription_status_handler(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::ANY_EVENT, handler);

            std::set<vsomeip::eventgroup_t> its_eventgroups;
            its_eventgroups.insert(i.eventgroup_id);
            app_->request_event(i.service_id, i.instance_id, i.event_id, its_eventgroups, vsomeip::event_type_e::ET_FIELD,
                                reliability_type_);

            other_services_available_[std::make_pair(i.service_id, i.instance_id)] = false;
            other_services_received_notification_[std::make_pair(i.service_id, i.method_id)] = 0;
            ++expected_requests_;
        }

        // register subscription handler to detect whether or not all other
        // other services have subscribed
        app_->register_subscription_handler(service_info_.service_id, service_info_.instance_id, service_info_.eventgroup_id,
                                            std::bind(&subscribe_notify_test_service::on_subscription, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

        app_->start();
    }

    ~subscribe_notify_test_service() {
        offer_thread_.join();
        stop_thread_.join();
    }

    void offer() { app_->offer_service(service_info_.service_id, service_info_.instance_id); }

    void stop_offer() {
        app_->stop_offer_event(service_info_.service_id, service_info_.instance_id, service_info_.event_id);
        app_->stop_offer_service(service_info_.service_id, service_info_.instance_id);
    }

    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
                     << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.");

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            std::scoped_lock its_lock(mutex_);
            wait_until_registered_ = false;
            condition_.notify_one();
        }
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        if (_is_available) {
            auto its_service = other_services_available_.find(std::make_pair(_service, _instance));
            if (its_service != other_services_available_.end()) {
                if (its_service->second != _is_available) {
                    its_service->second = true;
                    VSOMEIP_INFO << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << ": Service ["
                                 << std::setw(4) << _service << "." << _instance << "] is available.";
                }
            }

            if (std::all_of(
                        other_services_available_.cbegin(), other_services_available_.cend(),
                        [](const std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, bool>::value_type& v) { return v.second; })) {
                std::scoped_lock its_lock(mutex_);
                wait_until_other_services_available_ = false;
                condition_.notify_one();
            }
        }
    }

    void on_subscription_state_change(const vsomeip::service_t _service, const vsomeip::instance_t _instance,
                                      const vsomeip::eventgroup_t _eventgroup, const vsomeip::event_t _event, const uint16_t _error) {
        vsomeip::method_t request_method = vsomeip::ANY_METHOD;

        std::scoped_lock its_lock(subscription_state_handler_called_mutex_);
        if (!_error) {
            if ((_service == service_info_.service_id && _instance == service_info_.instance_id)
                || (_service == 0xFFFF && _instance == 0xFFFF)) {
                return;
            }

            const auto peer_it = std::find_if(service_infos_.begin(), service_infos_.end(),
                                              [=](const auto& s) { return s.service_id == _service && s.instance_id == _instance; });
            if (peer_it == service_infos_.end()) {
                return;
            }

            subscription_state_handler_called_.insert(std::make_tuple(_service, _instance, _eventgroup));

            request_method = peer_it->method_id;

            VSOMEIP_INFO << "[" << std::hex << service_info_.service_id << "] subscription state OK for [" << _service << "." << _instance
                         << "." << _eventgroup << "." << _event << "]";
        } else {
            subscription_error_occured_ = true;
            VSOMEIP_WARNING << std::hex << service_info_.service_id << " : on_subscription_state_change: for service " << std::hex
                            << _service << "." << _instance << "." << _eventgroup << "." << _event << " received a subscription error!";
        }

        // Trigger request when subscription ACK is received.
        if (request_method != vsomeip::ANY_METHOD) {
            std::shared_ptr<vsomeip::message> its_req = vsomeip::runtime::get()->create_request(use_tcp_);
            its_req->set_service(_service);
            its_req->set_instance(_instance);
            its_req->set_method(request_method);
            app_->send(its_req);
        }
    }

    bool on_subscription(vsomeip::client_t _client, std::uint32_t _uid, std::uint32_t _gid, bool _subscribed) {
        (void)_uid;
        (void)_gid;
        std::scoped_lock its_lock(subscribers_mutex_);
        if (_subscribed) {
            subscribers_.insert(_client);
        } else {
            subscribers_.erase(_client);
        }

        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] "
                      << "Client: " << std::setw(4) << _client << (_subscribed ? " subscribed" : " unsubscribed") << ", now have "
                      << std::dec << subscribers_.size() << " subscribers";

        return true;
    }

    void on_request(const std::shared_ptr<vsomeip::message>& _message) {
        if (_message->get_message_type() == vsomeip::message_type_e::MT_REQUEST) {
            VSOMEIP_DEBUG << "Received a request with Client/Session [" << std::hex << std::setfill('0') << std::setw(4)
                          << _message->get_client() << "/" << std::setw(4) << _message->get_session() << "]";
            ++requests_received_;

            if (requests_received_ == expected_requests_) {
                std::scoped_lock its_lock(notify_mutex_);
                all_requests_received_ = true;
                notify_condition_.notify_one();
            }
            VSOMEIP_INFO << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << std::dec
                         << "] Received requests: " << requests_received_ << "/" << expected_requests_;
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message>& _message) {
        if (_message->get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {
            // another dispatcher might be currently evaluating whether all notifications are received
            std::unique_lock lock{mutex_};
            other_services_received_notification_[std::make_pair(_message->get_service(), _message->get_method())]++;

            VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] "
                          << "Received a notification with Client/Session [" << std::setw(4) << _message->get_client() << "/"
                          << std::setw(4) << _message->get_session() << "] from Service/Method [" << std::setw(4) << _message->get_service()
                          << "/" << std::setw(4) << _message->get_method() << "/" << std::dec << _message->get_length() << "] (now have: "
                          << other_services_received_notification_[std::make_pair(_message->get_service(), _message->get_method())] << ")";

            if (all_notifications_received()) {
                lock.unlock();
                std::scoped_lock its_lock(stop_mutex_);
                wait_for_stop_ = false;
                stop_condition_.notify_one();
            }
        }
    }

    bool all_notifications_received() {
        return std::all_of(other_services_received_notification_.cbegin(), other_services_received_notification_.cend(),
                           [&](const std::map<std::pair<vsomeip::service_t, vsomeip::method_t>, std::uint32_t>::value_type& v) {
                               return v.second >= subscribe_notify_test::notifications_to_send;
                           });
    }

    void run() {
        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Running";
        std::unique_lock its_lock(mutex_);
        condition_.wait(its_lock, [this] { return !wait_until_registered_; });

        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Offering";
        offer();

        condition_.wait(its_lock, [this] { return !wait_until_other_services_available_; });

        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Subscribing";
        // subscribe to events of other services
        uint32_t subscribe_count = 0;
        for (const subscribe_notify_test::service_info& i : service_infos_) {
            if ((i.service_id == service_info_.service_id && i.instance_id == service_info_.instance_id)
                || (i.service_id == 0xFFFF && i.instance_id == 0xFFFF)) {
                continue;
            }
            ++subscribe_count;
            app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::DEFAULT_MAJOR);
            VSOMEIP_DEBUG << "[" << std::hex << service_info_.service_id << "] subscribing to Service/Instance/Eventgroup [" << std::hex
                          << std::setfill('0') << std::setw(4) << i.service_id << "/" << std::setw(4) << i.instance_id << "/"
                          << std::setw(4) << i.eventgroup_id << "]";
        }

        condition_.wait(its_lock, [this] { return !wait_until_notified_from_other_services_; });

        // It is possible that we run in the case a subscription is NACKED
        // due to TCP endpoint not completely connected when subscription
        // is processed in the server - due to resubscribing the error handler
        // count may differ from expected value, but its not a real but as
        // the subscription takes places anyways and all events will be received.
        std::scoped_lock its_subscription_lock(subscription_state_handler_called_mutex_);
        if (!subscription_error_occured_) {
            ASSERT_EQ(subscribe_count, subscription_state_handler_called_.size());
        } else {
            VSOMEIP_WARNING << "Subscription state handler check skipped: CallCount=" << std::dec
                            << subscription_state_handler_called_.size();
        }
    }

    void notify() {
        // Requests are sent after subscription acks are received.
        // Wait until all peers have requested before sending notifications.
        std::unique_lock its_lock(notify_mutex_);
        notify_condition_.wait(its_lock, [this] { return all_requests_received_; });

        VSOMEIP_INFO << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Starting to notify";

        for (uint32_t i = 0; i < subscribe_notify_test::notifications_to_send; i++) {
            std::shared_ptr<vsomeip::payload> its_payload = vsomeip::runtime::get()->create_payload();

            vsomeip::byte_t its_data[10] = {0};
            for (uint32_t j = 0; j < i + 1; ++j) {
                its_data[j] = static_cast<uint8_t>(j);
            }
            its_payload->set_data(its_data, i + 1);
            VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Notifying: " << i + 1;
            app_->notify(service_info_.service_id, service_info_.instance_id, service_info_.event_id, its_payload);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void wait_for_stop() {
        std::unique_lock its_lock(stop_mutex_);
        stop_condition_.wait(its_lock, [this] { return !wait_for_stop_; });

        // wait until all notifications have been sent out
        notify_thread_.join();

        VSOMEIP_INFO << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                     << "] Received notifications from all other services, going down";

        // let offer thread exit
        {
            std::scoped_lock its_lock(mutex_);
            wait_until_notified_from_other_services_ = false;
            condition_.notify_one();
        }

        stop_offer();

        // ensure that the service which hosts the routing doesn't exit to early
        if (app_->is_routing()) {
            for (const auto& i : service_infos_) {
                if ((i.service_id == service_info_.service_id && i.instance_id == service_info_.instance_id)
                    || (i.service_id == 0xFFFF && i.instance_id == 0xFFFF)) {
                    continue;
                }
                while (app_->is_available(i.service_id, i.instance_id, vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        for (const auto& i : service_infos_) {
            if ((i.service_id == service_info_.service_id && i.instance_id == service_info_.instance_id)
                || (i.service_id == 0xFFFF && i.instance_id == 0xFFFF)) {
                continue;
            }
            app_->unregister_subscription_status_handler(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::ANY_EVENT);
            app_->unsubscribe(i.service_id, i.instance_id, i.eventgroup_id);
            app_->release_event(i.service_id, i.instance_id, i.event_id);
            app_->release_service(i.service_id, i.instance_id);
        }
        app_->clear_all_handler();
        app_->stop();
    }

private:
    subscribe_notify_test::service_info service_info_;
    std::array<subscribe_notify_test::service_info, 7> service_infos_;
    std::shared_ptr<vsomeip::application> app_;
    std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, bool> other_services_available_;
    std::map<std::pair<vsomeip::service_t, vsomeip::method_t>, std::uint32_t> other_services_received_notification_;

    bool wait_until_registered_;
    bool wait_until_other_services_available_;
    bool wait_until_notified_from_other_services_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::thread offer_thread_;

    bool wait_for_stop_;
    std::mutex stop_mutex_;
    std::condition_variable stop_condition_;
    std::thread stop_thread_;

    std::set<vsomeip::client_t> subscribers_;
    bool all_requests_received_;
    std::mutex notify_mutex_;
    std::condition_variable notify_condition_;
    std::thread notify_thread_;
    std::uint32_t expected_requests_;
    std::atomic<std::uint32_t> requests_received_;

    std::mutex subscription_state_handler_called_mutex_;
    bool subscription_error_occured_;
    std::set<std::tuple<vsomeip::service_t, vsomeip::instance_t, vsomeip::eventgroup_t>> subscription_state_handler_called_;

    std::mutex subscribers_mutex_;
    vsomeip::reliability_type_e reliability_type_;
    bool use_tcp_;
};

static unsigned long service_number;
static bool use_same_service_id;
static bool use_tcp = false;

vsomeip::reliability_type_e reliability_type = vsomeip::reliability_type_e::RT_UNKNOWN;

TEST(someip_subscribe_notify_test, send_ten_notifications_to_service) {
    if (use_same_service_id) {
        subscribe_notify_test_service its_sample(subscribe_notify_test::service_infos_same_service_id[service_number],
                                                 subscribe_notify_test::service_infos_same_service_id, reliability_type, use_tcp);
    } else {
        subscribe_notify_test_service its_sample(subscribe_notify_test::service_infos[service_number], subscribe_notify_test::service_infos,
                                                 reliability_type, use_tcp);
    }
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Please specify a service number and event reliability type, like: " << argv[0] << " 2 UDP SAME_SERVICE_ID"
                  << std::endl;
        std::cerr << "Valid service numbers are in the range of [1,6]" << std::endl;
        std::cerr << "Valid event reliability types are [UDP, TCP, TCP_AND_UDP]" << std::endl;
        std::cerr << "If SAME_SERVICE_ID is specified as third parameter the test is run w/ "
                     "multiple instances of the same service"
                  << std::endl;
        return 1;
    }

    service_number = std::stoul(std::string(argv[1]), nullptr);

    if (argc >= 3) {
        if (std::string("TCP") == std::string(argv[2])) {
            reliability_type = vsomeip::reliability_type_e::RT_RELIABLE;
            use_tcp = true;
        } else if (std::string("UDP") == std::string(argv[2])) {
            reliability_type = vsomeip::reliability_type_e::RT_UNRELIABLE;
            use_tcp = false;
        } else if (std::string("TCP_AND_UDP") == std::string(argv[2])) {
            use_tcp = true;
            reliability_type = vsomeip::reliability_type_e::RT_BOTH;
        }
    }

    if (argc >= 4 && std::string("SAME_SERVICE_ID") == std::string(argv[3])) {
        use_same_service_id = true;
    } else {
        use_same_service_id = false;
    }

    return test_main(argc, argv);
}
#endif
