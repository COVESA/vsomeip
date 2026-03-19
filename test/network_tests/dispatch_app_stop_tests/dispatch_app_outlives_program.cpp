// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

TEST(dispatch_app_stop, outlives_program) {
    /**
     * Validates a specific shutdown ordering:
     *
     *   1. The dispatcher thread invokes the state handler (ST_REGISTERED).
     *   2. The handler stops the application.
     *   3. The handler joins the thread executing app->start().
     *   4. The handler signals the main test thread.
     *   5. The handler continues executing briefly after the signal.
     *
     * The critical property under test is that the main test thread is allowed
     * to return (and thus let the process tear down) while the dispatcher
     * callback is still running. This ensures that:
     *
     *   - Joining the start() thread from within the dispatcher context
     *     does not deadlock.
     *   - Stopping the application from inside its own state handler is safe.
     *   - The thread is able to finish before the program is really finished.
     */
    auto cv = std::make_shared<std::condition_variable>();
    std::mutex mt;

    auto thread_assign_cv = std::make_shared<std::condition_variable>();
    auto thread_assign_mt = std::make_shared<std::mutex>();
    auto thread_assign_gate = std::make_shared<bool>(false);

    const std::string app_name = "dispatch_app_outlives_program";
    ASSERT_EQ(create_config(app_name), 0);

    // Create and initialize application.
    auto app = vsomeip_v3::runtime::get()->create_application(app_name);
    app->init();

    // Thread running app->start(); joined explicitly from the dispatcher.
    std::thread t0;

    app->register_state_handler([app, &t0, cv, thread_assign_cv, thread_assign_mt, thread_assign_gate](vsomeip_v3::state_type_e state) {
        if (state == vsomeip_v3::state_type_e::ST_REGISTERED) {
            std::cout << "[TEST] Stopping application" << std::endl;

            // Remove handlers to release objects held by the lambda
            app->clear_all_handler();

            // Stop application from within the dispatcher thread.
            app->stop();

            // Blocks until t0 constructor properly finishes
            std::unique_lock lock{*thread_assign_mt};
            thread_assign_cv->wait(lock, [thread_assign_gate] { return *thread_assign_gate; });

            // Intentionally join the start() thread from the dispatcher context.
            // This exercises a shutdown path that could deadlock if internal
            // locking or thread ownership is incorrect.
            std::cout << "[TEST] Joining t0" << std::endl;
            t0.join();

            // Notify the main thread that the shutdown sequence completed.
            // The callback deliberately continues executing afterward to
            // overlap with main thread teardown.
            cv->notify_one();

            std::cout << "[TEST] Sleeping inside dispatcher thread" << std::endl;

            // Keep the dispatcher callback alive briefly after notification.
            // This forces the main thread to return while this callback
            // is still active, validating safe teardown under overlap.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            std::cout << "[TEST] Waking up dispatcher thread" << std::endl;
        }
    });

    // Start application event loop in a dedicated thread.
    std::cout << "[TEST] Starting t0 thread" << std::endl;
    t0 = std::thread([a = app] { a->start(); });
    {
        std::scoped_lock thread_assign_lock{*thread_assign_mt};
        *thread_assign_gate = true;
    }
    thread_assign_cv->notify_one();

    // Block until the dispatcher has completed the stop/join sequence.
    // The test is considered successful if we reach this point without
    // deadlock, crash, or undefined behavior.
    std::unique_lock lock{mt};
    cv->wait(lock);

    std::cout << "[TEST] Returning on main" << std::endl;
}

int main(int argc, char** argv) {
    return test_main(argc, argv);
}
