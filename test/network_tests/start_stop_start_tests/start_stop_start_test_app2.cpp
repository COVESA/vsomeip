#include <common/vsomeip_app.hpp>
#include "start_stop_start_test_globals.hpp"

#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <string_view>

namespace start_stop_start {
namespace app2 {

using namespace common;

TEST(start_stop_start_test_app2, start_stop_application) {
    // ------------------------------------------------------------------------------------------
    //  SETUP
    // ------------------------------------------------------------------------------------------
    auto shm = shared_memory_slave_t<state_t>(std::string(start_stop_start::TEST_NAME));

    auto& queue = shm.get_queue(start_stop_start::APP2_IDX);

    std::promise<bool> has_registered;

    auto test_app2 = base_vsip_app_builder("start_stop_start_test_app2", "RDTC")
                             .with_state_callback([&](vsomeip::state_type_e state) {
                                 if (state == vsomeip::state_type_e::ST_REGISTERED) {
                                     has_registered.set_value(true);
                                 }
                                 std::cout << "APPLICATION 2 GOT "
                                           << (state == vsomeip::state_type_e::ST_DEREGISTERED ? "ST_DEREGISTERED" : "ST_REGISTERED")
                                           << std::endl;
                             })
                             .auto_start(false) // Application does not start automatically.
                             .build();

    test_scenario_t<state_t> scenario(start_stop_start::N_TEST_STEPS);

    scenario.add_step(Steps::_3, "APPLICATION 2 Launch",
                      [&]() {
                          test_app2->start();
                          auto val = has_registered.get_future().get();
                          std::cout << "APPLICATION 2 registered=" << val << std::endl;
                          EXPECT_TRUE(val);
                          queue.send(val ? state_t::REGISTERED : state_t::DEREGISTERED);
                      })
            .add_step(Steps::_5, "APPLICATION 2 Stop the application",
                      [&]() {
                          test_app2->stop();
                          std::this_thread::sleep_for(std::chrono::milliseconds(100));
                          queue.send(state_t::STOPPED);
                      })
            .run(shm, "APPLICATION_2");

    std::cout << "APPLICATION 2 TEST IS FINISHING" << std::endl;
} // TEST
} // namespace app2
} // namespace start_stop_start

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
