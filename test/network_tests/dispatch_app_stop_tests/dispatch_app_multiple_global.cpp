// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/>.

#include <condition_variable>
#include <mutex>
#include <thread>
#include <future>
#include <array>

#include "vsomeip/application.hpp"
#include "vsomeip/enumeration_types.hpp"
#include "vsomeip/runtime.hpp"
#include "common/timeout_detector.hpp"
#include "common/test_main.hpp"
#include "tools/tools.h"

// Global application instances. Lifetime extends until process teardown.
static std::array<std::shared_ptr<vsomeip_v3::application>, 3> apps;

TEST(dispatch_app_stop, multiple_global) {
    /**
     * Validates dispatcher-driven shutdown with multiple applications
     * running concurrently in the same process, where application
     * objects are globally owned.
     *
     * Test model:
     *
     *   - Create and start N applications (app_count) in one process.
     *   - Each application runs start() in its own thread.
     *   - On ST_REGISTERED, each dispatcher callback:
     *        - clears handlers,
     *        - stops its application,
     *        - joins its corresponding start() thread,
     *        - increments a shared completion counter,
     *        - continues briefly after notifying main.
     *
     * Compared to multiple_program, the application objects are stored
     * in a global static array. This exercises the same shutdown ordering
     * under static/global ownership.
     *
     * The test ensures:
     *
     *   - Concurrent dispatcher callbacks can safely stop their own app.
     *   - Joining start() threads from dispatcher context does not deadlock.
     *   - Overlap between main thread return and dispatcher execution
     *     remains safe with globally owned applications.
     */

    auto cv = std::make_shared<std::condition_variable>();
    auto mt = std::make_shared<std::mutex>();
    auto registered = std::make_shared<std::size_t>(0);

    constexpr std::size_t app_count = 3;
    const std::string app_name_prefix = "dispatch_app_multiple_global";
    std::array<std::string, app_count> app_names{};

    for (std::size_t i = 0; i < app_names.size(); ++i) {
        app_names[i] = app_name_prefix + "_" + std::to_string(i);
    }

    /**
     * All applications run in the same process, but each must behave as an
     * independent routing host. vsomeip selects configuration by application
     * name, so a dedicated configuration file is generated per application.
     *
     * A single base configuration is used as a template to avoid maintaining
     * multiple near-identical configuration files in the repository.
     */

    for (std::size_t i = 0; i < app_names.size(); ++i) {
        ASSERT_EQ(create_config(app_names[i]), 0);
    }

    std::array<std::promise<void>, app_count> its_promises{};
    std::array<std::shared_future<void>, app_count> its_futures{};
    for (std::size_t i = 0; i < app_count; ++i) {
        its_futures[i] = its_promises[i].get_future().share();
    }

    std::array<std::thread, app_names.size()> threads{};

    // Create and initialize all applications.
    for (std::size_t i = 0; i < app_names.size(); ++i) {
        apps[i] = nullptr;
        apps[i] = vsomeip_v3::runtime::get()->create_application(app_names[i]);
        apps[i]->init();
    }

    // Register shutdown handler for each globally owned application.
    for (std::size_t i = 0; i < apps.size(); ++i) {
        auto app = apps[i];

        app->register_state_handler([app, &threads, i, cv, mt, registered](vsomeip_v3::state_type_e state) {
            if (state == vsomeip_v3::state_type_e::ST_REGISTERED) {
                std::cout << "[TEST] Stopping application " << i << std::endl;

                app->clear_all_handler();
                app->stop();

                // Join corresponding start() thread from dispatcher context.
                // Multiple dispatcher threads may execute this concurrently.
                std::cout << "[TEST] Joining t" << i << std::endl;

                if (threads[i].joinable()) {
                    threads[i].join();
                }

                // Notify main that this instance completed stop + join.
                {
                    std::scoped_lock lock{*mt};
                    (*registered)++;
                }
                cv->notify_one();

                // Keep dispatcher callback alive briefly after signaling.
                // This enforces overlap with main thread return.
                std::cout << "[TEST] Sleeping inside dispatcher thread " << i << std::endl;

                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                std::cout << "[TEST] Waking up dispatcher thread " << i << std::endl;
            }
        });
    }

    // Start all applications in separate threads.
    for (std::size_t i = 0; i < apps.size(); ++i) {
        threads[i] = std::thread([app = apps[i], future = its_futures[i]] {
            future.wait();
            app->start();
        });
    }

    for (std::size_t i = 0; i < app_count; ++i) {
        its_promises[i].set_value();
    }

    // Wait until all dispatcher callbacks completed stop + join.
    std::unique_lock lock{*mt};
    EXPECT_TRUE(cv->wait_for(lock, std::chrono::seconds(5), [registered] {
        std::cout << "[TEST] " << *registered << " applications finished" << std::endl;
        return (*registered > 2);
    })) << "Application did not reach ST_REGISTERED";

    // Success means all instances shut down cleanly without deadlock
    // or crash under concurrent dispatcher-driven joins.
    std::cout << "[TEST] Returning on main" << std::endl;
}

int main(int argc, char** argv) {
    return test_main(argc, argv);
}
