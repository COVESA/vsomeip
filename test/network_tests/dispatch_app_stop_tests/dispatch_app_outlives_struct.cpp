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

/**
 * Global owner for the application and its start() thread.
 *
 * The destructor performs shutdown (stop + join). Because this instance is
 * static, destruction occurs during process teardown, after main() returns.
 */
struct app_holder {
    std::shared_ptr<vsomeip_v3::application> app;
    std::shared_ptr<std::thread> t0;

    ~app_holder() {
        // Shutdown is intentionally deferred to static destruction.
        // This models a lifetime where cleanup is not driven by the
        // dispatcher callback or test body, but by object teardown.
        std::cout << "[TEST] Stopping application" << std::endl;

        if (app) {
            app->stop();
        }

        if (t0 && t0->joinable()) {
            // Join the start() thread during static destruction.
            // This validates that shutdown remains safe even when
            // performed at process teardown time.
            std::cout << "[TEST] Joining t0" << std::endl;
            t0->join();
        }
    }
};

// Static instance: destroyed after main() exits.
static struct app_holder holder;

TEST(dispatch_app_stop, outlives_struct) {
    /**
     * Validates a deferred-lifetime shutdown model:
     *
     *   - The application and its start() thread are owned by a static struct.
     *   - The test waits only for ST_REGISTERED.
     *   - No explicit stop() or join() is performed in the test body.
     *   - Cleanup occurs later in the global struct destructor.
     *
     * Compared to the other variants:
     *
     *   - outlives_program: shutdown happens inside the dispatcher callback.
     *   - outlives_global:  shutdown still in callback, but app has static lifetime.
     *   - outlives_struct:  shutdown is fully deferred to static destruction.
     *
     * This ensures:
     *
     *   - Returning from the test while the application is running is safe.
     *   - stop() and join() executed during static teardown do not deadlock or crashes.
     *   - No race conditions occur between dispatcher activity and
     *     process-level destruction.
     */
    auto cv = std::make_shared<std::condition_variable>();
    auto mt = std::make_shared<std::mutex>();
    auto registered = std::make_shared<bool>(false);

    const std::string app_name = "dispatch_app_outlives_struct";
    ASSERT_EQ(create_config(app_name), 0);

    // Reset and recreate the application instance.
    holder.app = nullptr;
    holder.app = vsomeip_v3::runtime::get()->create_application(app_name);
    holder.app->init();

    holder.app->register_state_handler([cv, mt, registered](vsomeip_v3::state_type_e state) {
        if (state == vsomeip_v3::state_type_e::ST_REGISTERED) {
            // Signal that initialization and registration completed.
            std::lock_guard<std::mutex> lock(*mt);
            *registered = true;
            cv->notify_one();
        }
    });

    // Start the application event loop.
    holder.t0 = std::make_shared<std::thread>([a = holder.app] { a->start(); });

    // Wait until the application reaches ST_REGISTERED.
    // The timeout prevents indefinite blocking in case of failure.
    std::unique_lock<std::mutex> lock{*mt};
    EXPECT_TRUE(cv->wait_for(lock, std::chrono::seconds(5), [registered] { return *registered; }))
            << "Application did not reach ST_REGISTERED";

    // Test body exits without stopping the application.
    // Actual shutdown occurs later during static destruction of `holder`.
    std::cout << "[TEST] Returning on main" << std::endl;
}

int main(int argc, char** argv) {
    return test_main(argc, argv, std::chrono::seconds(20));
}
