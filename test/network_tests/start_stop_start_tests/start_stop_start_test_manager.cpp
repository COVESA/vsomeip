// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>

#include <gtest/gtest.h>
#include <thread>
#include <vsomeip/internal/logger.hpp>
#include <vsomeip/vsomeip.hpp>
#include "common/interprocess.hpp"
#include "common/process.hpp"
#include "common/test_main.hpp"
#include "start_stop_start_test_globals.hpp"

namespace start_stop_start {
namespace manager {

class start_stop_start_test_manager : public testing::Test {
protected:
    void SetUp() {
        VSOMEIP_INFO << "Setting up start_stop_start_test_manager";
        boost::interprocess::shared_memory_object::remove(start_stop_start::TEST_NAME.data());
    }
    void TearDown() { VSOMEIP_INFO << "Tearing down start_stop_start_test_manager"; }
};

TEST_F(start_stop_start_test_manager, start_stop_start_with_one_application) {
    auto shm =
            shared_memory_master_t<start_stop_start::state_t>(std::string(start_stop_start::TEST_NAME), start_stop_start::N_COMPONENTS_T1);

    auto& app1_queue = shm.get_queue(start_stop_start::APP1_IDX);
    int receiver_timeout_ms = 10000;

    process_group_t group;
    group.define_type("application1", "./start_stop_start_test_app1",
                      [](auto& env) { env["VSOMEIP_CONFIGURATION"] = "start_stop_start_test.json"; })
            .add_daemon("start_stop_start_test.json")
            .add_process("APPLICATION1", "application1");

    test_scenario_t<start_stop_start::state_t> scenario(start_stop_start::N_TEST_STEPS);

    scenario.add_step(Steps::_1, "#1 - LAUNCH APPLICATION 1",
                      [&]() {
                          // NOTE: might receive DEREGISTERED multiple times, app can start before the daemon, fail to register for a bit..
                          ASSERT_TRUE(app1_queue.receive_until(start_stop_start::state_t::REGISTERED, receiver_timeout_ms));
                          std::cout << "MANAGER GOT APPLICATION 1 REGISTERED" << std::endl;
                      })
            .add_step(Steps::_2, "#2 - STOP APPLICATION 1",
                      [&]() {
                          ASSERT_EQ(start_stop_start::state_t::DEREGISTERED, app1_queue.receive(receiver_timeout_ms));
                          std::cout << "MANAGER GOT APPLICATION 1 DEREGISTERED" << std::endl;
                      })
            .add_step(Steps::_4, "#3 - START APPLICATION 1 AGAIN",
                      [&]() {
                          ASSERT_EQ(start_stop_start::state_t::REGISTERED, app1_queue.receive(receiver_timeout_ms));
                          std::cout << "MANAGER GOT APPLICATION 1 REGISTERED AGAIN" << std::endl;
                      })
            .add_step(Steps::_5, "#4 - STOP APPLICATION 1. GOING DOWN",
                      [&]() { ASSERT_EQ(start_stop_start::state_t::STOPPED, app1_queue.receive(receiver_timeout_ms)); });
    try {
        std::cout << "MANAGER LAUNCHED ALL PROCESSES" << std::endl;
        group.start();
        std::cout << "GROUPS CONFIG SIZE" << group.configs_.size() << std::endl;
        scenario.run(shm, "MANAGER");
        std::cout << "MANAGER STOPPED ALL" << std::endl;
        group.stop();
    } catch (boost::interprocess::interprocess_exception& ex) {
        std::cout << ex.what() << std::endl;
    }
} // start_stop_start_with_one_application

TEST_F(start_stop_start_test_manager, start_stop_start_with_two_application) {
    auto shm =
            shared_memory_master_t<start_stop_start::state_t>(std::string(start_stop_start::TEST_NAME), start_stop_start::N_COMPONENTS_T2);

    auto& app1_queue = shm.get_queue(start_stop_start::APP1_IDX);
    auto& app2_queue = shm.get_queue(start_stop_start::APP2_IDX);
    int receiver_timeout_ms = 10000;

    process_group_t group;
    group.define_type("application1", "./start_stop_start_test_app1",
                      [](auto& env) { env["VSOMEIP_CONFIGURATION"] = "start_stop_start_test.json"; })
            .define_type("application2", "./start_stop_start_test_app2",
                         [](auto& env) { env["VSOMEIP_CONFIGURATION"] = "start_stop_start_test.json"; })
            .add_daemon("start_stop_start_test.json")
            .add_process("APPLICATION1", "application1")
            .add_process("APPLICATION2", "application2");

    test_scenario_t<start_stop_start::state_t> scenario(start_stop_start::N_TEST_STEPS);

    scenario.add_step(Steps::_1, "#1 - LAUNCH APPLICATION 1",
                      [&]() {
                          ASSERT_TRUE(app1_queue.receive_until(start_stop_start::state_t::REGISTERED, receiver_timeout_ms));
                          std::cout << "MANAGER GOT APPLICATION 1 REGISTERED" << std::endl;
                      })
            .add_step(Steps::_2, "#2 - STOP APPLICATION 1",
                      [&]() {
                          ASSERT_EQ(start_stop_start::state_t::DEREGISTERED, app1_queue.receive(receiver_timeout_ms));
                          std::cout << "MANAGER GOT APPLICATION 1 DEREGISTERED" << std::endl;
                      })
            .add_step(Steps::_3, "#3 - LAUNCH APPLICATION 2",
                      [&]() {
                          ASSERT_TRUE(app2_queue.receive_until(start_stop_start::state_t::REGISTERED, receiver_timeout_ms));
                          std::cout << "MANAGER GOT APPLICATION 2 REGISTERED" << std::endl;
                      })
            .add_step(Steps::_4, "#4 - START APPLICATION 1 AGAIN",
                      [&]() {
                          ASSERT_EQ(start_stop_start::state_t::REGISTERED, app1_queue.receive(receiver_timeout_ms));
                          std::cout << "MANAGER GOT APPLICATION 1 REGISTERED AGAIN" << std::endl;
                      })
            .add_step(Steps::_5, "#5 - STOP BOTH APPLICATIONS", [&]() {
                ASSERT_EQ(start_stop_start::state_t::STOPPED, app1_queue.receive(receiver_timeout_ms));
                ASSERT_EQ(start_stop_start::state_t::STOPPED, app2_queue.receive(receiver_timeout_ms));
                std::cout << "MANAGER GOT BOTH APPLICATIONS STOPPED" << std::endl;
            });
    try {
        std::cout << "MANAGER LAUNCHED ALL PROCESSES" << std::endl;
        group.start();
        std::cout << "GROUPS CONFIG SIZE" << group.configs_.size() << std::endl;
        scenario.run(shm, "MANAGER");
        std::cout << "MANAGER STOPPED ALL" << std::endl;
        group.stop();
    } catch (boost::interprocess::interprocess_exception& ex) {
        std::cout << ex.what() << std::endl;
    }
} // start_stop_start_with_two_application
} // namespace manager
} // namespace start_stop_start

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv);
}
#endif
