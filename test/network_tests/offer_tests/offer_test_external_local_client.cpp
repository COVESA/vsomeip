// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../someip_test_globals.hpp"
#include "common/test_main.hpp"

#include <fstream>
#include <filesystem>
#include <future>
#include <thread>

#include "offer_test_globals.hpp"

using namespace offer_test;

TEST(OfferTestExternal, OfferTestExternalLocalClient) {

    // Initialize the runtime
    auto runtime = vsomeip::runtime::get();
    ASSERT_TRUE(runtime) << "Should create a vsomeip runtime.";

    // Initialize the application
    auto application = runtime->create_application("local-client");
    ASSERT_TRUE(application) << "Should create a vsomeip application.";
    ASSERT_TRUE(application->init()) << "Should initialize application.";

    // Request the test service
    application->request_service(service.service_id, service.instance_id);
    application->request_event(service.service_id, service.instance_id, service.event_id, {service.eventgroup_id});

    // Create future and promise to know when service becomes unavailable
    std::promise<bool> is_unavailable_prom;
    std::future<bool> is_unavailable_fut = is_unavailable_prom.get_future();

    // Register the availability handler for the service
    application->register_availability_handler(
            service.service_id, service.instance_id, [&](vsomeip::service_t, vsomeip::instance_t, const bool is_available) {
                if (is_available) {
                    std::filesystem::path filename = std::filesystem::current_path() / "service_available.flag";
                    std::ofstream file(filename);
                    ASSERT_TRUE(file) << "Failed to create service available file!";
                } else {
                    is_unavailable_prom.set_value(true);
                }
            });

    // Start the vsomeip application
    std::thread start_thread([&application] { application->start(); });

    // After the service becomes available, wait 3 seconds to subscribe to it
    // so that the other service provider has time to try to offer the service
    std::filesystem::path service_available_filename = std::filesystem::current_path() / "service_available.flag";
    while (!std::filesystem::exists(service_available_filename)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    std::this_thread::sleep_for(std::chrono::seconds(3));

    application->subscribe(service.service_id, service.instance_id, service.eventgroup_id);

    // After the client subscribes to the service, wait until service provider
    // stops which will lead to the service becoming unavailable
    is_unavailable_fut.wait();

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
