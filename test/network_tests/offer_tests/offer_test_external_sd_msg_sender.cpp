// Copyright (C) 2015-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <exception>
#include <iostream>

#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <thread>

#include <vsomeip/vsomeip.hpp>

#include "../../implementation/utility/include/bithelper.hpp"
#include "../../implementation/message/include/deserializer.hpp"
#include "../../implementation/service_discovery/include/message_impl.hpp"
#include "../../implementation/service_discovery/include/constants.hpp"
#include "../../implementation/service_discovery/include/enumeration_types.hpp"
#include "../../implementation/service_discovery/include/eventgroupentry_impl.hpp"

static char* passed_address;

TEST(someip_offer_test, send_offer_service_sd_message) {
    try {
        boost::asio::io_context io_;

        boost::system::error_code ec;
        boost::asio::ip::udp::socket udp_socket(io_);
        udp_socket.open(boost::asio::ip::udp::v4(), ec);
        ASSERT_FALSE(ec) << "Failed to open UDP socket: " << ec.message();

        udp_socket.set_option(boost::asio::socket_base::reuse_address(true));
        udp_socket.set_option(boost::asio::socket_base::linger(true, 0));

        udp_socket.bind(boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 30490));

        udp_socket.set_option(boost::asio::ip::multicast::join_group(boost::asio::ip::make_address_v4("224.0.23.1")));

        std::vector<std::uint8_t> receive_buffer(4096);

        for (bool keep_receiving = true; keep_receiving;) {
            std::size_t bytes_transferred = udp_socket.receive(boost::asio::buffer(receive_buffer, receive_buffer.capacity()), 0, ec);
            ASSERT_FALSE(ec) << __func__ << " error: " << ec.message();

            vsomeip::deserializer its_deserializer(&receive_buffer[0], bytes_transferred, 0);
            vsomeip::service_t its_service = vsomeip::bithelper::read_uint16_be(&receive_buffer[VSOMEIP_SERVICE_POS_MIN]);
            vsomeip::method_t its_method = vsomeip::bithelper::read_uint16_be(&receive_buffer[VSOMEIP_METHOD_POS_MIN]);

            if (its_service == vsomeip::sd::service && its_method == vsomeip::sd::method) {
                vsomeip::sd::message_impl sd_msg;
                EXPECT_TRUE(sd_msg.deserialize(&its_deserializer));
                EXPECT_EQ(1u, sd_msg.get_entries().size());

                for (const auto& e : sd_msg.get_entries()) {
                    if (e->get_type() == vsomeip::sd::entry_type_e::OFFER_SERVICE) {
                        udp_socket.set_option(boost::asio::ip::multicast::leave_group(boost::asio::ip::make_address_v4("224.0.23.1")));
                        keep_receiving = false;
                        std::cout << "[SD_MSG_SENDER] Received OFFER_SERVICE message - the test can start sending." << std::endl;
                    }
                }
            }
        }

        std::uint8_t its_offer_service_message[] = {0xff, 0xff, 0x81, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x01, 0x01,
                                                    0x01, 0x02, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x01, 0x00,
                                                    0x00, 0x20, 0x11, 0x11, 0x00, 0x01, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00,
                                                    0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x09, 0x04, 0x00, 0x0a, 0x00, 0x03, 0x01,
                                                    0x00, 0x06, 0x86, 0xcf, 0x00, 0x09, 0x04, 0x00, 0x0a, 0x00, 0x03,
                                                    0x7D, // slave address
                                                    0x00, 0x11, 0x75, 0x31};

        boost::asio::ip::udp::socket::endpoint_type target_sd(boost::asio::ip::make_address(std::string(passed_address)), 30490);
        // send messages forever, caller will signal to stop
        while (true) {
            udp_socket.send_to(boost::asio::buffer(its_offer_service_message), target_sd);
            ++its_offer_service_message[11];
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        FAIL() << "Caught exception: " << e.what();
    }
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if (argc < 2) {
        std::cout << "Please pass an target IP address to this binary like: " << argv[0] << " 10.0.3.1" << std::endl;
        exit(1);
    }
    passed_address = argv[1];
    return RUN_ALL_TESTS();
}
#endif
