// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <iomanip>
#include <cstring>

#include <vsomeip/internal/logger.hpp>
#include "common/test_main.hpp"
#include "memory_test_client.hpp"

// Flag the desired event availability
void memory_test_client::on_availability(vsomeip::service_t service_, vsomeip::instance_t instance_, bool is_available_) {
    if (is_available_ && service_ == MEMORY_SERVICE && instance_ == MEMORY_INSTANCE) {
        std::unique_lock lk(availability_mutex);
        availability = true;
        condition_availability.notify_one();
    }
}

void memory_test_client::on_message(const std::shared_ptr<vsomeip::message>& message_) {
    if (MEMORY_SERVICE == message_->get_service() && message_->get_method() <= MEMORY_EVENT + TEST_EVENT_NUMBER
        && message_->get_method() >= MEMORY_EVENT) {
        std::uint64_t count{0};
        {
            std::scoped_lock lk(event_counter_mutex);
            received_messages_counter++;
            count = received_messages_counter;
            sec = std::chrono::system_clock::now();
        }
        // Report progress back to the service so it can bound the amount of
        // data in flight. Acking every ACK_INTERVAL messages (with only an
        // 8-byte cumulative count) keeps the return channel light instead of
        // echoing every full 4 KB payload back.
        if (count % ACK_INTERVAL == 0) {
            send_ack(count);
        }
    }
}

void memory_test_client::send_ack(std::uint64_t received_count_) {
    auto its_runtime = vsomeip::runtime::get();
    auto its_message = its_runtime->create_request(false);
    its_message->set_service(MEMORY_SERVICE);
    its_message->set_instance(MEMORY_INSTANCE);
    its_message->set_method(MEMORY_ACK_METHOD);
    its_message->set_interface_version(MEMORY_MAJOR);
    its_message->set_message_type(vsomeip::message_type_e::MT_REQUEST_NO_RETURN);

    auto its_payload = its_runtime->create_payload();
    std::vector<vsomeip::byte_t> data(sizeof(received_count_));
    // Raw host-order copy of the counter; the service reads it back the same way
    // in on_ack(). Both endpoints run on the same architecture in CI, so this
    // loopback ack needs no byte-order conversion.
    std::memcpy(data.data(), &received_count_, sizeof(received_count_));
    its_payload->set_data(std::move(data));
    its_message->set_payload(its_payload);

    _app->send(its_message);
}

memory_test_client::memory_test_client(const char* app_name_, std::map<vsomeip::event_t, int> map_events_) :
    vsomeip_utilities::base_vsip_app(app_name_), map_events(map_events_) {
    sec = std::chrono::system_clock::now();
    _app->register_availability_handler(
            MEMORY_SERVICE, MEMORY_INSTANCE,
            std::bind(&memory_test_client::on_availability, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
            MEMORY_MAJOR, MEMORY_MINOR);
    _app->register_message_handler(MEMORY_SERVICE, MEMORY_INSTANCE, vsomeip::ANY_EVENT,
                                   std::bind(&memory_test_client::on_message, this, std::placeholders::_1));
    for (uint16_t i = 0; i < TEST_EVENT_NUMBER; i++) {
        _app->request_event(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_EVENT + i, {MEMORY_EVENTGROUP}, vsomeip::event_type_e::ET_FIELD,
                            vsomeip::reliability_type_e::RT_UNRELIABLE);
    }
    _app->request_service(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_MAJOR, MEMORY_MINOR);
    for (uint16_t i = 0; i < TEST_EVENT_NUMBER; i++) {
        _app->subscribe(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_EVENTGROUP, MEMORY_MAJOR, MEMORY_EVENT + i);
    }
}

void memory_test_client::send_request(std::atomic<bool>& stop_checking_) {
    std::unique_lock lk(availability_mutex);
    // Only send the requests when the service availability is secured
    if (condition_availability.wait_for(lk, WAIT_AVAILABILITY, [this] { return availability; })) {

        // Baseline captured while the app is warm and subscribed but before any
        // events flow, so the memory evaluation measures growth under traffic.
        baseline_rss_ = read_rss_kib();

        // Trigger the test
        auto its_message = vsomeip_utilities::create_standard_vsip_request(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_START_METHOD,
                                                                           MEMORY_MAJOR, vsomeip::message_type_e::MT_REQUEST_NO_RETURN);
        _app->send(its_message);
    }

    EXPECT_TRUE(availability) << "Events expected by the client were not available for 15 seconds ";

    bool stop_watchdog{false};
    uint64_t prev_counter{0};
    auto prev_time = std::chrono::steady_clock::now();

    // 3. Wait for service to send all the messages
    while (!stop_watchdog) {
        std::this_thread::sleep_for(WATCHDOG_INTERVAL);
        auto now = std::chrono::steady_clock::now();
        double elapsed_s = std::chrono::duration<double>(now - prev_time).count();

        uint64_t current_counter{0};
        bool timed_out{false};
        {
            std::scoped_lock lk(event_counter_mutex);
            current_counter = received_messages_counter;
            timed_out = (std::chrono::system_clock::now() - sec) > CONSUMER_IDLE_TIMEOUT;
        }

        uint64_t delta = current_counter - prev_counter;
        double throughput_mbs = (static_cast<double>(delta) * NOTIFY_PAYLOAD_SIZE) / elapsed_s / (1024.0 * 1024.0);

        VSOMEIP_INFO << "[watchdog] messages in last " << std::fixed << std::setprecision(1) << elapsed_s << "s: " << delta
                     << " | throughput: " << std::fixed << std::setprecision(2) << throughput_mbs << " MB/s"
                     << " | total received: " << current_counter;

        prev_counter = current_counter;
        prev_time = now;

        if (timed_out) {
            stop_watchdog = true;
        }
    }
    std::uint64_t final_count{0};
    {
        std::scoped_lock lk(event_counter_mutex);
        final_count = received_messages_counter;
    }
    VSOMEIP_INFO << "received " << final_count;
    stop_checking_ = true;
}

void memory_test_client::unsubscribe_all() {
    _app->unsubscribe(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_EVENTGROUP);
}

void memory_test_client::stop_service() {
    auto its_message = vsomeip_utilities::create_standard_vsip_request(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_STOP_METHOD, MEMORY_MAJOR,
                                                                       vsomeip::message_type_e::MT_REQUEST_NO_RETURN);
    _app->send(its_message);
    std::uint64_t count{0};
    {
        std::scoped_lock lk(event_counter_mutex);
        count = received_messages_counter;
    }
    VSOMEIP_INFO << "sending stop " << count;
}

memory_test_client::~memory_test_client() {
    stop_service();
    unsubscribe_all();
}

TEST(memory_tests, receive_messages) {

    // Test steps:
    //      1: Start sampling memory
    //      2: Subscribe to 20 events and trigger the service (baseline captured
    //         in send_request once the service is available)
    //      3: Receive notifications, acking progress back to the service
    //      4: On the main thread, evaluate that peak memory stayed within
    //         MEMORY_LOAD_LIMIT of the steady-state floor

    std::map<vsomeip::event_t, int> events_to_subscribe;

    for (vsomeip::event_t i = 0; i < TEST_EVENT_NUMBER; i++) {
        events_to_subscribe[MEMORY_EVENT + i] = 0;
    }

    memory_test_client memory_test_client("memory_tests_client", events_to_subscribe);

    std::atomic<bool> stop_checking{false};
    std::vector<std::uint64_t> test_memory_array;

    // 1. Measure load until stop_checking is triggered
    std::thread memory_checker_thread([&stop_checking, &test_memory_array] { check_memory(test_memory_array, stop_checking); });

    // 2./3. Send a request and wait until all the messages are sent by the service
    memory_test_client.send_request(stop_checking);

    if (memory_checker_thread.joinable()) {
        memory_checker_thread.join();
    }

    // 4. Evaluate memory load increase on the main thread.
    evaluate_memory(test_memory_array, memory_test_client.baseline_rss());
}

int main(int argc, char** argv) {
    return test_main(argc, argv, std::chrono::seconds(300));
}
