// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "multicast_group_test_globals.hpp"

using namespace multicast_group_test;

TEST(MulticastGroupTest, FirstClientSubscribesToService) {

    // Create variables to store paylaod and check if last msg is received correctly
    int current_payload = 0;
    int last_payload = 0;
    std::promise<bool> received_last_msg_prom;
    std::future<bool> received_last_msg_fut = received_last_msg_prom.get_future();

    // Initialize the runtime
    auto runtime = vsomeip::runtime::get();
    ASSERT_TRUE(runtime) << "Should create a vsomeip runtime.";

    // Initialize the application
    auto client_application = runtime->create_application("first-client-sample");
    ASSERT_TRUE(client_application) << "Should create a vsomeip application.";
    ASSERT_TRUE(client_application->init()) << "Should initialize application.";

    // When the client is registered, request the test service and event
    client_application->register_state_handler([&](vsomeip::state_type_e _state) {
        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            client_application->request_service(SERVICE_ID, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
            std::set<vsomeip::eventgroup_t> its_groups;
            its_groups.insert(EVENTGROUP_ID);
            client_application->request_event(SERVICE_ID, INSTANCE_ID, EVENT_ID, its_groups, vsomeip::event_type_e::ET_EVENT);
        }
    });

    // When the test service is available, subscribe to it
    client_application->register_availability_handler(
            SERVICE_ID, INSTANCE_ID, [&](vsomeip::service_t, vsomeip::instance_t, bool _is_available) {
                VSOMEIP_INFO << "Test service is:" << (_is_available ? " available" : "not available") << std::endl;
                client_application->subscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION);
            });

    // Check if the received payload is in order with the last one and if it is the last msg
    client_application->register_message_handler(SERVICE_ID, INSTANCE_ID, vsomeip::ANY_METHOD,
                                                 [&](const std::shared_ptr<vsomeip::message>& _message) {
                                                     auto payload = _message->get_payload();

                                                     current_payload = vsomeip_v3::bithelper::read_uint16_le(payload->get_data());

                                                     if ((last_payload == 0) || ((last_payload + 1) == current_payload)) {
                                                         VSOMEIP_DEBUG << "Received payload i=" << current_payload;
                                                         last_payload = current_payload;
                                                     } else if (current_payload == 65535) {
                                                         VSOMEIP_INFO << "Received last msg, ending test";
                                                         received_last_msg_prom.set_value(true);
                                                     } else if ((last_payload + 1) < current_payload) {
                                                         VSOMEIP_ERROR << "Failed to received a payload!";
                                                         received_last_msg_prom.set_value(false);
                                                     }
                                                 });

    // Start the vsomeip application
    std::thread worker_thread([client_application] { client_application->start(); });

    // Check if last msg was received
    received_last_msg_fut.wait();
    EXPECT_TRUE(received_last_msg_fut.get()) << "Failed to receive a notification!";

    // Unsubscribe to the service and stop the vsomeip application
    client_application->unsubscribe(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, EVENT_ID);
    client_application->stop();

    // Clean up worker thread
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
