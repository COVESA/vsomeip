// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "tcp_user_timeout_test_globals.hpp"

using namespace tcp_user_timeout_test;

TEST(TcpUserTimeoutTest, ClientSubscribesToService) {

    // Variables to hold the register sequence
    std::mutex client_register_mutex;
    std::condition_variable client_register_cv;
    std::vector<vsomeip::state_type_e> client_register_state;

    // Initialize the runtime
    auto runtime = vsomeip::runtime::get();
    ASSERT_TRUE(runtime) << "Should create a vsomeip runtime.";

    // Initialize the application
    auto application = runtime->create_application("client-app");
    ASSERT_TRUE(application) << "Should create a vsomeip application.";
    ASSERT_TRUE(application->init()) << "Should initialize application.";

    application->register_state_handler(
            [&client_register_state, &client_register_mutex, &client_register_cv](vsomeip::state_type_e app_state) {
                std::scoped_lock scoped_lock(client_register_mutex);
                if (app_state == vsomeip_v3::state_type_e::ST_DEREGISTERED) {
                    // Once the deregister is detected, create the file to signal it to the script
                    std::filesystem::path filename = std::filesystem::current_path() / "connection_timeout.flag";
                    std::ofstream file(filename);
                    ASSERT_TRUE(file) << "Failed to create timeout file";
                }
                client_register_state.push_back(app_state);
                client_register_cv.notify_one();
            });

    // Request the test service
    application->request_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
    application->request_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, {EVENTGROUP_ID});
    application->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION);

    // Start the vsomeip application
    std::thread start_thread([&application] { application->start(); });

    // After the vsomeip application has started,
    // the break and resume of tcp communications
    // is detected through the state of the vsomeip app.
    // After the re-register is detected, end the test
    std::vector<vsomeip::state_type_e> expected_register_sequence{
            vsomeip_v3::state_type_e::ST_REGISTERED, vsomeip_v3::state_type_e::ST_DEREGISTERED, vsomeip_v3::state_type_e::ST_REGISTERED};
    {
        std::unique_lock<std::mutex> its_lock(client_register_mutex);
        client_register_cv.wait_for(its_lock, std::chrono::seconds(30), [&client_register_state, &expected_register_sequence] {
            return client_register_state == expected_register_sequence;
        });
    }
    EXPECT_EQ(client_register_state, expected_register_sequence);

    // Stop the application after the test is done
    application->stop();

    // Clean up worker thread
    if (start_thread.joinable()) {
        start_thread.join();
    }

    // Once the test is done, create the file to signal it to the server
    std::filesystem::path filename = std::filesystem::current_path() / "shutdown_test.flag";
    std::ofstream file(filename);
    ASSERT_TRUE(file) << "Failed to create shutdown file!";
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv, std::chrono::seconds(30));
}
#endif
