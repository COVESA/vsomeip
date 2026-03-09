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
#include <unordered_set>
#include <atomic>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "subscribe_notify_one_test_globals.hpp"
#include "../someip_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>
#include "common/test_main.hpp"

class subscribe_notify_one_test_service {
public:
    subscribe_notify_one_test_service(struct subscribe_notify_one_test::service_info _service_info,
                                      vsomeip::reliability_type_e _reliability_type, bool _is_tcp) :
        service_info_(_service_info), app_(vsomeip::runtime::get()->create_application()), wait_until_registered_(true),
        wait_until_other_services_available_(true), wait_until_notified_from_other_services_(true),
        offer_thread_(std::bind(&subscribe_notify_one_test_service::run, this)), wait_for_stop_(true),
        stop_thread_(std::bind(&subscribe_notify_one_test_service::wait_for_stop, this)), wait_for_notify_(true),
        notify_thread_(std::bind(&subscribe_notify_one_test_service::notify_one, this)), wait_for_shutdown_acks_(true),
        subscription_state_handler_called_(0), subscription_error_occured_(false), reliability_type_(_reliability_type), is_tcp_(_is_tcp) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        app_->register_state_handler(std::bind(&subscribe_notify_one_test_service::on_state, this, std::placeholders::_1));

        // offer event
        std::set<vsomeip::eventgroup_t> its_eventgroups;
        its_eventgroups.insert(service_info_.eventgroup_id);
        app_->offer_event(service_info_.service_id, service_info_.instance_id, service_info_.event_id, its_eventgroups,
                          vsomeip::event_type_e::ET_SELECTIVE_EVENT, std::chrono::milliseconds::zero(), false, true, nullptr,
                          reliability_type_);

        app_->register_message_handler(service_info_.service_id, service_info_.instance_id, service_info_.method_id,
                                       std::bind(&subscribe_notify_one_test_service::on_request, this, std::placeholders::_1));

        // register subscription handler to detect whether or not all other
        // other services have subscribed
        app_->register_subscription_handler(service_info_.service_id, service_info_.instance_id, service_info_.eventgroup_id,
                                            std::bind(&subscribe_notify_one_test_service::on_subscription, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

        // register availability for all other services and request their event.
        for (const auto& i : subscribe_notify_one_test::service_infos) {
            if ((i.service_id == service_info_.service_id && i.instance_id == service_info_.instance_id)
                || (i.service_id == 0xFFFF && i.instance_id == 0xFFFF)) {
                continue;
            }
            app_->register_message_handler(i.service_id, i.instance_id, vsomeip::ANY_METHOD,
                                           std::bind(&subscribe_notify_one_test_service::on_message, this, std::placeholders::_1));
            app_->register_availability_handler(i.service_id, i.instance_id,
                                                std::bind(&subscribe_notify_one_test_service::on_availability, this, std::placeholders::_1,
                                                          std::placeholders::_2, std::placeholders::_3));

            app_->request_service(i.service_id, i.instance_id, vsomeip::DEFAULT_MAJOR, vsomeip::DEFAULT_MINOR);

            auto handler = std::bind(&subscribe_notify_one_test_service::on_subscription_state_change, this, std::placeholders::_1,
                                     std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
            app_->register_subscription_status_handler(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::ANY_EVENT, handler);
            app_->register_subscription_status_handler(vsomeip::ANY_SERVICE, i.instance_id, i.eventgroup_id, vsomeip::ANY_EVENT, handler);
            app_->register_subscription_status_handler(i.service_id, vsomeip::ANY_INSTANCE, i.eventgroup_id, vsomeip::ANY_EVENT, handler);
            app_->register_subscription_status_handler(vsomeip::ANY_SERVICE, vsomeip::ANY_INSTANCE, i.eventgroup_id, vsomeip::ANY_EVENT,
                                                       handler);

            std::set<vsomeip::eventgroup_t> its_eventgroups;
            its_eventgroups.insert(i.eventgroup_id);
            app_->request_event(i.service_id, i.instance_id, i.event_id, its_eventgroups, vsomeip::event_type_e::ET_SELECTIVE_EVENT,
                                reliability_type_);

            other_services_available_[std::make_pair(i.service_id, i.instance_id)] = false;
            other_services_received_notification_[std::make_pair(i.service_id, i.method_id)] = 0;
            other_services_shutdown_ack_[std::make_pair(i.service_id, i.instance_id)] = false;
        }

        // Register handler for shutdown coordination messages on our own service
        app_->register_message_handler(service_info_.service_id, service_info_.instance_id, 0x0999,
                                       std::bind(&subscribe_notify_one_test_service::on_shutdown_message, this, std::placeholders::_1));

        app_->start();
    }

    ~subscribe_notify_one_test_service() {
        offer_thread_.join();
        stop_thread_.join();
    }

    void offer() { app_->offer_service(service_info_.service_id, service_info_.instance_id); }

    void stop_offer() {
        app_->stop_offer_event(service_info_.service_id, service_info_.instance_id, service_info_.event_id);
        app_->stop_offer_service(service_info_.service_id, service_info_.instance_id);
    }

    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_DEBUG << "Application " << app_->get_name() << " is "
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
                    VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Service ["
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
        (void)_service;
        (void)_instance;
        (void)_eventgroup;
        (void)_event;
        if (!_error) {
            subscription_state_handler_called_++;
        } else {
            subscription_error_occured_ = true;
            VSOMEIP_ERROR << std::hex << app_->get_client() << " : on_subscription_state_change: for service " << std::hex << _service
                          << " received a subscription error!";
        }
    }

    bool on_subscription(vsomeip::client_t _client, std::uint32_t _uid, std::uint32_t _gid, bool _subscribed) {
        (void)_uid;
        (void)_gid;
        std::scoped_lock its_subscribers_lock(subscribers_mutex_);

        // check if all other services have subscribed:
        // -1 for placeholder in array and -1 for the service itself
        if (subscribers_.size() == subscribe_notify_one_test::service_infos.size() - 2) {
            return true;
        }

        if (_subscribed) {
            subscribers_.insert(_client);
        } else {
            subscribers_.erase(_client);
        }

        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] "
                      << "Client: " << _client << " subscribed, now have " << std::dec << subscribers_.size() << " subscribers. Expecting "
                      << subscribe_notify_one_test::service_infos.size() - 2;

        if (subscribers_.size() == subscribe_notify_one_test::service_infos.size() - 2) {
            // notify the notify thread to start sending out notifications
            std::scoped_lock its_lock(notify_mutex_);
            wait_for_notify_ = false;
            notify_condition_.notify_one();
        }
        return true;
    }

    void on_request(const std::shared_ptr<vsomeip::message>& _message) {
        if (_message->get_message_type() == vsomeip::message_type_e::MT_REQUEST) {
            VSOMEIP_DEBUG << "Received a request with Client/Session [" << std::hex << std::setfill('0') << std::setw(4)
                          << _message->get_client() << "/" << std::setw(4) << _message->get_session() << "]";
            std::shared_ptr<vsomeip::message> its_response = vsomeip::runtime::get()->create_response(_message);
            app_->send(its_response);
        }
    }

    void on_shutdown_message(const std::shared_ptr<vsomeip::message>& _message) {
        if (_message->get_message_type() == vsomeip::message_type_e::MT_REQUEST) {
            vsomeip::service_t sender_service = _message->get_client();
            vsomeip::instance_t sender_instance = _message->get_instance();

            VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] "
                          << "Received a shutdown coordination message with Client/Session [" << std::setw(4) << service_info_.service_id
                          << "/" << std::setw(4) << _message->get_session() << "] from Service/Method [" << std::setw(4) << sender_service
                          << "/" << std::setw(4) << _message->get_method() << "]";

            std::scoped_lock its_lock(shutdown_mutex_);
            if (other_services_shutdown_ack_[std::make_pair(sender_service, sender_instance)] = true, all_shutdown_acks_received()) {
                wait_for_shutdown_acks_ = false;
                shutdown_condition_.notify_one();
            }
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message>& _message) {
        if (_message->get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {

            other_services_received_notification_[std::make_pair(_message->get_service(), _message->get_method())]++;

            VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] "
                          << "Received a notification with Client/Session [" << std::setw(4) << _message->get_client() << "/"
                          << std::setw(4) << _message->get_session() << "] from Service/Method [" << std::setw(4) << _message->get_service()
                          << "/" << std::setw(4) << _message->get_method() << "] (now have: " << std::dec
                          << other_services_received_notification_[std::make_pair(_message->get_service(), _message->get_method())] << ")";

            if (all_notifications_received()) {
                std::scoped_lock its_lock(stop_mutex_);
                wait_for_stop_ = false;
                stop_condition_.notify_one();
            }
        }
    }

    bool all_notifications_received() {
        return std::all_of(other_services_received_notification_.cbegin(), other_services_received_notification_.cend(),
                           [&](const std::map<std::pair<vsomeip::service_t, vsomeip::method_t>, std::uint32_t>::value_type& v) {
                               return v.second == subscribe_notify_one_test::notifications_to_send;
                           });
    }

    bool all_shutdown_acks_received() {
        return std::all_of(
                other_services_shutdown_ack_.cbegin(), other_services_shutdown_ack_.cend(),
                [&](const std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, bool>::value_type& v) { return v.second; });
    }

    void send_shutdown_messages() {
        for (const auto& i : subscribe_notify_one_test::service_infos) {
            if ((i.service_id == service_info_.service_id && i.instance_id == service_info_.instance_id)
                || (i.service_id == 0xFFFF && i.instance_id == 0xFFFF)) {
                continue;
            }

            std::shared_ptr<vsomeip::message> its_shutdown_message = vsomeip::runtime::get()->create_request();
            its_shutdown_message->set_service(i.service_id);
            its_shutdown_message->set_instance(i.instance_id);
            its_shutdown_message->set_method(0x0999);
            its_shutdown_message->set_reliable(is_tcp_);

            VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                          << "] Sending shutdown coordination message to Service/Instance [" << std::setw(4) << i.service_id << "/"
                          << std::setw(4) << i.instance_id << "]";

            app_->send(its_shutdown_message);
        }
    }

    void run() {
        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Running";
        std::unique_lock<std::mutex> its_lock(mutex_);
        EXPECT_TRUE(condition_.wait_for(its_lock, std::chrono::seconds(5), [this] { return !wait_until_registered_; }))
                << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Service registration timeout";

        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Offering";
        offer();

        EXPECT_TRUE(condition_.wait_for(its_lock, std::chrono::seconds(15), [this] { return !wait_until_other_services_available_; }))
                << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                << "] Other services availability timeout";

        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Subscribing";
        // subscribe to events of other services
        uint32_t subscribe_count = 0;
        for (const subscribe_notify_one_test::service_info& i : subscribe_notify_one_test::service_infos) {
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

        EXPECT_TRUE(condition_.wait_for(its_lock, std::chrono::seconds(15), [this] { return !wait_until_notified_from_other_services_; }))
                << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Notification timeout";

        // It is possible that we run in the case a subscription is NACKED
        // due to TCP endpoint not completely connected when subscription
        // is processed in the server - due to resubscribing the error handler
        // count may differ from expected value, but its not a real but as
        // the subscription takes places anyways and all events will be received.
        if (!subscription_error_occured_) {
            // 4 * subscribe count cause we installed three additional wild-card handlers
            ASSERT_EQ(subscribe_count * 4, subscription_state_handler_called_);
        } else {
            VSOMEIP_ERROR << "Subscription state handler check skipped: CallCount=" << std::dec << subscription_state_handler_called_;
        }
    }

    void notify_one() {
        std::unique_lock<std::mutex> its_lock(notify_mutex_);
        EXPECT_TRUE(notify_condition_.wait_for(its_lock, std::chrono::seconds(15), [this] { return !wait_for_notify_; }))
                << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Notify one timeout";

        // sleep a while before starting to notify this is necessary as it's not
        // possible to detect if _all_ clients on the remote side have
        // successfully subscribed as we only receive once subscription per
        // remote node no matter how many clients subscribed to this eventgroup
        // on the remote node
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        for (uint32_t i = 0; i < subscribe_notify_one_test::notifications_to_send; i++) {
            std::shared_ptr<vsomeip::payload> its_payload = vsomeip::runtime::get()->create_payload();

            vsomeip::byte_t its_data[10] = {0};
            for (uint32_t j = 0; j < i + 1; ++j) {
                its_data[j] = static_cast<uint8_t>(j);
            }
            its_payload->set_data(its_data, i + 1);

            for (vsomeip::client_t client : subscribers_) {
                VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                              << "] Notifying client: " << client << " : " << i + 1;
                app_->notify_one(service_info_.service_id, service_info_.instance_id, service_info_.event_id, its_payload, client);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void wait_for_stop() {
        std::unique_lock<std::mutex> its_lock(stop_mutex_);
        EXPECT_TRUE(stop_condition_.wait_for(its_lock, std::chrono::seconds(15), [this] { return !wait_for_stop_; }))
                << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Stop timeout";
        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                      << "] Received notifications from all other services, sending shutdown coordination messages";

        // wait until all notifications have been sent out
        notify_thread_.join();

        // Send shutdown coordination message to all other services
        send_shutdown_messages();

        // Wait for shutdown acknowledgments from all other services
        {
            std::unique_lock<std::mutex> its_shutdown_lock(shutdown_mutex_);
            EXPECT_TRUE(
                    shutdown_condition_.wait_for(its_shutdown_lock, std::chrono::seconds(15), [this] { return !wait_for_shutdown_acks_; }))
                    << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                    << "] Shutdown coordination timeout";
        }

        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                      << "] Received all shutdown acknowledgments, going down";

        // let offer thread exit
        {
            std::scoped_lock its_lock(mutex_);
            wait_until_notified_from_other_services_ = false;
            condition_.notify_one();
        }

        stop_offer();

        // ensure that the service which hosts the routing doesn't exit to early
        if (app_->is_routing()) {
            for (const auto& i : subscribe_notify_one_test::service_infos) {
                if ((i.service_id == service_info_.service_id && i.instance_id == service_info_.instance_id)
                    || (i.service_id == 0xFFFF && i.instance_id == 0xFFFF)) {
                    continue;
                }
                while (app_->is_available(i.service_id, i.instance_id, vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        }

        for (const auto& i : subscribe_notify_one_test::service_infos) {
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
    subscribe_notify_one_test::service_info service_info_;
    std::shared_ptr<vsomeip::application> app_;
    std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, bool> other_services_available_;
    std::map<std::pair<vsomeip::service_t, vsomeip::method_t>, std::uint32_t> other_services_received_notification_;
    std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, bool> other_services_shutdown_ack_;

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

    bool wait_for_notify_;
    std::mutex notify_mutex_;
    std::condition_variable notify_condition_;
    std::thread notify_thread_;

    bool wait_for_shutdown_acks_;
    std::mutex shutdown_mutex_;
    std::condition_variable shutdown_condition_;

    std::unordered_set<vsomeip::client_t> subscribers_;
    std::atomic<uint32_t> subscription_state_handler_called_;
    std::atomic<bool> subscription_error_occured_;

    std::mutex subscribers_mutex_;
    vsomeip::reliability_type_e reliability_type_;
    bool is_tcp_;
};

static unsigned long service_number;
vsomeip::reliability_type_e reliability_type = vsomeip::reliability_type_e::RT_UNKNOWN;
static bool is_tcp = false;

TEST(someip_subscribe_notify_one_test, send_ten_notifications_to_service) {
    subscribe_notify_one_test_service its_sample(subscribe_notify_one_test::service_infos[service_number], reliability_type, is_tcp);
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Please specify a service number and event reliability type, like: " << argv[0] << " 2 UDP" << std::endl;
        std::cerr << "Valid service numbers are in the range of [1,6]" << std::endl;
        std::cerr << "Valid service reliability types are [UDP, TCP, TCP_AND_UDP]" << std::endl;

        return 1;
    }

    service_number = std::stoul(std::string(argv[1]), nullptr);

    if (std::string("TCP") == std::string(argv[2])) {
        reliability_type = vsomeip::reliability_type_e::RT_RELIABLE;
        is_tcp = true;
    } else if (std::string("UDP") == std::string(argv[2])) {
        reliability_type = vsomeip::reliability_type_e::RT_UNRELIABLE;
        is_tcp = false;
    } else if (std::string("TCP_AND_UDP") == std::string(argv[2])) {
        reliability_type = vsomeip::reliability_type_e::RT_BOTH;
        is_tcp = true;
    }

    return test_main(argc, argv);
}
#endif
