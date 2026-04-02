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
#include <thread>

#include "offer_test_globals.hpp"

using namespace offer_test;

TEST(OfferTestExternal, OfferTestExternalRemoteService) {

    // Initialize the runtime
    auto runtime = vsomeip::runtime::get();
    ASSERT_TRUE(runtime) << "Should create a vsomeip runtime.";

    // Initialize the application
    auto application = runtime->create_application("remote-service");
    ASSERT_TRUE(application) << "Should create a vsomeip application.";
    ASSERT_TRUE(application->init()) << "Should initialize application.";

    // Offer the test service
    application->offer_service(service.service_id, service.instance_id);
    application->offer_event(service.service_id, service.instance_id, service.event_id, {service.eventgroup_id},
                             vsomeip_v3::event_type_e::ET_EVENT);

    // Register subscription handler
    application->register_subscription_handler(service.service_id, service.instance_id, service.eventgroup_id,
                                               [&](vsomeip::client_t, std::uint32_t, std::uint32_t, const std::string&, bool) {
                                                   std::filesystem::path remote_service_subscribed_filename =
                                                           std::filesystem::current_path() / "remote_service_subscribed.flag";
                                                   std::ofstream file(remote_service_subscribed_filename);
                                                   // Do not assert file creation as this lambda needs to return true;
                                                   return true;
                                               });

    // Start the vsomeip application
    std::thread start_thread([&application] { application->start(); });

    // Wait for the service to be subscribed before stopping the test
    // (provided by either the local or remote service)
    std::filesystem::path local_service_subscribed_filename = std::filesystem::current_path() / "local_service_subscribed.flag";
    std::filesystem::path remote_service_subscribed_filename = std::filesystem::current_path() / "remote_service_subscribed.flag";
    while (!std::filesystem::exists(local_service_subscribed_filename) && !std::filesystem::exists(remote_service_subscribed_filename)) {
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
