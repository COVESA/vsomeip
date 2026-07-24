// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <iomanip>

#include <vsomeip/internal/logger.hpp>
#include "common/test_main.hpp"
#include "common/timeout_scale.hpp"

#include "debounce_filter_test_client.hpp"

static std::vector<std::vector<std::shared_ptr<vsomeip::payload>>> payloads__;

namespace {
// Number of debounce intervals to discard at the start of a measurement.
// Right after subscription the client application's dispatch thread may be
// cold and deliver an initial backlog of debounced events as a burst, which
// would skew the average we actually wantto measure. Ignore the first few intervals
// until the stream has settled into steady state.
constexpr int64_t WARMUP_INTERVALS = 3;
// Number of steady-state intervals to average over.
constexpr int64_t MEASURE_INTERVALS = 10;
// Base tolerance (in ms) allowed around the expected debounce interval when
// checking the measured average. Scaled by the test timeout multiplier so
// slower/loaded environments get proportionally more slack.
constexpr int64_t TOLERANCE_MS = 25;
} // namespace

debounce_test_client::debounce_test_client(int64_t _interval) :
    interval(_interval), is_available_(false), runner_(std::bind(&debounce_test_client::run, this)),
    app_(vsomeip::runtime::get()->create_application("debounce_test_client")), sum_time(0) { }

bool debounce_test_client::init() {
    dBFilter.interval_ = interval;

    bool its_result = app_->init();
    if (its_result) {
        app_->register_availability_handler(DEBOUNCE_SERVICE, DEBOUNCE_INSTANCE,
                                            std::bind(&debounce_test_client::on_availability, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3),
                                            DEBOUNCE_MAJOR, DEBOUNCE_MINOR);
        app_->register_message_handler(DEBOUNCE_SERVICE, DEBOUNCE_INSTANCE, vsomeip::ANY_EVENT,
                                       std::bind(&debounce_test_client::on_message, this, std::placeholders::_1));
        app_->request_event(DEBOUNCE_SERVICE, DEBOUNCE_INSTANCE, DEBOUNCE_EVENT, {DEBOUNCE_EVENTGROUP}, vsomeip::event_type_e::ET_FIELD,
                            vsomeip::reliability_type_e::RT_UNRELIABLE);
        app_->request_service(DEBOUNCE_SERVICE, DEBOUNCE_INSTANCE, DEBOUNCE_MAJOR, DEBOUNCE_MINOR);
        app_->subscribe_with_debounce(DEBOUNCE_SERVICE, DEBOUNCE_INSTANCE, DEBOUNCE_EVENTGROUP, DEBOUNCE_MAJOR, DEBOUNCE_EVENT, dBFilter);
    }
    return its_result;
}

void debounce_test_client::start() {
    VSOMEIP_INFO << "Starting Client...";
    app_->start();
}

void debounce_test_client::stop() {
    VSOMEIP_INFO << "Stopping Client...";
    app_->stop();
}

void debounce_test_client::run() {
    {
        std::unique_lock its_lock(run_mutex_);
        if (!run_condition_.wait_for(its_lock, common::scaled_timeout(std::chrono::seconds(15)), [this] { return is_available_; })) {
            GTEST_FATAL_FAILURE_("Debounce service did not become available after 15s.");
            stop();
            return;
        }
    }

    VSOMEIP_INFO << "Running test.";
    run_test();

    unsubscribe_all();

    VSOMEIP_INFO << "Stopping the service.";
    stop_service();

    // Wait for service to become unavailable before stopping client
    // This ensures the service has completed its cleanup before the next test starts
    {
        std::unique_lock its_lock(run_mutex_);
        if (!run_condition_.wait_for(its_lock, common::scaled_timeout(std::chrono::seconds(2)), [this] { return !is_available_; })) {
            VSOMEIP_WARNING << "Service did not become unavailable within timeout";
        }
    }

    stop();
}

void debounce_test_client::wait() {
    if (runner_.joinable())
        runner_.join();
}

void debounce_test_client::on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
    if (_service == DEBOUNCE_SERVICE && _instance == DEBOUNCE_INSTANCE) {

        if (_is_available) {
            VSOMEIP_INFO << "Debounce service becomes available.";
            {
                std::scoped_lock its_lock(run_mutex_);
                is_available_ = true;
            }
            run_condition_.notify_one();
        } else {
            VSOMEIP_INFO << "Debounce service becomes unavailable.";

            std::scoped_lock its_lock(run_mutex_);
            is_available_ = false;
        }
    }
}

void debounce_test_client::on_message(const std::shared_ptr<vsomeip::message>& _message) {
    if (DEBOUNCE_SERVICE != _message->get_service() || DEBOUNCE_EVENT != _message->get_method()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const int64_t count = ++nb_msgs_rcvd;

    std::stringstream s;
    s << "RECV: ";
    for (uint32_t i = 0; i < _message->get_payload()->get_length(); i++) {
        s << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(_message->get_payload()->get_data()[i]) << " ";
    }
    s << "\t- Message: " << std::dec << std::setw(2) << count;

    if (count == 1) {
        // Start the warm-up window on the first received event.
        warmup_end = now + std::chrono::milliseconds(WARMUP_INTERVALS * interval);
        time_last = now;
    } else if (now < warmup_end) {
        // Still warming up: advance the baseline but don't measure, so an
        // initial burst of backlogged events doesn't corrupt the average.
        time_last = now;
    } else {
        // Steady state: measure the interval between consecutive events.
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - time_last);
        sum_time += elapsed;
        time_last = now;
        ++nb_measured;
        s << " - interval " << elapsed.count() << " ms, average is " << get_avgtime().count() << " ms";
    }

    VSOMEIP_DEBUG << s.str();
}

void debounce_test_client::run_test() {
    // Trigger the test
    auto its_runtime = vsomeip::runtime::get();
    auto its_payload = its_runtime->create_payload();
    auto its_message = its_runtime->create_request(false);
    its_message->set_service(DEBOUNCE_SERVICE);
    its_message->set_instance(DEBOUNCE_INSTANCE);
    its_message->set_method(DEBOUNCE_START_METHOD);
    its_message->set_interface_version(DEBOUNCE_MAJOR);
    its_message->set_message_type(vsomeip::message_type_e::MT_REQUEST_NO_RETURN);
    its_message->set_payload(its_payload);
    app_->send(its_message);

    if (interval > 0) {
        // wait until we have collected enough steady-state samples
        const auto deadline = std::chrono::steady_clock::now() + common::scaled_timeout(std::chrono::seconds(15));
        while (nb_measured.load() < MEASURE_INTERVALS && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } else {
        // wait.. a while, to see that nothing appears
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void debounce_test_client::unsubscribe_all() {
    app_->unsubscribe(DEBOUNCE_SERVICE, DEBOUNCE_INSTANCE, DEBOUNCE_EVENTGROUP);
}

void debounce_test_client::stop_service() {
    auto its_runtime = vsomeip::runtime::get();
    auto its_payload = its_runtime->create_payload();
    auto its_message = its_runtime->create_request(false);
    its_message->set_service(DEBOUNCE_SERVICE);
    its_message->set_instance(DEBOUNCE_INSTANCE);
    its_message->set_method(DEBOUNCE_STOP_METHOD);
    its_message->set_interface_version(DEBOUNCE_MAJOR);
    its_message->set_message_type(vsomeip::message_type_e::MT_REQUEST_NO_RETURN);
    its_message->set_payload(its_payload);
    app_->send(its_message);
}

int64_t debounce_test_client::getNbMsgsRcvd() {
    return nb_msgs_rcvd;
}

std::chrono::milliseconds debounce_test_client::get_avgtime() {
    const int64_t measured = nb_measured.load();
    if (measured <= 0) {
        return std::chrono::milliseconds(0);
    }
    return (sum_time / measured);
}

TEST(debounce_test, normal_interval) {
    debounce_test_client its_client(DEBOUNCE_INTERVAL_1);
    ASSERT_TRUE(its_client.init());
    VSOMEIP_INFO << "Debounce client successfully initialized!";
    its_client.start();
    its_client.wait();

    // Average Interval should be within TOLERANCE_MS (scaled) of DEBOUNCE_INTERVAL_1
    const auto its_tolerance = (double)TOLERANCE_MS * common::get_timeout_scale();
    EXPECT_GE(its_client.get_avgtime().count(), (double)DEBOUNCE_INTERVAL_1 - its_tolerance);
    EXPECT_LE(its_client.get_avgtime().count(), (double)DEBOUNCE_INTERVAL_1 + its_tolerance);
}

TEST(debounce_test, large_interval) {
    debounce_test_client its_client(DEBOUNCE_INTERVAL_2);
    ASSERT_TRUE(its_client.init());
    VSOMEIP_INFO << "Debounce client successfully initialized!";
    its_client.start();
    its_client.wait();

    // Average Interval should be within TOLERANCE_MS (scaled) of DEBOUNCE_INTERVAL_2
    const auto its_tolerance = (double)TOLERANCE_MS * common::get_timeout_scale();
    EXPECT_GE(its_client.get_avgtime().count(), (double)DEBOUNCE_INTERVAL_2 - its_tolerance);
    EXPECT_LE(its_client.get_avgtime().count(), (double)DEBOUNCE_INTERVAL_2 + its_tolerance);
}

TEST(debounce_test, disable) {
    debounce_test_client its_client(DEBOUNCE_INTERVAL_3);
    ASSERT_TRUE(its_client.init());
    VSOMEIP_INFO << "Debounce Client successfully initialized!";
    its_client.start();
    its_client.wait();

    // With a debounce interval disabled (-1), the client is expected to not receive any message
    EXPECT_EQ(its_client.getNbMsgsRcvd(), 0);
}

int main(int argc, char** argv) {
    return test_main(argc, argv);
}
