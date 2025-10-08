// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <limits>

#include "multicast_group_test_globals.hpp"

using namespace multicast_group_test;

TEST(MulticastGroupTest, SecondClientSubscribesToService) {

    // Create variables to store paylaod and check if last msg is received correctly
    std::mutex sync;
    int last_payload = 0;
    int counting_payload = std::numeric_limits<int>::max();
    int unicast_count = 0;
    int multicast_count = 0;
    int unicast_payload = 0;
    int multicast_payload = 0;
    std::promise<bool> received_last_msg_prom;
    std::future<bool> received_last_msg_fut = received_last_msg_prom.get_future();

    // Initialize the runtime
    auto runtime = vsomeip::runtime::get();
    ASSERT_TRUE(runtime) << "Should create a vsomeip runtime.";

    // Initialize the application
    auto client_application = runtime->create_application("second-client-sample");
    ASSERT_TRUE(client_application) << "Should create a vsomeip application.";
    ASSERT_TRUE(client_application->init()) << "Should initialize application.";

    // When the client is registered, request the test service and event
    client_application->register_state_handler([&](vsomeip::state_type_e _state) {
        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            client_application->request_service(SERVICE_ID_MULTICAST, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
            std::set<vsomeip::eventgroup_t> its_groups{EVENTGROUP_ID};
            client_application->request_event(SERVICE_ID_MULTICAST, INSTANCE_ID, EVENT_ID_MULTICAST, its_groups,
                                              vsomeip::event_type_e::ET_EVENT);
            client_application->request_service(SERVICE_ID_UNICAST, INSTANCE_ID, MAJOR_VERSION, MINOR_VERSION);
            client_application->request_event(SERVICE_ID_UNICAST, INSTANCE_ID, EVENT_ID_UNICAST, its_groups,
                                              vsomeip::event_type_e::ET_EVENT);
        }
    });

    // When the test service is available, subscribe to it
    client_application->register_availability_handler(
            SERVICE_ID_MULTICAST, INSTANCE_ID, [&](vsomeip::service_t, vsomeip::instance_t, bool _is_available) {
                VSOMEIP_INFO << "Test service is:" << (_is_available ? " available" : "not available") << std::endl;
                client_application->subscribe(SERVICE_ID_MULTICAST, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION);
                client_application->subscribe(SERVICE_ID_UNICAST, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION);
            });

    // Check if the received payload is in order with the last one and if it is the last msg
    client_application->register_message_handler(
            vsomeip::ANY_SERVICE, INSTANCE_ID, vsomeip::ANY_METHOD, [&](const std::shared_ptr<vsomeip::message>& _message) {
                bool is_unicast = _message->get_service() == SERVICE_ID_UNICAST && _message->get_method() == EVENT_ID_UNICAST;
                bool is_multicast = _message->get_service() == SERVICE_ID_MULTICAST && _message->get_method() == EVENT_ID_MULTICAST;

                if (!is_unicast && !is_multicast) {
                    VSOMEIP_WARNING << "Unexpected message " << std::setfill('0') << std::setw(4) << std::hex << _message->get_service()
                                    << "." << std::setw(4) << _message->get_instance() << "." << std::setw(4) << _message->get_method();
                    return;
                }

                std::lock_guard lock(sync);

                auto payload = _message->get_payload();
                int current_payload = vsomeip_v3::bithelper::read_uint16_le(payload->get_data());

                // Count the unicast messages received, if both channels are ready.
                if (is_unicast) {
                    unicast_payload = current_payload;
                    if (current_payload >= counting_payload) {
                        unicast_count += 1;
                    }
                }

                // Count the multicast messages received, if both channels are ready.
                if (is_multicast) {
                    multicast_payload = current_payload;
                    if (current_payload >= counting_payload) {
                        multicast_count += 1;
                    }
                }

                // Counting must begin when both the unicast channel and the multicast channel are ready to receive messages.
                if (unicast_payload != 0 && multicast_payload != 0 && counting_payload == std::numeric_limits<int>::max()) {
                    counting_payload = current_payload + 1;
                }

                // The test performs specific validations on multicast messages.
                if (!is_multicast) {
                    return;
                }

                // Specific validation for multicast messages
                if (current_payload >= 500) {
                    VSOMEIP_INFO << "Received last msg, ending test";
                    received_last_msg_prom.set_value(true);
                } else if ((last_payload == 0) || ((last_payload + 1) == current_payload)) {
                    VSOMEIP_DEBUG << "Received payload i=" << current_payload;
                    last_payload = current_payload;
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

    {
        // The test continues to run; we must pause it during validation.
        std::lock_guard lock(sync);

        EXPECT_NE(unicast_count, 0) << "Failed to receive unicast notification!";
        EXPECT_NE(multicast_count, 0) << "Failed to receive multicast notification!";

        // When comparing the number of messages received via unicast
        // and multicast, we accept a margin due to packets dropped via UDP
        static constexpr int UDP_MARGIN = 1;

        // We don't know when the test was interrupted, this could be in the middle
        // of a transmission, and only one or two messages were received by unicast
        // or multicast. This effect must be corrected.
        int correction = unicast_payload > multicast_payload ? -1 : (unicast_payload < multicast_payload ? 1 : 0);
        VSOMEIP_INFO << "unicast=" << unicast_count << "(" << unicast_payload << "), multicast=" << multicast_count << "("
                     << multicast_payload << "), correction=" << correction << ", counting_payload=" << counting_payload;
        EXPECT_LE(unicast_count + correction, multicast_count + UDP_MARGIN);
        EXPECT_GE(unicast_count + correction, multicast_count - UDP_MARGIN);
    }

    // Unsubscribe to the service and stop the vsomeip application
    client_application->unsubscribe(SERVICE_ID_MULTICAST, INSTANCE_ID, EVENTGROUP_ID, EVENT_ID_MULTICAST);
    client_application->unsubscribe(SERVICE_ID_UNICAST, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION);
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
