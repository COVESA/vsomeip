// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <gtest/gtest.h>
#include <thread>

#include "vsomeip/application.hpp"
#include "vsomeip/enumeration_types.hpp"
#include "vsomeip/runtime.hpp"
#include "common/timeout_detector.hpp"
#include "common/test_main.hpp"
#include "tools/tools.h"

// Global application instance.
// Its destruction is deferred until static teardown at process exit.
static std::shared_ptr<vsomeip_v3::application> app;

TEST(dispatch_app_stop, outlives_global) {
    /**
     * Validates the same shutdown sequence as outlives_program, with a
     * different ownership model for the application object.
     *
     * Shutdown sequence under test:
     *
     *   1. Dispatcher invokes the state handler (ST_REGISTERED).
     *   2. The handler stops the application.
     *   3. The handler joins the thread executing app->start().
     *   4. The handler signals the main test thread.
     *   5. The handler continues running briefly after signaling.
     *
     * The distinction in this test is that the application is stored in a
     * global static shared_ptr. Therefore, its destruction is tied to process
     * teardown rather than the end of the test scope.
     *
     * This ensures that:
     *
     *   - Stopping and joining from the dispatcher context does not deadlock.
     *   - Overlapping callback execution with main thread return is safe.
     *   - Global/static destruction during process shutdown does not cause
     *     use-after-free or race conditions with dispatcher activity.
     */
    auto cv = std::make_shared<std::condition_variable>();
    std::mutex mt;

    auto thread_ctor_cv = std::make_shared<std::condition_variable>();
    auto thread_ctor_mt = std::make_shared<std::mutex>();
    auto thread_ctor_gate = std::make_shared<std::atomic_bool>(false);

    const std::string app_name = "dispatch_app_outlives_global";
    ASSERT_EQ(create_config(app_name), 0);

    // Reset and recreate the global application instance explicitly to
    // ensure a clean test state.
    app = nullptr;
    app = vsomeip_v3::runtime::get()->create_application(app_name);
    app->init();

    // Thread executing app->start(); explicitly joined from dispatcher.
    std::thread t0;

    app->register_state_handler([a = app, &t0, cv, thread_ctor_cv, thread_ctor_mt, thread_ctor_gate](vsomeip_v3::state_type_e state) {
        if (state == vsomeip_v3::state_type_e::ST_REGISTERED) {
            std::cout << "[TEST] Stopping application" << std::endl;

            // Remove handlers to release objects held by the lambda
            a->clear_all_handler();

            // Stop application from within the dispatcher thread.
            a->stop();

            // Blocks until t0 constructor properly finishes
            std::unique_lock lock{*thread_ctor_mt};
            thread_ctor_cv->wait(lock, [thread_ctor_gate] { return thread_ctor_gate->load(); });

            // Join the start() thread from dispatcher context. This is a
            // critical part of the test and would expose incorrect internal
            // locking or thread ownership.
            std::cout << "[TEST] Joining t0" << std::endl;
            t0.join();

            // Notify main thread that shutdown + join completed.
            // The callback deliberately continues executing afterward to
            // overlap with test teardown and static lifetime handling.
            std::cout << "[TEST] Sleeping inside dispatcher thread" << std::endl;
            cv->notify_one();

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            std::cout << "[TEST] Waking up dispatcher thread" << std::endl;
        }
    });

    // Launch application event loop in a dedicated thread.
    std::cout << "[TEST] Starting t0 thread" << std::endl;
    t0 = std::thread([a = app] { a->start(); });
    thread_ctor_gate->store(true);
    thread_ctor_cv->notify_one();

    // Wait until the dispatcher has completed stop() and join().
    // Successful return implies no deadlock, crash, or undefined behavior
    // despite overlapping global lifetime and callback execution.
    std::unique_lock lock{mt};
    cv->wait(lock);

    std::cout << "[TEST] Returning on main" << std::endl;
}

int main(int argc, char** argv) {
    return test_main(argc, argv);
}
