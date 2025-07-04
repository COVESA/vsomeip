// Copyright(C) 2015 - 2025 Bayerische Motoren Werke Aktiengesellschaft(BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v.2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http: // mozilla.org/MPL/2.0/.
#include "registration_check_service.hpp"

TEST(registration_check_test, test) {
    // Test steps:
    // 1: Service application initializes correctly
    service service_app;
    ASSERT_TRUE(service_app.init());
    service_app.start();

    test_timer_t test_timer(TEST_TIMEOUT);

    // 2: Wait for the service app to register to the routing host
    while (!test_timer.has_elapsed()) {
        if (service_app.is_registered()) break;
        std::this_thread::sleep_for(SLEEP_TIME);
    }
    // 3: Validate that the service app is registered
    EXPECT_TRUE(service_app.is_registered()) << "Service app is not registered to the routing host";
}

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif // REGISTRATION_CHECK_TEST_CPP
