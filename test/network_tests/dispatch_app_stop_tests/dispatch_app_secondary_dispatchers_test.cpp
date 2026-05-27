// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include "vsomeip/application.hpp"
#include "vsomeip/constants.hpp"
#include "vsomeip/enumeration_types.hpp"
#include "vsomeip/primitive_types.hpp"
#include "vsomeip/runtime.hpp"
#include "common/test_main.hpp"
#include "tools/tools.h"

using namespace std::chrono_literals;

namespace {
constexpr vsomeip_v3::service_t TEST_SERVICE_ID_1 = 0x1111;
constexpr vsomeip_v3::service_t TEST_SERVICE_ID_2 = 0x2222;
constexpr vsomeip_v3::service_t TEST_SERVICE_ID_3 = 0x3333;
constexpr vsomeip_v3::instance_t TEST_INSTANCE_ID = 0x0001;
} // namespace

TEST(dispatch, blocking_handler_spawns_secondary_dispatcher) {
    /**
     * Validates that a handler blocking beyond max_dispatch_time (300ms) causes
     * the timer callback to fire and spawn a secondary dispatcher thread, which
     * then picks up the next queued handler.
     */

    // Service application used to trigger the availability callbacks on the client.
    auto service = vsomeip_v3::runtime::get()->create_application("service-sample");
    ASSERT_TRUE(service->init());

    // Client used to test the dispatcher threads through the availability handlers.
    auto client = vsomeip_v3::runtime::get()->create_application("client-sample");
    ASSERT_TRUE(client->init());

    // These synchronize test steps in the callbacks.
    std::mutex mutex;
    std::condition_variable condvar;
    bool callback_called{false};

    // Register the first handler to block the main dispatcher.
    client->register_availability_handler(
            TEST_SERVICE_ID_1, TEST_INSTANCE_ID,
            [&mutex, &condvar, &callback_called, service](const vsomeip_v3::service_t _service, const vsomeip_v3::instance_t _instance,
                                                          const bool _is_available) {
                std::printf("availability{1}: service=%04x instance=%04x available=%d\n", _service, _instance, _is_available);
                EXPECT_EQ(_service, TEST_SERVICE_ID_1);
                EXPECT_EQ(_instance, TEST_INSTANCE_ID);
                if (!_is_available) {
                    return;
                }

                // Offer the test service to trigger the other callback.
                service->offer_service(TEST_SERVICE_ID_2, TEST_INSTANCE_ID);

                std::printf("availability{1}: service=%04x instance=%04x available=%d Waiting.\n", _service, _instance, _is_available);

                // Wait for the other callback to complete before unblocking.
                {
                    auto lock = std::unique_lock(mutex);
                    condvar.wait(lock, [&] { return callback_called; });
                }

                std::printf("availability{1}: service=%04x instance=%04x available=%d Exiting.\n", _service, _instance, _is_available);
            });
    client->request_service(TEST_SERVICE_ID_1, TEST_INSTANCE_ID);

    // Register the second handler to confirm that an extra dispatcher was called.
    client->register_availability_handler(
            TEST_SERVICE_ID_2, TEST_INSTANCE_ID,
            [&mutex, &condvar, &callback_called](const vsomeip_v3::service_t _service, const vsomeip_v3::instance_t _instance,
                                                 const bool _is_available) {
                std::printf("availability{2}: service=%04x instance=%04x available=%d\n", _service, _instance, _is_available);
                EXPECT_EQ(_service, TEST_SERVICE_ID_2);
                EXPECT_EQ(_instance, TEST_INSTANCE_ID);
                if (!_is_available) {
                    return;
                }

                // Confirm that the second callback was called while the first dispatcher was blocked.
                {
                    auto lock = std::unique_lock(mutex);
                    callback_called = true;
                }

                // Notify the first callback to unblock the dispatcher.
                condvar.notify_all();

                std::printf("availability{2}: service=%04x instance=%04x available=%d Exiting.\n", _service, _instance, _is_available);
            });
    client->request_service(TEST_SERVICE_ID_2, TEST_INSTANCE_ID);

    // Start message processing.
    auto service_worker = std::thread([service] { service->start(); });
    auto client_worker = std::thread([client] { client->start(); });

    // Offer the first service to start the test.
    service->offer_service(TEST_SERVICE_ID_1, TEST_INSTANCE_ID);

    // Wait for the second callback to be called.
    {
        auto lock = std::unique_lock(mutex);
        condvar.wait(lock, [&] { return callback_called; });
        EXPECT_TRUE(callback_called);
    }

    // Clean-up the test.
    client->clear_all_handler();
    client->stop();
    client_worker.join();
    service->stop();
    service_worker.join();
}

TEST(dispatch, max_dispatchers_limit_enforced) {
    /**
     * Validates that when all dispatcher slots are occupied (max_dispatchers=1
     * in config, meaning internal max = 2: main + 1 secondary), no additional
     * threads are spawned.
     */

    // Service application used to trigger the availability callbacks on the client.
    auto service = vsomeip_v3::runtime::get()->create_application("service-sample");
    ASSERT_TRUE(service->init());

    // Client used to test the dispatcher threads through the availability handlers.
    auto client = vsomeip_v3::runtime::get()->create_application("client-sample");
    ASSERT_TRUE(client->init());

    // These synchronize test steps in the callbacks.
    std::mutex mutex;
    std::condition_variable condvar;
    std::uint8_t active_callbacks{0};
    std::uint8_t max_callbacks{0};
    bool unblock{false};
    bool callback_called{false};

    // Callback used to block the dispatcher threads.
    auto blocking_callback = [&mutex, &condvar, &active_callbacks, &max_callbacks, &unblock](const vsomeip_v3::service_t _service,
                                                                                             const vsomeip_v3::instance_t _instance,
                                                                                             const bool _is_available) {
        std::printf("blocking_callback: service=%04x instance=%04x available=%d\n", _service, _instance, _is_available);
        if (!_is_available) {
            return;
        }

        // Track this callback.
        {
            auto lock = std::scoped_lock(mutex);
            active_callbacks += 1;
            max_callbacks = std::max(max_callbacks, active_callbacks);
        }

        // Notify the main thread.
        condvar.notify_all();

        std::printf("blocking_callback: service=%04x instance=%04x available=%d Waiting.\n", _service, _instance, _is_available);

        // Wait for the main thread to request that we unblock.
        {
            auto lock = std::unique_lock(mutex);
            condvar.wait(lock, [&] { return unblock; });

            // Untrack this callback.
            active_callbacks -= 1;
        }

        std::printf("blocking_callback: service=%04x instance=%04x available=%d Exiting.\n", _service, _instance, _is_available);
    };

    // Register the first and second handlers to block the dispatchers.
    client->register_availability_handler(TEST_SERVICE_ID_1, TEST_INSTANCE_ID, blocking_callback);
    client->register_availability_handler(TEST_SERVICE_ID_2, TEST_INSTANCE_ID, blocking_callback);
    client->request_service(TEST_SERVICE_ID_1, TEST_INSTANCE_ID);
    client->request_service(TEST_SERVICE_ID_2, TEST_INSTANCE_ID);

    // Register the third handler that will only run after the others unblock.
    client->register_availability_handler(
            TEST_SERVICE_ID_3, TEST_INSTANCE_ID,
            [&mutex, &condvar, &callback_called, &active_callbacks, &max_callbacks,
             service](const vsomeip_v3::service_t _service, const vsomeip_v3::instance_t _instance, const bool _is_available) {
                std::printf("availability: service=%04x instance=%04x available=%d\n", _service, _instance, _is_available);
                EXPECT_EQ(_service, TEST_SERVICE_ID_3);
                EXPECT_EQ(_instance, TEST_INSTANCE_ID);
                if (!_is_available) {
                    return;
                }

                {
                    auto lock = std::scoped_lock(mutex);

                    // Track this callback.
                    active_callbacks += 1;
                    max_callbacks = std::max(max_callbacks, active_callbacks);

                    // Confirm that this callback was called.
                    callback_called = true;
                }

                // Notify the main thread that this handler was called.
                condvar.notify_all();

                // Untrack this callback.
                {
                    auto lock = std::scoped_lock(mutex);
                    active_callbacks -= 1;
                }

                std::printf("availability: service=%04x instance=%04x available=%d Exiting.\n", _service, _instance, _is_available);
            });
    client->request_service(TEST_SERVICE_ID_3, TEST_INSTANCE_ID);

    // Start message processing.
    auto service_worker = std::thread([service] { service->start(); });
    auto client_worker = std::thread([client] { client->start(); });

    // Offer the first service to start the test.
    service->offer_service(TEST_SERVICE_ID_1, TEST_INSTANCE_ID);

    // Wait for the first callback to be called.
    {
        auto lock = std::unique_lock(mutex);
        condvar.wait(lock, [&] { return active_callbacks == 1; });
        EXPECT_EQ(active_callbacks, 1);
        EXPECT_EQ(max_callbacks, 1);
        EXPECT_FALSE(callback_called);
    }

    // Offer the second service to block the other dispatcher.
    service->offer_service(TEST_SERVICE_ID_2, TEST_INSTANCE_ID);

    // Wait for the second callback to be called.
    {
        auto lock = std::unique_lock(mutex);
        condvar.wait(lock, [&] { return active_callbacks == 2; });
        EXPECT_EQ(active_callbacks, 2);
        EXPECT_EQ(max_callbacks, 2);
        EXPECT_FALSE(callback_called);
    }

    // Offer the third service.
    service->offer_service(TEST_SERVICE_ID_3, TEST_INSTANCE_ID);

    // The third callback should remain blocked.
    {
        auto lock = std::unique_lock(mutex);
        condvar.wait_for(lock, std::chrono::milliseconds(1500)); // 5x max dispatch timer

        // Should still be blocked.
        EXPECT_EQ(active_callbacks, 2);
        EXPECT_EQ(max_callbacks, 2);
        EXPECT_FALSE(callback_called);

        // Unblock the dispatchers.
        unblock = true;
    }

    // Wake-up the blocked dispatchers.
    condvar.notify_all();

    // Wait for the third callback to be called.
    {
        auto lock = std::unique_lock(mutex);
        condvar.wait(lock, [&] { return callback_called; });
        EXPECT_LE(active_callbacks, 2);
        EXPECT_EQ(max_callbacks, 2);
        EXPECT_TRUE(callback_called);
    }

    // Clean-up the test.
    client->clear_all_handler();
    client->stop();
    client_worker.join();
    service->stop();
    service_worker.join();
}

TEST(dispatch, stop_while_secondary_dispatcher_running) {
    /**
     * Regression test for the RTBuildProxiesAndStubs scenario: calling stop()
     * while a secondary dispatcher thread is actively running a handler.
     *
     * This verifies that the shutdown sequence correctly handles:
     * - Stopping while a secondary dispatcher is blocked in a handler
     * - The secondary dispatcher thread being properly joined or saved
     * - No deadlock or crash during teardown
     */

    // Service application used to trigger the availability callbacks on the client.
    auto service = vsomeip_v3::runtime::get()->create_application("service-sample");
    ASSERT_TRUE(service->init());

    // Client used to test the dispatcher threads through the availability handlers.
    auto client = vsomeip_v3::runtime::get()->create_application("client-sample");
    ASSERT_TRUE(client->init());

    // These synchronize test steps in the callbacks.
    std::mutex mutex;
    std::condition_variable condvar;
    bool callback_called{false};
    bool stop{false};

    // Register the first handler to block the main dispatcher.
    client->register_availability_handler(
            TEST_SERVICE_ID_1, TEST_INSTANCE_ID,
            [&mutex, &condvar, &callback_called, &stop, client, service](const vsomeip_v3::service_t _service,
                                                                         const vsomeip_v3::instance_t _instance, const bool _is_available) {
                std::printf("availability{1}: service=%04x instance=%04x available=%d\n", _service, _instance, _is_available);
                EXPECT_EQ(_service, TEST_SERVICE_ID_1);
                EXPECT_EQ(_instance, TEST_INSTANCE_ID);
                if (!_is_available) {
                    return;
                }

                // Offer the test service to trigger the other callback.
                service->offer_service(TEST_SERVICE_ID_2, TEST_INSTANCE_ID);

                std::printf("availability{1}: service=%04x instance=%04x available=%d Waiting.\n", _service, _instance, _is_available);

                // Wait for the other callback to be called.
                {
                    auto lock = std::unique_lock(mutex);
                    condvar.wait(lock, [&] { return callback_called; });
                }

                // Stop the client while the other dispatcher is blocked.
                client->clear_all_handler();
                client->stop();

                // Signal the other dispatcher that we are stopping.
                {
                    std::scoped_lock lock(mutex);
                    stop = true;
                }
                condvar.notify_all();

                std::printf("availability{1}: service=%04x instance=%04x available=%d Exiting.\n", _service, _instance, _is_available);
            });
    client->request_service(TEST_SERVICE_ID_1, TEST_INSTANCE_ID);

    // Register the second handler to confirm that an extra dispatcher was called.
    client->register_availability_handler(
            TEST_SERVICE_ID_2, TEST_INSTANCE_ID,
            [&mutex, &condvar, &callback_called, &stop](const vsomeip_v3::service_t _service, const vsomeip_v3::instance_t _instance,
                                                        const bool _is_available) {
                std::printf("availability{2}: service=%04x instance=%04x available=%d\n", _service, _instance, _is_available);
                EXPECT_EQ(_service, TEST_SERVICE_ID_2);
                EXPECT_EQ(_instance, TEST_INSTANCE_ID);
                if (!_is_available) {
                    return;
                }

                // Confirm that the second callback was called while the first dispatcher was blocked.
                {
                    auto lock = std::unique_lock(mutex);
                    callback_called = true;
                }

                // Notify the first callback to unblock the dispatcher.
                condvar.notify_all();

                // Wait for the other dispatcher to stop the application.
                {
                    auto lock = std::unique_lock(mutex);
                    condvar.wait(lock, [&] { return stop; });
                }

                std::printf("availability{2}: service=%04x instance=%04x available=%d Waiting.\n", _service, _instance, _is_available);

                // Wait before unblocking.
                std::this_thread::sleep_for(10ms);

                std::printf("availability{2}: service=%04x instance=%04x available=%d Exiting.\n", _service, _instance, _is_available);
            });
    client->request_service(TEST_SERVICE_ID_2, TEST_INSTANCE_ID);

    // Start message processing.
    auto service_worker = std::thread([service] { service->start(); });
    auto client_worker = std::thread([client] { client->start(); });

    // Offer the first service to start the test.
    service->offer_service(TEST_SERVICE_ID_1, TEST_INSTANCE_ID);

    // Wait for the client to join the dispatcher threads.
    client_worker.join();

    // Clean-up the test.
    service->stop();
    service_worker.join();

    ASSERT_TRUE(callback_called);
    ASSERT_TRUE(stop);
}

int main(int argc, char** argv) {
    return test_main(argc, argv, std::chrono::seconds(20));
}
