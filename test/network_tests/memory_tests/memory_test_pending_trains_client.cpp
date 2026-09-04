// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <common/vsomeip_app_utilities.hpp>
#include <vsomeip/internal/logger.hpp>

#include "common/test_main.hpp"
#include "memory_test_pending_trains_common.hpp"
#include "memory_test_rss.hpp"

namespace {

using namespace memory_test_pending_trains;

class pending_trains_client : public vsomeip_utilities::base_vsip_app {
public:
    pending_trains_client() : base_vsip_app("memory_test_pending_trains_client") {
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
                                       [this](const std::shared_ptr<vsomeip::message>& message) { on_response(message); });
        _app->request_service(SERVICE, INSTANCE, MAJOR, MINOR);
    }

    ~pending_trains_client() {
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
        send(method, vsomeip::message_type_e::MT_REQUEST, {});

        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, RESPONSE_TIMEOUT, [this] { return response_received_; });
    }

    // The traffic message is built once: allocating a message and a payload per
    // request would dominate the RSS samples taken during the burst.
    void prepare_traffic() {
        traffic_request_ = vsomeip_utilities::create_standard_vsip_request(SERVICE, INSTANCE, TRAFFIC_METHOD, MAJOR,
                                                                           vsomeip::message_type_e::MT_REQUEST_NO_RETURN);
        auto payload = vsomeip::runtime::get()->create_payload();
        payload->set_data(std::vector<vsomeip::byte_t>(PAYLOAD_SIZE, 0x5a));
        traffic_request_->set_payload(payload);
    }

    void send_traffic() { _app->send(traffic_request_); }

private:
    void send(vsomeip::method_t method, vsomeip::message_type_e type, const std::vector<vsomeip::byte_t>& data) {
        auto request = vsomeip_utilities::create_standard_vsip_request(SERVICE, INSTANCE, method, MAJOR, type);
        auto payload = vsomeip::runtime::get()->create_payload();
        payload->set_data(data);
        request->set_payload(payload);
        _app->send(request);
    }

    void on_response(const std::shared_ptr<vsomeip::message>& message) {
        if (message->get_message_type() != vsomeip::message_type_e::MT_RESPONSE) {
            return;
        }
        std::scoped_lock lock(mutex_);
        if (message->get_service() == SERVICE && message->get_instance() == INSTANCE && message->get_method() == expected_method_) {
            response_received_ = true;
            condition_.notify_all();
        }
    }

    std::condition_variable condition_;
    std::mutex mutex_;
    std::shared_ptr<vsomeip::message> traffic_request_;
    bool available_{false};
    bool response_received_{false};
    vsomeip::method_t expected_method_{0};
};

TEST(memory_test_pending_trains, client_releases_pending_train_memory) {
    pending_trains_client client;
    ASSERT_TRUE(client.wait_for_service()) << "Service was not available within the timeout";

    client.prepare_traffic();
    for (std::size_t request = 0; request < WARMUP_REQUESTS; ++request) {
        client.send_traffic();
    }
    // Wait for warm-up trains to drain before measuring the baseline.
    std::this_thread::sleep_for(TRAIN_DRAIN_DURATION);

    ASSERT_TRUE(client.exchange(START_METHOD));
    const auto baseline_kib = memory_test::trim_and_read_rss_kib();

    for (std::size_t request = 0; request < BURST_REQUESTS; ++request) {
        client.send_traffic();
    }
    const auto held_kib = memory_test::wait_for_rss_at_most_kib(baseline_kib, HELD_RSS_GROWTH_LIMIT_KIB, HELD_RSS_SETTLING_TIMEOUT);

    // Wait for vsomeip to flush pending trains before checking memory again.
    std::this_thread::sleep_for(TRAIN_DRAIN_DURATION);
    const auto retained_kib =
            memory_test::wait_for_rss_at_most_kib(baseline_kib, RETAINED_RSS_GROWTH_LIMIT_KIB, RETAINED_RSS_SETTLING_TIMEOUT);
    ASSERT_TRUE(client.exchange(STOP_METHOD));

    ASSERT_NE(baseline_kib, 0U) << "Failed to read the client baseline RSS";
    ASSERT_NE(held_kib, 0U) << "Failed to read the client RSS while requests were pending";
    ASSERT_NE(retained_kib, 0U) << "Failed to read the client RSS after the endpoint drained";

    VSOMEIP_INFO << "pending trains RSS: baseline=" << baseline_kib << " KiB, held=" << held_kib << " KiB, retained=" << retained_kib
                 << " KiB";
    EXPECT_LE(held_kib, baseline_kib + HELD_RSS_GROWTH_LIMIT_KIB)
            << "the burst bytes were parked in pending trains instead of being rejected by the endpoint queue limit";
    EXPECT_LE(retained_kib, baseline_kib + RETAINED_RSS_GROWTH_LIMIT_KIB)
            << "pending request memory was not released after the endpoint drained";
}

} // namespace

int main(int argc, char** argv) {
    return test_main(argc, argv, TEST_TIMEOUT);
}
