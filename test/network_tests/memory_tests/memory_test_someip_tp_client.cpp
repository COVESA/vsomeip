// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <condition_variable>
#include <mutex>

#include <gtest/gtest.h>

#include <common/vsomeip_app_utilities.hpp>

#include "common/test_main.hpp"
#include "memory_test_someip_tp_common.hpp"

namespace {

using namespace memory_test_someip_tp;

class someip_tp_client : public vsomeip_utilities::base_vsip_app {
public:
    someip_tp_client() : base_vsip_app("memory_test_someip_tp_client") {
        _app->register_availability_handler(
                SERVICE, INSTANCE,
                [this](vsomeip::service_t service, vsomeip::instance_t instance, bool available) {
                    if (service == SERVICE && instance == INSTANCE) {
                        std::scoped_lock lock(mutex_);
                        available_ = available;
                        condition_.notify_all();
                    }
                },
                MAJOR, MINOR);
        _app->register_message_handler(SERVICE, INSTANCE, vsomeip::ANY_METHOD,
                                       [this](const std::shared_ptr<vsomeip::message>& message) { on_message(message); });
        _app->request_event(SERVICE, INSTANCE, RETENTION_EVENT, {EVENTGROUP}, vsomeip::event_type_e::ET_EVENT,
                            vsomeip::reliability_type_e::RT_UNRELIABLE);
        _app->request_service(SERVICE, INSTANCE, MAJOR, MINOR);
        _app->subscribe(SERVICE, INSTANCE, EVENTGROUP, MAJOR, vsomeip::ANY_EVENT);
    }

    ~someip_tp_client() {
        _app->unsubscribe(SERVICE, INSTANCE, EVENTGROUP);
        _app->clear_all_handler();
        _app->release_service(SERVICE, INSTANCE);
    }

    bool wait_for_service() {
        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, AVAILABILITY_TIMEOUT, [this] { return available_; });
    }

    bool exchange(vsomeip::method_t method) {
        {
            std::scoped_lock lock(mutex_);
            expected_method_ = method;
            response_received_ = false;
        }
        send(method, vsomeip::message_type_e::MT_REQUEST);

        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, RESPONSE_TIMEOUT, [this] { return response_received_; });
    }

    std::size_t event_count() {
        std::scoped_lock lock(mutex_);
        return retention_events_;
    }

    bool wait_for_events(std::size_t count) {
        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, EVENT_TIMEOUT, [this, count] { return retention_events_ >= count; });
    }

private:
    void send(vsomeip::method_t method, vsomeip::message_type_e type) {
        auto request = vsomeip_utilities::create_standard_vsip_request(SERVICE, INSTANCE, method, MAJOR, type);
        _app->send(request);
    }

    void on_message(const std::shared_ptr<vsomeip::message>& message) {
        std::scoped_lock lock(mutex_);
        if (message->get_method() == RETENTION_EVENT && message->get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION
            && message->get_payload()->get_length() == RETENTION_PAYLOAD_SIZE) {
            ++retention_events_;
            condition_.notify_all();
        } else if (message->get_message_type() == vsomeip::message_type_e::MT_RESPONSE && message->get_method() == expected_method_) {
            response_received_ = true;
            condition_.notify_all();
        }
    }

    std::condition_variable condition_;
    std::mutex mutex_;
    std::size_t retention_events_{0};
    bool available_{false};
    bool response_received_{false};
    vsomeip::method_t expected_method_{0};
};

TEST(memory_test_someip_tp, segmented_notification_phase_completes) {
    someip_tp_client client;
    ASSERT_TRUE(client.wait_for_service()) << "Service was not available within the timeout";

    bool warmup_received = false;
    for (std::size_t attempt = 0; attempt < 3 && !warmup_received; ++attempt) {
        const auto expected = client.event_count() + 1;
        ASSERT_TRUE(client.exchange(WARMUP_METHOD));
        warmup_received = client.wait_for_events(expected);
    }
    ASSERT_TRUE(warmup_received) << "No segmented warm-up notification was received";

    ASSERT_TRUE(client.exchange(START_METHOD));
    for (std::size_t notification = 0; notification < RETENTION_NOTIFICATIONS; ++notification) {
        const auto expected = client.event_count() + 1;
        ASSERT_TRUE(client.exchange(WARMUP_METHOD)) << "Sequential TP notification " << notification << " failed";
        ASSERT_TRUE(client.wait_for_events(expected)) << "Sequential TP notification " << notification << " was not delivered";
    }
    ASSERT_TRUE(client.exchange(RETENTION_CHECK_METHOD));
    ASSERT_TRUE(client.exchange(STOP_METHOD));
}

} // namespace

int main(int argc, char** argv) {
    return test_main(argc, argv, TEST_TIMEOUT);
}
