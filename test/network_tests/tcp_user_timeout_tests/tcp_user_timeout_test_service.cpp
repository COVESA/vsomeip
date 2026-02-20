// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "tcp_user_timeout_test_globals.hpp"

using namespace tcp_user_timeout_test;

TEST(TcpUserTimeoutTest, ServerOffersService) {
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
    std::mutex service_register_mutex;
    std::condition_variable service_register_cv;
    bool service_register_state = false;
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

    // Create the payload to be notified
    auto payload_ = runtime->create_payload();

    // Huge data array in order to fill vsomeip host socket queue
    std::vector<vsomeip::byte_t> its_data;

    // Fill the data array
    for (int i = 0; i < 1000000; ++i) {
        its_data.push_back(static_cast<vsomeip::byte_t>(255));
    }

    // Don't notify before application is registered
    // (prevents race condition on sec client)
    {
        std::unique_lock<std::mutex> its_lock(service_register_mutex);
        service_register_cv.wait_for(its_lock, std::chrono::seconds(30),
                                     [&service_register_state] { return service_register_state == true; });
    }

    // While the socket queue is not filled, notify
    std::filesystem::path socket_queue_filename = std::filesystem::current_path() / "socket_queue_filled.flag";
    while (!std::filesystem::exists(socket_queue_filename)) {
        // Set payload and notify service/event
        payload_->set_data(its_data);
        application->notify(SERVICE_ID, INSTANCE_ID, EVENT_ID, payload_);
    }

    // Wait for client to re-register before stopping app that is acting as host
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
