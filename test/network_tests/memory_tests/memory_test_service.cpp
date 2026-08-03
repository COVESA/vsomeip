// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/internal/logger.hpp>
#include <chrono>
#include <cstring>
#include "common/test_main.hpp"
#include "memory_test_service.hpp"

memory_test_service::memory_test_service(const char* app_name_) : vsomeip_utilities::base_vsip_app(app_name_) {
    for (uint16_t i = 0; i < TEST_EVENT_NUMBER; i++) {
        _app->offer_event(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_EVENT + i, {MEMORY_EVENTGROUP}, vsomeip::event_type_e::ET_FIELD,
                          std::chrono::milliseconds::zero(), false, true, nullptr, vsomeip::reliability_type_e::RT_UNRELIABLE);
    }
    _app->register_message_handler(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_START_METHOD,
                                   std::bind(&memory_test_service::on_start, this, std::placeholders::_1));
    _app->register_message_handler(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_STOP_METHOD,
                                   std::bind(&memory_test_service::on_stop, this, std::placeholders::_1));
    _app->register_message_handler(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_ACK_METHOD,
                                   std::bind(&memory_test_service::on_ack, this, std::placeholders::_1));
    _app->offer_service(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_MAJOR, MEMORY_MINOR);
}
void memory_test_service::on_start(const std::shared_ptr<vsomeip::message> /*&_message*/) {
    std::unique_lock lk(start_mutex);
    received_message = true;
    condition_wait_start.notify_one();
}

void memory_test_service::on_stop(const std::shared_ptr<vsomeip::message> /*&_message*/) {
    {
        std::unique_lock lk(stop_mutex);
        condition_wait_stop.notify_one();
    }
    VSOMEIP_INFO << "Received a STOP command.";
}

void memory_test_service::on_ack(const std::shared_ptr<vsomeip::message>& _message) {
    const auto its_payload = _message->get_payload();
    if (!its_payload || its_payload->get_length() < sizeof(uint64_t)) {
        return;
    }
    uint64_t received{0};
    // Host-order copy; the client wrote the counter with the same layout and
    // both run on the same architecture in CI (see send_ack()).
    std::memcpy(&received, its_payload->get_data(), sizeof(received));

    // Acks may arrive out of order over UDP; only ever move the high-water
    // mark forward.
    uint64_t prev = acked_count_.load();
    while (received > prev && !acked_count_.compare_exchange_weak(prev, received)) {
        // prev was reloaded by compare_exchange_weak; retry.
    }
}

void memory_test_service::wait_for_flow_control(uint64_t sent_) {
    auto last_progress = std::chrono::steady_clock::now();
    uint64_t last_acked = acked_count_.load();

    while (sent_ - acked_count_.load() >= FLOW_CONTROL_WINDOW) {
        const uint64_t current_acked = acked_count_.load();
        if (current_acked != last_acked) {
            last_acked = current_acked;
            last_progress = std::chrono::steady_clock::now();
        } else if (std::chrono::steady_clock::now() - last_progress > FLOW_CONTROL_STALL_TIMEOUT) {
            VSOMEIP_WARNING << "message_sender: no ack progress for "
                            << std::chrono::duration_cast<std::chrono::seconds>(FLOW_CONTROL_STALL_TIMEOUT).count() << "s (sent " << sent_
                            << ", acked " << current_acked << "); proceeding";
            return;
        }
        std::this_thread::sleep_for(FLOW_CONTROL_POLL);
    }
}

void memory_test_service::message_sender(std::atomic<bool>& stop_checking_) {
    auto its_payload = vsomeip::runtime::get()->create_payload();
    auto its_payload2 = vsomeip::runtime::get()->create_payload();

    its_payload->set_data(std::vector<uint8_t>(NOTIFY_PAYLOAD_SIZE, 20));
    its_payload2->set_data(std::vector<uint8_t>(NOTIFY_PAYLOAD_SIZE, 10));

    uint64_t sent{0};
    const auto deadline = std::chrono::steady_clock::now() + MESSAGE_SENDER_DURATION;
    while (std::chrono::steady_clock::now() < deadline) {
        wait_for_flow_control(sent);
        for (uint16_t i = 0; i < TEST_EVENT_NUMBER; i++) {
            _app->notify(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_EVENT + i, its_payload);
            sent++;
        }
        std::this_thread::sleep_for(MESSAGE_SENDER_INTERVAL);
        wait_for_flow_control(sent);
        for (uint16_t i = 0; i < TEST_EVENT_NUMBER; i++) {
            _app->notify(MEMORY_SERVICE, MEMORY_INSTANCE, MEMORY_EVENT + i, its_payload2);
            sent++;
        }
        std::this_thread::sleep_for(MESSAGE_SENDER_INTERVAL);
    }
    stop_checking_ = true;
    VSOMEIP_INFO << "sent " << sent << " messages, client acked " << acked_count_.load();
}

// wait for the start message, run the threads to send messages
// and receive the stop message in the end
void memory_test_service::setup_app(const std::function<void(void)> executionHandler_) {
    std::unique_lock lk(start_mutex);
    if (condition_wait_start.wait_for(lk, WAIT_START_MESSAGE, [this] { return received_message; })) {

        // If executionHandler_ is set / not nullptr
        if (executionHandler_) {
            // run send the messages
            executionHandler_();
        }

        {
            // 4. Wait for client to send stop message
            std::unique_lock lk(stop_mutex);
            condition_wait_stop.wait_for(lk, WAIT_STOP_MESSAGE);
            std::cout << "service: exiting" << std::endl;
        }
    }
}

TEST(memory_test, send_messages) {

    // Test steps:
    //      1: After receiving the start message from the client, capture a
    //         pre-traffic memory baseline and start sampling memory
    //      2: Send notifications (SOME/IP-TP segmented) for each event under
    //         application-level flow control until MESSAGE_SENDER_DURATION
    //      3: On the main thread, evaluate that peak memory stayed within
    //         MEMORY_LOAD_LIMIT of the steady-state floor
    //      4: Wait for the client stop message, then exit
    //
    // Flow control keeps the send queue from filling, so a memory increase
    // beyond the threshold reflects a genuine leak rather than queue congestion
    // on a slow/contended host.

    memory_test_service its_service("memory_test_service");
    std::atomic<bool> stop_checking{false};
    std::vector<uint64_t> test_memory_array;
    uint64_t baseline_rss{0};

    std::thread memory_checker_thread;

    its_service.setup_app([&] {
        // 1. Baseline captured while the app is warm but no traffic flows yet.
        baseline_rss = read_rss_kib();
        memory_checker_thread = std::thread([&stop_checking, &test_memory_array] { check_memory(test_memory_array, stop_checking); });
        // 2. Start sending notifications
        its_service.message_sender(stop_checking);

        if (memory_checker_thread.joinable()) {
            memory_checker_thread.join();
        }

        // 3. Evaluate memory load increase on the main thread so a failure is
        //    reported by gtest instead of escaping the worker thread and aborting.
        //    Done before waiting for the client's stop message, so the STOP
        //    handshake is the last step of the test.
        evaluate_memory(test_memory_array, baseline_rss);
    });
    // 4. setup_app returns after the client's stop message (or WAIT_STOP_MESSAGE
    //    times out); the app then tears down.
}
int main(int argc, char** argv) {
    return test_main(argc, argv, std::chrono::seconds(300));
}
