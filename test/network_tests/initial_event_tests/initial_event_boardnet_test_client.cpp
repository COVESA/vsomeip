
// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <thread>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "initial_event_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>

class boardnet_client : public vsomeip_utilities::base_logger {
public:
    boardnet_client(struct initial_event_test::service_info _service_info) :
        vsomeip_utilities::base_logger("IEBTC", "INITIAL EVENT BOARDNET TEST CLIENT"), service_info_(_service_info),
        app_(vsomeip::runtime::get()->create_application("initial_event_test_client")), initial_event_counter{0} {

        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        app_->register_state_handler(std::bind(&boardnet_client::on_state, this, std::placeholders::_1));

        app_->register_message_handler(service_info_.service_id, service_info_.instance_id, service_info_.method_id,
                                       std::bind(&boardnet_client::on_message, this, std::placeholders::_1));
    }

    ~boardnet_client() {
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

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            app_->request_service(service_info_.service_id, service_info_.instance_id);
            std::set<vsomeip::eventgroup_t> its_groups;
            its_groups.insert(service_info_.eventgroup_id);
            app_->request_event(service_info_.service_id, service_info_.instance_id, service_info_.event_id, its_groups,
                                vsomeip::event_type_e::ET_FIELD);
            app_->subscribe(service_info_.service_id, service_info_.instance_id, service_info_.eventgroup_id);
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message>& _response) {
        VSOMEIP_INFO << "Received a notification for Event [" << std::hex << std::setfill('0') << std::setw(4) << _response->get_service()
                     << "." << std::setw(4) << _response->get_instance() << "." << std::setw(4) << _response->get_method()
                     << "] to Client/Session [" << std::setw(4) << _response->get_client() << "/" << std::setw(4)
                     << _response->get_session() << "] = ";
        {
            std::scoped_lock lock(mutex_);
            ++initial_event_counter;
        }
        condition_.notify_one();
    }

    void send_message() {
        auto request = vsomeip::runtime::get()->create_request(true);
        request->set_service(service_info_.service_id);
        request->set_instance(service_info_.instance_id);
        request->set_method(service_info_.method_id);

        std::shared_ptr<vsomeip::payload> its_payload_request = vsomeip::runtime::get()->create_payload();
        std::vector<vsomeip::byte_t> its_payload_data;
        for (std::size_t i = 0; i < 10; ++i)
            its_payload_data.push_back(vsomeip::byte_t(i % 256));
        its_payload_request->set_data(its_payload_data);
        request->set_payload(its_payload_request);

        app_->send(request);
    }

    bool wait_for_initial_event(uint32_t expected_counter_value) {
        std::unique_lock<std::mutex> lock{mutex_};
        return condition_.wait_for(lock, std::chrono::seconds(5), [&]() { return expected_counter_value == initial_event_counter; });
    }

    void stop() { app_->stop(); }

private:
    initial_event_test::service_info service_info_;
    std::shared_ptr<vsomeip::application> app_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::thread start_thread_;
    uint32_t initial_event_counter;
};

TEST(someip_initial_event_test, boardnet_client) {
    setenv("VSOMEIP_CONFIGURATION", "initial_event_boardnet_test_slave.json", 1);
    boardnet_client client_consumer(initial_event_test::service_infos_same_service_id[1]);
    client_consumer.start();
    ASSERT_TRUE(client_consumer.wait_for_initial_event(1)) << "Failed to receive initial event before master host restart";
    client_consumer.send_message();
    // Host should be restart by manager at this point.
    ASSERT_TRUE(client_consumer.wait_for_initial_event(2)) << "Failed to receive initial event after master host restart";
    client_consumer.send_message();

    // magic sleep to give time for the last message to be sent
    // TODO: FIXME! REMOVE THIS!
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
