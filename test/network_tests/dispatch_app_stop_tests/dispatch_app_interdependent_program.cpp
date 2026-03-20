// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include "common/test_main.hpp"
#include "vsomeip/application.hpp"
#include "vsomeip/enumeration_types.hpp"
#include "vsomeip/runtime.hpp"
#include "tools/tools.h"

using namespace std::chrono_literals;

TEST(dispatch_app_stop, interdependent_program) {
    /**
     * Reproduces the edge case that motivated storing dispatcher threads in
     * thread_manager::thread_map_ instead of a single thread slot.
     *
     * Sequence under test:
     *   - T0 calls app_0->start().
     *   - T1 calls app_1->start().
     *   - DT0 stops app_0, joins T0, then blocks on its_bool.
     *   - DT1 stops app_1, joins T1, then sets its_bool = true.
     *
     * With the previous single-thread handover, T1 would try to replace DT0 in
     * thread_manager and join it first. Because DT0 is waiting for its_bool, DT1
     * would then wait for T1 forever and the shutdown sequence deadlocks.
     */
    constexpr auto handler_timeout = 5s;
    constexpr auto test_timeout = 10s;

    auto its_cv = std::make_shared<std::condition_variable>();
    auto its_mutex = std::make_shared<std::mutex>();
    auto its_bool = std::make_shared<bool>(false);

    auto app_0_wait_cv = std::make_shared<std::condition_variable>();
    auto app_0_wait_mutex = std::make_shared<std::mutex>();
    auto app_0_waiting = std::make_shared<bool>(false);

    auto completed_cv = std::make_shared<std::condition_variable>();
    auto completed_mutex = std::make_shared<std::mutex>();
    auto completed_handlers = std::make_shared<uint8_t>(0);

    auto thread_assign_cv = std::make_shared<std::condition_variable>();
    auto thread_assign_mt = std::make_shared<std::mutex>();
    auto thread_assign_gate = std::make_shared<bool>(false);

    const std::string app_0_name = "dispatch_app_interdependent_program_0";
    const std::string app_1_name = "dispatch_app_interdependent_program_1";
    ASSERT_EQ(create_config(app_0_name), 0);
    ASSERT_EQ(create_config(app_1_name), 0);

    auto app_0 = vsomeip_v3::runtime::get()->create_application(app_0_name);
    auto app_1 = vsomeip_v3::runtime::get()->create_application(app_1_name);
    app_0->init();
    app_1->init();

    auto t0 = std::make_shared<std::thread>();
    auto t1 = std::make_shared<std::thread>();

    app_0->register_state_handler([app = app_0, t0, its_cv, its_mutex, its_bool, app_0_wait_cv, app_0_waiting, app_0_wait_mutex,
                                   completed_cv, completed_mutex, completed_handlers, handler_timeout, thread_assign_cv, thread_assign_mt,
                                   thread_assign_gate](vsomeip_v3::state_type_e state) {
        if (state != vsomeip_v3::state_type_e::ST_REGISTERED) {
            return;
        }

        std::cout << "[TEST] app_0 stopping from dispatcher" << std::endl;
        app->clear_all_handler();
        app->stop();

        {
            std::unique_lock thread_assign_lock{*thread_assign_mt};
            thread_assign_cv->wait(thread_assign_lock, [thread_assign_gate] { return *thread_assign_gate; });
        }

        std::cout << "[TEST] app_0 joining T0" << std::endl;
        if (t0->joinable()) {
            t0->join();
        }

        {
            std::scoped_lock wait_lock{*app_0_wait_mutex};
            *app_0_waiting = true;
        }
        app_0_wait_cv->notify_one();

        {
            std::unique_lock its_lock{*its_mutex};
            EXPECT_TRUE(its_cv->wait_for(its_lock, handler_timeout, [its_bool] { return *its_bool; }))
                    << "app_0 did not receive app_1 shutdown completion";
        }

        {
            std::scoped_lock completed_lock{*completed_mutex};
            (*completed_handlers)++;
        }
        completed_cv->notify_one();
    });

    app_1->register_state_handler([app = app_1, t1, its_cv, its_bool, its_mutex, app_0_wait_cv, app_0_wait_mutex, app_0_waiting,
                                   completed_cv, completed_mutex, completed_handlers, handler_timeout, thread_assign_cv, thread_assign_mt,
                                   thread_assign_gate](vsomeip_v3::state_type_e state) {
        if (state != vsomeip_v3::state_type_e::ST_REGISTERED) {
            return;
        }

        {
            std::unique_lock its_wait_lock{*app_0_wait_mutex};
            EXPECT_TRUE(app_0_wait_cv->wait_for(its_wait_lock, handler_timeout, [app_0_waiting] { return *app_0_waiting; }))
                    << "app_0 did not reach the wait on app_1 shutdown";
        }

        std::cout << "[TEST] app_1 stopping from dispatcher" << std::endl;

        app->clear_all_handler();
        app->stop();

        {
            std::unique_lock thread_assign_lock{*thread_assign_mt};
            thread_assign_cv->wait(thread_assign_lock, [thread_assign_gate] { return *thread_assign_gate; });
        }

        std::cout << "[TEST] app_1 joining T1" << std::endl;
        if (t1->joinable()) {
            t1->join();
        }

        {
            std::scoped_lock its_lock{*its_mutex};
            *its_bool = true;
        }
        its_cv->notify_one();

        {
            std::scoped_lock completed_lock{*completed_mutex};
            (*completed_handlers)++;
        }
        completed_cv->notify_one();
    });

    *t0 = std::thread([app = app_0] { app->start(); });
    *t1 = std::thread([app = app_1] { app->start(); });
    {
        std::scoped_lock thread_lock{*thread_assign_mt};
        *thread_assign_gate = true;
    }
    thread_assign_cv->notify_all();

    std::unique_lock completed_lock{*completed_mutex};
    EXPECT_TRUE(completed_cv->wait_for(completed_lock, test_timeout, [completed_handlers] { return *completed_handlers == 2; }))
            << "Interdependent dispatcher-driven shutdown did not finish";
}

int main(int argc, char** argv) {
    return test_main(argc, argv, std::chrono::seconds(20));
}
