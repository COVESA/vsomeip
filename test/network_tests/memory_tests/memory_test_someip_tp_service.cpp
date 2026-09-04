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
#include "memory_test_rss.hpp"
#include "memory_test_someip_tp_common.hpp"

namespace {

using namespace memory_test_someip_tp;

struct measurement_result {
    std::uint64_t baseline_kib{0};
    std::uint64_t after_retention_kib{0};
};

class someip_tp_service : public vsomeip_utilities::base_vsip_app {
public:
    someip_tp_service() : base_vsip_app("memory_test_someip_tp_service") {
        _app->offer_event(SERVICE, INSTANCE, RETENTION_EVENT, {EVENTGROUP}, vsomeip::event_type_e::ET_EVENT,
                          std::chrono::milliseconds::zero(), false, true, nullptr, vsomeip::reliability_type_e::RT_UNRELIABLE);
        _app->register_message_handler(SERVICE, INSTANCE, vsomeip::ANY_METHOD,
                                       [this](const std::shared_ptr<vsomeip::message>& request) { on_request(request); });
        _app->offer_service(SERVICE, INSTANCE, MAJOR, MINOR);
    }

    ~someip_tp_service() {
        _app->stop_offer_event(SERVICE, INSTANCE, RETENTION_EVENT);
        _app->stop_offer_service(SERVICE, INSTANCE, MAJOR, MINOR);
        _app->clear_all_handler();
    }

    bool wait_for_result(measurement_result& result) {
        std::unique_lock lock(mutex_);
        if (!condition_.wait_for(lock, TEST_TIMEOUT - std::chrono::seconds(3), [this] { return finished_; })) {
            return false;
        }
        result = result_;
        return true;
    }

private:
    void notify(vsomeip::event_t event, std::size_t payload_size, std::size_t index) {
        std::vector<vsomeip::byte_t> data(payload_size, 0x5a);
        data[0] = static_cast<vsomeip::byte_t>(index);
        data[1] = static_cast<vsomeip::byte_t>(index >> 8);
        auto payload = vsomeip::runtime::get()->create_payload();
        payload->set_data(data);
        _app->notify(SERVICE, INSTANCE, event, payload, true);
    }

    void send_response(const std::shared_ptr<vsomeip::message>& request) {
        auto response = vsomeip::runtime::get()->create_response(request);
        _app->send(response);
    }

    void on_request(const std::shared_ptr<vsomeip::message>& request) {
        if (request->get_method() == WARMUP_METHOD) {
            notify(RETENTION_EVENT, RETENTION_PAYLOAD_SIZE, 0);
            send_response(request);
            return;
        }
        if (request->get_method() == START_METHOD) {
            result_.baseline_kib = memory_test::trim_and_read_rss_kib();
            send_response(request);
            return;
        }
        if (request->get_method() == RETENTION_CHECK_METHOD) {
            result_.after_retention_kib = memory_test::wait_for_rss_at_most_kib(result_.baseline_kib, RETENTION_RSS_GROWTH_LIMIT_KIB,
                                                                                RETENTION_RSS_SETTLING_TIMEOUT);
            send_response(request);
            return;
        }
        if (request->get_method() == STOP_METHOD) {
            send_response(request);
            // Give the asynchronous endpoint time to send the response before
            // the test completes and destroys the application.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::scoped_lock lock(mutex_);
            finished_ = true;
            condition_.notify_one();
        }
    }

    std::condition_variable condition_;
    std::mutex mutex_;
    measurement_result result_;
    bool finished_{false};
};

TEST(memory_test_someip_tp, completed_train_memory_stays_bounded) {
    someip_tp_service service;
    measurement_result result;
    ASSERT_TRUE(service.wait_for_result(result)) << "Client did not finish the segmented notification phase";

    ASSERT_NE(result.baseline_kib, 0U) << "Failed to read the service baseline RSS";
    ASSERT_NE(result.after_retention_kib, 0U) << "Failed to read the service RSS after the paced notification phase";

    VSOMEIP_INFO << "SOME/IP-TP RSS: baseline=" << result.baseline_kib << " KiB, after retention phase=" << result.after_retention_kib
                 << " KiB";
    EXPECT_LE(result.after_retention_kib, result.baseline_kib + RETENTION_RSS_GROWTH_LIMIT_KIB)
            << "segmented trains remained referenced after paced notifications";
}

} // namespace

int main(int argc, char** argv) {
    return test_main(argc, argv, TEST_TIMEOUT);
}
