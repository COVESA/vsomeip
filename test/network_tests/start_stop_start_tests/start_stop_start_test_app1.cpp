// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <future>
#include <gtest/gtest.h>

#include "start_stop_start_test_globals.hpp"
#include "vsomeip/enumeration_types.hpp"
#include <common/vsomeip_app.hpp>

namespace start_stop_start {
namespace app1 {

using namespace common;

TEST(start_stop_start_test_app1, start_stop_start_application) {
    // ------------------------------------------------------------------------------------------
    //  SETUP
    // ------------------------------------------------------------------------------------------
    auto shm = shared_memory_slave_t<state_t>(std::string(start_stop_start::TEST_NAME));

    auto& queue = shm.get_queue(start_stop_start::APP1_IDX);

    std::promise<bool> has_registered;

    auto test_app1 = base_vsip_app_builder("start_stop_start_test_app1", "RDTS")
                             .with_state_callback([&](vsomeip::state_type_e state) {
                                 if (state == vsomeip::state_type_e::ST_REGISTERED) {
                                     has_registered.set_value(true);
                                 }
                                 std::cout << "APPLICATION 1 GOT "
                                           << (state == vsomeip::state_type_e::ST_DEREGISTERED ? "ST_DEREGISTERED" : "ST_REGISTERED")
                                           << std::endl;
                             })
                             .auto_start(false) // Application does not start automatically.
                             .build();

    test_scenario_t<state_t> scenario(start_stop_start::N_TEST_STEPS);

    scenario.add_step(Steps::_1, "APPLICATION 1 Launch",
                      [&]() {
                          test_app1->start();
                          auto val = has_registered.get_future().get();
                          std::cout << "APPLICATION 1 registered=" << val << std::endl;
                          EXPECT_TRUE(val);
                          queue.send(val ? state_t::REGISTERED : state_t::DEREGISTERED);
                      })
            .add_step(Steps::_2, "APPLICATION 1 Stop",
                      [&]() {
                          test_app1->only_stop();
                          test_app1->join();
                          // KLUDGE(dsantiago): testing on some valgrind types delays stop process, such that
                          // introducing this delay fixes odd, sporadic behaviours in the stop process, stabilizing the test.
                          // It was verified that when running the test in a loop, sporadically, in the scenario where we only have one
                          // application, it would break/hang without this sleep for the valgrinds, since the process is slower. I suspect
                          // there was still some stop process happening in the background when the application was starting again.
                          std::this_thread::sleep_for(std::chrono::milliseconds(100));
                          has_registered = {};
                          queue.send(state_t::DEREGISTERED);
                      })
            .add_step(Steps::_4, "APPLICATION 1 started again",
                      [&]() {
                          test_app1->only_start();
                          auto val = has_registered.get_future().get();
                          std::cout << "APPLICATION 1 registered again=" << val << std::endl;
                          EXPECT_TRUE(val);
                          queue.send(val ? state_t::REGISTERED : state_t::DEREGISTERED);
                      })
            .add_step(Steps::_5, "APPLICATION 1 Stop the application",
                      [&]() {
                          test_app1->stop();
                          std::this_thread::sleep_for(std::chrono::milliseconds(100));
                          queue.send(state_t::STOPPED);
                      })
            .run(shm, "APPLICATION_1");

    std::cout << "APPLICATION 1 TEST IS FINISHING" << std::endl;
} // TEST
} // namespace app1
} // namespace start_stop_start

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
