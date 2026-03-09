#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "vsomeip/application.hpp"
#include "vsomeip/enumeration_types.hpp"
#include "vsomeip/runtime.hpp"
#include <gtest/gtest.h>
#include <unistd.h>
#include "common/test_main.hpp"

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
    auto app = vsomeip_v3::runtime::get()->create_application("test_application");
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
    app->register_state_handler([&app, &t0, &finished, &finished_mutex, &done](vsomeip_v3::state_type_e state) {
        if (state == vsomeip_v3::state_type_e::ST_REGISTERED) {
            app->clear_all_handler();
            app->stop();

            // Wait for the start thread to finish, and notify that we are done to the test thread.
            t0.join(); // NOTE: This operation takes a while, around 1s locally.
            // From here on out, t0 has joined which proves no deadlock. Finish the test.
            auto lock = std::lock_guard<std::mutex>(finished_mutex);
            done = true;
            finished.notify_one();
        }
    });

    t0 = std::thread([&app] {
        app->start(); /* Blocks */
    });

    // If this wait hangs, it means:
    // - The state handler never completed (likely deadlocked)
    // - The start thread never finished (likely waiting for dispatcher threads)
    // - The test will timeout, indicating the deadlock bug has returned
    auto lock = std::unique_lock(finished_mutex);
    EXPECT_TRUE(finished.wait_for(lock, 5s, [&] { return done; })) << "Calling stop on dispatcher thread should not cause a deadlock";
}

int main(int argc, char** argv) {
    return test_main(argc, argv);
}
