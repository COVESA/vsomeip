// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <condition_variable>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <common/vsomeip_app_utilities.hpp>

#include "common/test_main.hpp"
#include "memory_test_pending_trains_common.hpp"

namespace {

using namespace memory_test_pending_trains;

class pending_trains_service : public vsomeip_utilities::base_vsip_app {
public:
    pending_trains_service() : base_vsip_app("memory_test_pending_trains_service") {
        _app->register_message_handler(SERVICE, INSTANCE, vsomeip::ANY_METHOD,
                                       [this](const std::shared_ptr<vsomeip::message>& request) { on_request(request); });
        _app->offer_service(SERVICE, INSTANCE, MAJOR, MINOR);
    }

    ~pending_trains_service() {
        _app->stop_offer_service(SERVICE, INSTANCE, MAJOR, MINOR);
        _app->clear_all_handler();
    }

    bool wait_for_result(std::size_t& requests) {
        std::unique_lock lock(mutex_);
        if (!condition_.wait_for(lock, TEST_TIMEOUT - std::chrono::seconds(3), [this] { return finished_; })) {
            return false;
        }
        requests = measured_requests_;
        return true;
    }

private:
    void on_request(const std::shared_ptr<vsomeip::message>& request) {
        bool finish = false;
        if (request->get_method() == TRAFFIC_METHOD) {
            std::scoped_lock lock(mutex_);
            if (measuring_) {
                ++measured_requests_;
            }
            return;
        }

        {
            std::scoped_lock lock(mutex_);
            if (request->get_method() == START_METHOD) {
                measured_requests_ = 0;
                measuring_ = true;
            } else if (request->get_method() == STOP_METHOD) {
                measuring_ = false;
                finish = true;
            }
        }

        auto response = vsomeip::runtime::get()->create_response(request);
        _app->send(response);

        if (finish) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::scoped_lock lock(mutex_);
            finished_ = true;
            condition_.notify_one();
        }
    }

    std::condition_variable condition_;
    std::mutex mutex_;
    std::size_t measured_requests_{0};
    bool measuring_{false};
    bool finished_{false};
};

TEST(memory_test_pending_trains, queue_limit_drops_excess_pending_requests) {
    pending_trains_service service;
    std::size_t requests = 0;
    ASSERT_TRUE(service.wait_for_result(requests)) << "Client did not finish the pending-train phase";
    EXPECT_GT(requests, 0U) << "No measured request reached the service";
    EXPECT_LT(requests, BURST_REQUESTS) << "All requests reached the service; the pending-train bytes escaped the queue limit";
}

} // namespace

int main(int argc, char** argv) {
    return test_main(argc, argv, TEST_TIMEOUT);
}
