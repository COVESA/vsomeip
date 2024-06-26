// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

#include <boost/asio.hpp>
#include <gtest/gtest.h>
#include <vsomeip/vsomeip.hpp>

#include <com/comconstants.hpp>
#include <someip/sd/options/ipv4endpointoption.hpp>
#include <someip/sd/someipsd.hpp>
#include <someip/sd/someipsdentry.hpp>
#include <someip/sd/someipsdoption.hpp>
#include <someip/someipmessage.hpp>

#include "../../implementation/service_discovery/include/constants.hpp"
#include "../../implementation/service_discovery/include/enumeration_types.hpp"

#include "unknown_sd_option_type_test_globals.hpp"

static char* remote_address;
static char* local_address;

using namespace unknown_sd_option_type_test;
using namespace vsomeip_utilities::someip;
using namespace vsomeip_utilities::someip::sd;

class unknown_sd_option_type_client : public ::testing::Test {
public:
    unknown_sd_option_type_client() :
        work_(std::make_shared<boost::asio::io_context::work>(io_)),
        io_thread_(std::bind(&unknown_sd_option_type_client::io_run, this)) {}

protected:
    void TearDown() {
        work_.reset();
        io_thread_.join();
        io_.stop();
    }

    void io_run() {
        io_.run();
    }

    boost::asio::io_context io_;
    std::shared_ptr<boost::asio::io_context::work> work_;
    std::thread io_thread_;
};

/*
 * @test Send a subscription to the service and check that the
 * subscription is answered with an SubscribeEventgroupAck entry by the service.
 * Check that the subscription is active at the end of the test and check that
 * the notifications sent by the service are received by the client
 */
TEST_F(unknown_sd_option_type_client, send_subscription) {
    std::promise<bool> trigger_notifications;

    // Create UDP socket.
    boost::asio::ip::udp::socket udp_socket(io_,
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), SD_PORT));
    udp_socket.set_option(boost::asio::socket_base::reuse_address(true));
    udp_socket.set_option(boost::asio::socket_base::linger(true, 0));

    // Create thread to receive messages.
    std::thread receive_thread([&]() {
        // Acknowledge counter.
        std::uint32_t subscribe_acks_received = 0;

        // Read control boolean.
        bool keep_receiving(true);

        while (keep_receiving) {
            // Creating a vector to receive SOMEIP Messages.
            std::vector<std::shared_ptr<SomeIpMessage>> someip_msgs;

            // Read SOMEIP messages.
            someip_msgs = SomeIpMessage::readSomeIpMessages(&udp_socket, vsomeip_utilities::com::DEFAULT_BUFFER_SIZE, keep_receiving);

            for (auto someip_msg : someip_msgs) {
                if (someip_msg->serviceId() == vsomeip::sd::service && someip_msg->methodId() == vsomeip::sd::method) {
                    SomeIpSd sd_msg = SomeIpSd::create();
                    EXPECT_TRUE(someip_msg->pull(sd_msg) > 0);
                    for (auto entry : sd_msg.entries().get()) {
                        EXPECT_EQ(entry.type(), SUBSCRIBE_EVENTGROUP_ACK);
                        EXPECT_EQ(3u, entry.ttl());
                        EXPECT_EQ(entry.serviceId(), SD_SERVICE_ID);
                        EXPECT_EQ(entry.instanceId(), SD_INSTANCE_ID);
                        if (entry.type() == SUBSCRIBE_EVENTGROUP_ACK) {
                            EXPECT_TRUE(entry.eventgroupId() == EVENTGROUP_ID);
                            subscribe_acks_received++;
                            std::cout << "RECEIVED ACK\n";
                        }
                    }
                    EXPECT_EQ(0u, sd_msg.options().length());
                }
            }

            if (subscribe_acks_received) { // all subscribeAcks received
                trigger_notifications.set_value(true);
                keep_receiving = false;
            }
        }
    });

    // Create thread to send messages.
    std::thread send_thread([&]() {
        try {
            // Create SOMEIP core of subscription message.
            SomeIpMessage someip_subscription_msg = SomeIpMessage::create()
                                      .serviceId(SD_DEFAULT_SERVICE_ID)
                                      .methodId(SD_DEFAULT_METHOD_ID)
                                      .clientId(SD_DEFAULT_CLIENT_ID)
                                      .sessionId(SD_SESSION_ID)
                                      .protocolVersion(SOMEIP_VERSION)
                                      .interfaceVersion(SOMEIP_SD_INTERFACE_VERSION)
                                      .messageType(types::messageType_e::E_MT_NOTIFICATION)
                                      .returnCode(types::returnCode_e::E_RC_OK);

            // Create SOMEIP-SD header of subscription message.
            SomeIpSd sd_header = SomeIpSd::create().reboot(true).unicast(true).controlFlag(false);

            // Create Eventgroup subscription entry.
            SomeIpSdEntry sd_entry =  SomeIpSdEntry::create()
                                    .type(std::uint8_t(vsomeip_v3::sd::entry_type_e::SUBSCRIBE_EVENTGROUP))
                                    .index1st(1)
                                    .optsCount(0x01)
                                    .optsCount1st(0x01)
                                    .optsCount2nd(0x00)
                                    .serviceId(SD_SERVICE_ID)
                                    .instanceId(SD_INSTANCE_ID)
                                    .majorVersion(SD_MAJOR_VERSION)
                                    .ttl(SD_TTL)
                                    .eventgroupCounter(SD_COUNTER)
                                    .eventgroupId(EVENTGROUP_ID);

            // Add entry to SOMEIP-SD header.
            sd_header.entries().pushEntry(sd_entry);

            // Create IPV4 endpoint.
            options::Ipv4EndpointOption sd_endpoint_option =
                options::Ipv4EndpointOption::create()
                        .ipv4Address(local_address)
                        .portNumber(SD_PORT)
                        .protocol(UDP_PROTOCOL);

            // Create a working and an unknown sd option based on ipv4 endpoint.
            SomeIpSdOption sd_option = SomeIpSdOption::create().type(0x04).push(sd_endpoint_option);
            SomeIpSdOption sd_unknown_option = SomeIpSdOption::create().type(0xFF).push(sd_endpoint_option);

            // Add those options to the sd header.
            sd_header.options().pushEntry(sd_unknown_option);
            sd_header.options().pushEntry(sd_option);

            // Add sd header to the core SOMEIP message.
            someip_subscription_msg.push(sd_header);

            // Create udp socket for SOME/IP-SD message.
            boost::asio::ip::udp::socket::endpoint_type target_sd(
                    boost::asio::ip::address::from_string(remote_address),
                    SD_PORT);

            // Create the endpoint to be used to send SOME/IP-SD eventgroup subscription.
            SomeIpMessage::sendSomeIpMessage(&udp_socket, target_sd, someip_subscription_msg);

            if (std::future_status::timeout == trigger_notifications.get_future().wait_for(std::chrono::seconds(10))) {
                ADD_FAILURE() << "Didn't receive SubscribeAck within time";
            }

            // Create shutdown method call message.
            SomeIpMessage shutdown_message = SomeIpMessage::create()
                                      .serviceId(SD_SERVICE_ID)
                                      .methodId(SHUTDOWN_METHOD_ID)
                                      .clientId(SD_CLIENT_ID)
                                      .protocolVersion(SOMEIP_VERSION)
                                      .interfaceVersion(SOMEIP_INTERFACE_VERSION)
                                      .messageType(types::messageType_e::E_MT_REQUEST)
                                      .returnCode(types::returnCode_e::E_RC_OK);


            // Create the endpoint to be used to send SOME/IP shutdown method call.
            boost::asio::ip::udp::socket::endpoint_type target_service(
                    boost::asio::ip::address::from_string(remote_address),
                    SERVER_PORT);

            // Send shutdown method call.
            SomeIpMessage::sendSomeIpMessage(&udp_socket, target_service, shutdown_message);
        } catch (...) {
            ASSERT_FALSE(true);
        }

    });

    send_thread.join();
    receive_thread.join();
    boost::system::error_code ec;
    udp_socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
    udp_socket.close(ec);
}

#if defined(__linux__)
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (argc < 3) {
        std::cerr << "Please pass a target and local IP address and test mode to this binary like: "
                << argv[0] << " 10.0.3.1 10.0.3.202" << std::endl;
        exit(1);
    }
    remote_address = argv[1];
    local_address = argv[2];

    return RUN_ALL_TESTS();
}
#endif
