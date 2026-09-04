// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <common/vsomeip_app_utilities.hpp>
#include <vsomeip/internal/logger.hpp>

#include "common/test_main.hpp"
#include "memory_test_request_and_response_common.hpp"
#include "memory_test_rss.hpp"

namespace {

using namespace memory_test_request_and_response;

class request_and_response_client : public vsomeip_utilities::base_vsip_app {
public:
    request_and_response_client() : base_vsip_app("memory_test_request_and_response_client") {
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

    ~request_and_response_client() {
        _app->clear_all_handler();
        _app->release_service(SERVICE, INSTANCE);
    }

    bool wait_for_service() {
        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, AVAILABILITY_TIMEOUT, [this] { return available_; });
    }

    bool exchange(vsomeip::method_t method, const std::vector<vsomeip::byte_t>& data = {}) {
        {
            std::scoped_lock lock(mutex_);
            response_received_ = false;
            response_valid_ = false;
            expected_method_ = method;
            expected_payload_ = data;
        }

        send_request(method, data);

        std::unique_lock lock(mutex_);
        return condition_.wait_for(lock, RESPONSE_TIMEOUT, [this] { return response_received_; }) && response_valid_;
    }

    void send_request(vsomeip::method_t method, const std::vector<vsomeip::byte_t>& data = {}) {
        auto request =
                vsomeip_utilities::create_standard_vsip_request(SERVICE, INSTANCE, method, MAJOR, vsomeip::message_type_e::MT_REQUEST);
        auto payload = vsomeip::runtime::get()->create_payload();
        payload->set_data(data);
        request->set_payload(payload);
        _app->send(request);
    }

private:
    void on_response(const std::shared_ptr<vsomeip::message>& message) {
        if (message->get_message_type() != vsomeip::message_type_e::MT_RESPONSE) {
            return;
        }

        const auto payload = message->get_payload();
        std::scoped_lock lock(mutex_);
        const bool payload_matches = payload->get_length() == expected_payload_.size()
                && std::equal(expected_payload_.begin(), expected_payload_.end(), payload->get_data());

        response_valid_ = message->get_service() == SERVICE && message->get_instance() == INSTANCE
                && message->get_method() == expected_method_ && payload_matches;
        response_received_ = true;
        condition_.notify_all();
    }

    std::condition_variable condition_;
    std::mutex mutex_;
    bool available_{false};
    bool response_received_{false};
    bool response_valid_{false};
    vsomeip::method_t expected_method_{0};
    std::vector<vsomeip::byte_t> expected_payload_;
};

void expect_stable_memory(const char* process, std::uint64_t baseline_kib, std::uint64_t final_kib) {
    ASSERT_NE(baseline_kib, 0U) << process << " failed to read its baseline RSS";
    ASSERT_NE(final_kib, 0U) << process << " failed to read its final RSS";

    VSOMEIP_INFO << process << " RSS: baseline=" << baseline_kib << " KiB, final=" << final_kib
                 << " KiB, allowed growth=" << MEMORY_GROWTH_LIMIT_KIB << " KiB";
    EXPECT_LE(final_kib, baseline_kib + MEMORY_GROWTH_LIMIT_KIB);
}

TEST(memory_test_request_and_response, client_memory_stays_bounded) {
    request_and_response_client client;
    ASSERT_TRUE(client.wait_for_service()) << "Service was not available within the timeout";

    const std::vector<vsomeip::byte_t> payload(PAYLOAD_SIZE, 0x5a);
    for (std::size_t request = 0; request < WARMUP_REQUESTS; ++request) {
        ASSERT_TRUE(client.exchange(REQUEST_METHOD, payload)) << "Warm-up request " << request << " failed";
    }

    ASSERT_TRUE(client.exchange(START_MEASUREMENT_METHOD));
    const auto baseline_kib = memory_test::trim_and_read_rss_kib();

    for (std::size_t batch = 0; batch < MEASURED_BATCHES; ++batch) {
        for (std::size_t request = 0; request < REQUESTS_PER_BATCH; ++request) {
            client.send_request(REQUEST_METHOD, payload);
        }
        // The response keeps application-level flow control without clearing
        // the pending REQUEST_METHOD sessions in the historical implementation.
        ASSERT_TRUE(client.exchange(BARRIER_METHOD)) << "Response barrier for batch " << batch << " failed";
    }

    const auto final_kib = memory_test::wait_for_rss_at_most_kib(baseline_kib, MEMORY_GROWTH_LIMIT_KIB, RSS_SETTLING_TIMEOUT);
    expect_stable_memory("client", baseline_kib, final_kib);

    ASSERT_TRUE(client.exchange(STOP_MEASUREMENT_METHOD));
}

} // namespace

int main(int argc, char** argv) {
    return test_main(argc, argv, TEST_TIMEOUT);
}
