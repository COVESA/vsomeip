
// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <thread>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "initial_event_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>
#include <common/process_manager.hpp>

class boardnet_service_provider : public vsomeip_utilities::base_logger {
public:
    boardnet_service_provider(struct initial_event_test::service_info _service_info) :
        vsomeip_utilities::base_logger("IEBTS", "INITIAL EVENT TEST SERVICE"), service_info_(_service_info),
        app_(vsomeip::runtime::get()->create_application("initial_event_test_service")),
        registration_status_{vsomeip::state_type_e::ST_DEREGISTERED}, client_subscribed_{false}, message_received_{false} {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        app_->register_state_handler(std::bind(&boardnet_service_provider::on_state, this, std::placeholders::_1));

        app_->register_subscription_handler(service_info_.service_id, service_info_.instance_id, service_info_.eventgroup_id,
                                            std::bind(&boardnet_service_provider::on_subscription, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

        app_->register_message_handler(service_info_.service_id, service_info_.instance_id, service_info_.method_id,
                                       std::bind(&boardnet_service_provider::on_message, this, std::placeholders::_1));
    }

    ~boardnet_service_provider() {
        app_->stop();
        if (start_thread_.joinable()) {
            start_thread_.join();
        }
    }

    void start() {
        start_thread_ = std::thread([this] { app_->start(); });
    }

    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
                     << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            registration_status_ = vsomeip::state_type_e::ST_REGISTERED;
        }
        condition_.notify_all();
    }

    bool on_subscription(vsomeip::client_t _client, [[maybe_unused]] vsomeip::uid_t _uid, [[maybe_unused]] vsomeip::gid_t _gid,
                         bool _accepted) {
        VSOMEIP_INFO << "Client " << std::hex << std::setfill('0') << std::setw(4) << _client << " is "
                     << ((_accepted) ? " subscribing " : "unsubscribing");
        {
            std::lock_guard<std::mutex> lock(mutex_);
            client_subscribed_ = _accepted;
        }
        condition_.notify_all();

        return _accepted;
    }

    void on_message(const std::shared_ptr<vsomeip::message>& _request) {
        VSOMEIP_INFO << "Received message from client " << std::hex << std::setfill('0') << std::setw(4) << _request->get_client();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            message_received_ = true;
        }
        condition_.notify_all();
    }

    void offer_and_notify() {
        app_->offer_service(service_info_.service_id, service_info_.instance_id);
        // offer field
        std::set<vsomeip::eventgroup_t> its_eventgroups;
        its_eventgroups.insert(service_info_.eventgroup_id);
        app_->offer_event(service_info_.service_id, service_info_.instance_id, static_cast<vsomeip::event_t>(service_info_.event_id),
                          its_eventgroups, vsomeip::event_type_e::ET_FIELD, std::chrono::milliseconds::zero(), false, true, nullptr);

        // set value to field
        std::shared_ptr<vsomeip::payload> its_payload = vsomeip::runtime::get()->create_payload();
        vsomeip::byte_t its_data[2] = {static_cast<vsomeip::byte_t>((service_info_.service_id & 0xFF00) >> 8),
                                       static_cast<vsomeip::byte_t>((service_info_.service_id & 0xFF))};
        its_payload->set_data(its_data, 2);
        app_->notify(service_info_.service_id, service_info_.instance_id, static_cast<vsomeip::event_t>(service_info_.event_id),
                     its_payload);
    }

    void wait_for_registration() {
        std::unique_lock<std::mutex> lock{mutex_};
        condition_.wait_for(lock, std::chrono::seconds(2),
                            [this]() { return registration_status_ == vsomeip::state_type_e::ST_REGISTERED; });
    }

    void wait_for_subscription() {
        std::unique_lock<std::mutex> lock{mutex_};
        condition_.wait_for(lock, std::chrono::seconds(2), [this]() { return client_subscribed_; });
    }

    bool wait_for_message() {
        std::unique_lock<std::mutex> lock{mutex_};
        return condition_.wait_for(lock, std::chrono::seconds(10), [this]() { return message_received_; });
    }

    void reset() {
        std::lock_guard<std::mutex> lock{mutex_};
        message_received_ = false;
        client_subscribed_ = false;
        registration_status_ = vsomeip::state_type_e::ST_DEREGISTERED;
    }

private:
    initial_event_test::service_info service_info_;
    std::shared_ptr<vsomeip::application> app_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::thread start_thread_;
    vsomeip::state_type_e registration_status_;
    bool client_subscribed_;
    bool message_received_;
};

std::string host_executor{"../../../examples/routingmanagerd/routingmanagerd"};
std::string slave_executor{"./initial_event_boardnet_test_slave_starter.sh"};

TEST(someip_initial_boardnet_event_test, wait_for_initial_events_of_all_services) {
    setenv("VSOMEIP_CONFIGURATION", "initial_event_boardnet_test_master.json", 0);

    // Start host
    process_manager host{host_executor, {}};
    host.run();

    // Start service provider
    boardnet_service_provider service_provider(initial_event_test::service_infos_same_service_id[1]);
    service_provider.start();
    service_provider.wait_for_registration();
    service_provider.offer_and_notify();

    // Start client
    process_manager slave{slave_executor, {}};
    slave.run();

    service_provider.wait_for_subscription();
    EXPECT_TRUE(service_provider.wait_for_message()) << "Client didn't received initial event before host restart";

    // Clear up service sync data
    service_provider.reset();
    // Restart host
    host.reset();

    service_provider.wait_for_registration();
    service_provider.wait_for_subscription();
    EXPECT_TRUE(service_provider.wait_for_message()) << "Client didn't received initial event after host restart";

    // Wait for client process exit and test process exit code.
    slave.join();
    EXPECT_EQ(slave.exit_code_, 0);

    host.terminate();
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
