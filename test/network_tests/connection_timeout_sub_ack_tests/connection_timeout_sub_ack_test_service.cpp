// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "connection_timeout_sub_ack_test_globals.hpp"

using namespace connection_timeout_sub_ack_test;

TEST(ConnectionTimeoutSubAckTest, ServerOffersService) {

    // Initialize the runtime
    auto runtime = vsomeip::runtime::get();
    ASSERT_TRUE(runtime) << "Should create a vsomeip runtime.";

    // Initialize the application
    auto application = runtime->create_application("service-app");
    ASSERT_TRUE(application) << "Should create a vsomeip application.";
    ASSERT_TRUE(application->init()) << "Should initialize application.";

    // Offer the test service
    application->offer_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
    application->offer_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, {EVENTGROUP_ID}, vsomeip_v3::event_type_e::ET_EVENT);

    // Only begin notifying after application is registered
    // (prevents race condition on sec client)
    bool service_register_state = false;
    std::mutex service_register_mutex;
    std::condition_variable service_register_cv;
    application->register_state_handler(
            [&service_register_state, &service_register_mutex, &service_register_cv](vsomeip_v3::state_type_e state) {
                if (state == vsomeip_v3::state_type_e::ST_REGISTERED) {
                    std::scoped_lock<std::mutex> its_lock(service_register_mutex);
                    service_register_state = true;
                    service_register_cv.notify_one();
                }
            });

    // Start the vsomeip application
    std::thread start_thread([&application] { application->start(); });

    // Only begin notifying after application is registered
    {
        std::unique_lock<std::mutex> its_lock(service_register_mutex);
        service_register_cv.wait_for(its_lock, std::chrono::seconds(30),
                                     [&service_register_state] { return service_register_state == true; });
    }

    // Notify once
    application->notify(SERVICE_ID, INSTANCE_ID, EVENT_ID, vsomeip::runtime::get()->create_payload({0x01}));

    // Wait for the client to create the shutdown file
    std::filesystem::path shutdown_filename = std::filesystem::current_path() / "shutdown_test.flag";
    while (!std::filesystem::exists(shutdown_filename)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Stop the application after the test is done
    application->stop();

    // Clean up thread
    if (start_thread.joinable()) {
        start_thread.join();
    }
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv, std::chrono::seconds(30));
}
#endif
