// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <condition_variable>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <common/vsomeip_app_utilities.hpp>
#include <vsomeip/internal/logger.hpp>

#include "common/test_main.hpp"
#include "memory_test_request_and_response_common.hpp"
#include "memory_test_rss.hpp"

namespace {

using namespace memory_test_request_and_response;

struct measurement_result {
    std::uint64_t baseline_kib{0};
    std::uint64_t final_kib{0};
    std::size_t requests{0};
};

class request_and_response_service : public vsomeip_utilities::base_vsip_app {
public:
    request_and_response_service() : base_vsip_app("memory_test_request_and_response_service") {
        _app->register_message_handler(SERVICE, INSTANCE, vsomeip::ANY_METHOD,
                                       [this](const std::shared_ptr<vsomeip::message>& request) { on_request(request); });
        _app->offer_service(SERVICE, INSTANCE, MAJOR, MINOR);
    }

    ~request_and_response_service() {
        _app->stop_offer_service(SERVICE, INSTANCE, MAJOR, MINOR);
        _app->clear_all_handler();
    }

    bool wait_for_result(measurement_result& result) {
        std::unique_lock lock(mutex_);
        if (!condition_.wait_for(lock, TEST_TIMEOUT - std::chrono::seconds(5), [this] { return finished_; })) {
            return false;
        }
        result = result_;
        return true;
    }

private:
    void on_request(const std::shared_ptr<vsomeip::message>& request) {
        bool finish_after_response = false;
        bool send_response = true;
        {
            std::scoped_lock lock(mutex_);
            if (request->get_method() == START_MEASUREMENT_METHOD) {
                result_.baseline_kib = memory_test::trim_and_read_rss_kib();
                result_.requests = 0;
                measuring_ = true;
            } else if (request->get_method() == REQUEST_METHOD && measuring_) {
                ++result_.requests;
                send_response = false;
            } else if (request->get_method() == STOP_MEASUREMENT_METHOD) {
                result_.final_kib =
                        memory_test::wait_for_rss_at_most_kib(result_.baseline_kib, MEMORY_GROWTH_LIMIT_KIB, RSS_SETTLING_TIMEOUT);
                measuring_ = false;
                finish_after_response = true;
            }
        }

        if (send_response) {
            auto response = vsomeip::runtime::get()->create_response(request);
            response->set_payload(request->get_payload());
            _app->send(response);
        }

        if (finish_after_response) {
            std::this_thread::sleep_for(FINAL_RESPONSE_GRACE_PERIOD);
            std::scoped_lock lock(mutex_);
            finished_ = true;
            condition_.notify_one();
        }
    }

    std::condition_variable condition_;
    std::mutex mutex_;
    measurement_result result_;
    bool measuring_{false};
    bool finished_{false};
};

TEST(memory_test_request_and_response, service_memory_stays_bounded) {
    request_and_response_service service;
    measurement_result result;
    ASSERT_TRUE(service.wait_for_result(result)) << "Client did not finish the request/response phase";
    ASSERT_EQ(result.requests, MEASURED_REQUESTS);
    ASSERT_NE(result.baseline_kib, 0U) << "Service failed to read its baseline RSS";
    ASSERT_NE(result.final_kib, 0U) << "Service failed to read its final RSS";

    VSOMEIP_INFO << "service RSS: baseline=" << result.baseline_kib << " KiB, final=" << result.final_kib
                 << " KiB, allowed growth=" << MEMORY_GROWTH_LIMIT_KIB << " KiB";
    EXPECT_LE(result.final_kib, result.baseline_kib + MEMORY_GROWTH_LIMIT_KIB);
}

} // namespace

int main(int argc, char** argv) {
    return test_main(argc, argv, TEST_TIMEOUT);
}
