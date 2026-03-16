// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "vsomeip/application.hpp"
#include "vsomeip/enumeration_types.hpp"
#include "vsomeip/runtime.hpp"
#include <gtest/gtest.h>
#include "common/test_main.hpp"
#include "tools/tools.h"

using namespace std::chrono_literals;

TEST(dispatch_app_stop, deadlock) {
    /**
     * This test reproduces the following pattern:
     * std::thread t {[&]{ app->start(); }};
     * app->register_message_handler(..., [&]{
     *     app->stop(); // calling stop from dispatcher -> this is ok
     *     t.join(); // This can lead to a deadlock
     * });
     *
     * This is a common pattern for CommonAPI applications, since the API
     * pushes into the kind of uses where a reference to a proxy, and thus
     * the underlying vsomeip application pointer, is commonly passed as an
     * argument to the callbacks for methods.
     *
     * Therefore, emulate this pattern here and ensure an application can call
     * stop and join the start thread.
     */

    auto thread_ctor_cv = std::make_shared<std::condition_variable>();
    auto thread_ctor_mt = std::make_shared<std::mutex>();
    auto thread_ctor_gate = std::make_shared<std::atomic_bool>(false);

    const std::string app_name = "test_application";
    ASSERT_EQ(create_config(app_name), 0);

    auto app = vsomeip_v3::runtime::get()->create_application(app_name);
    app->init();

    // Synchronization primitives to detect when the start thread has completed
    // This allows us to verify that the shutdown sequence completes successfully
    // without hanging indefinitely (which would indicate a deadlock)
    std::condition_variable finished;
    std::mutex finished_mutex;
    bool done = false;
    std::thread t0;

    // Register a callback to a dispatcher thread. In this case, we just want to make sure
    // the application can register, before clearing it up.
    app->register_state_handler([&app, &t0, &finished, &finished_mutex, &done, thread_ctor_cv, thread_ctor_mt,
                                 thread_ctor_gate](vsomeip_v3::state_type_e state) {
        if (state == vsomeip_v3::state_type_e::ST_REGISTERED) {
            app->clear_all_handler();
            app->stop();

            // Blocks until t0 constructor properly finishes
            std::unique_lock thread_lock{*thread_ctor_mt};
            thread_ctor_cv->wait(thread_lock, [thread_ctor_gate] { return thread_ctor_gate->load(); });

            // Wait for the start thread to finish, and notify that we are done to the test thread.
            t0.join(); // NOTE: This operation takes a while, around 1s locally.
            // From here on out, t0 has joined which proves no deadlock. Finish the test.
            auto lock = std::lock_guard<std::mutex>(finished_mutex);
            done = true;
            finished.notify_one();
        }
    });

    std::cout << "[TEST] Starting t0 thread" << std::endl;
    t0 = std::thread([&app] {
        app->start(); /* Blocks */
    });
    thread_ctor_gate->store(true);
    thread_ctor_cv->notify_one();

    // If this wait hangs, it means:
    // - The state handler never completed (likely deadlocked)
    // - The start thread never finished (likely waiting for dispatcher threads)
    // - The test will timeout, indicating the deadlock bug has returned
    auto lock = std::unique_lock(finished_mutex);
    EXPECT_TRUE(finished.wait_for(lock, 5s, [&] { return done; })) << "Calling stop on dispatcher thread should not cause a deadlock";
}

int main(int argc, char** argv) {
    return test_main(argc, argv, std::chrono::seconds(10));
}
