// Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <cstring>
#include <future>

#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <vsomeip/vsomeip.hpp>

#include "../../implementation/utility/include/byteorder.hpp"
#include "../../implementation/message/include/deserializer.hpp"
#include "../../implementation/service_discovery/include/service_discovery.hpp"
#include "../../implementation/service_discovery/include/message_impl.hpp"
#include "../../implementation/service_discovery/include/constants.hpp"
#include "../../implementation/service_discovery/include/enumeration_types.hpp"
#include "../../implementation/service_discovery/include/eventgroupentry_impl.hpp"
#include "../../implementation/message/include/message_impl.hpp"
#include "pending_subscription_test_globals.hpp"

static char* remote_address;
static char* local_address;

class pending_subscription : public ::testing::Test {
public:
    pending_subscription() :
        work_(std::make_shared<boost::asio::io_service::work>(io_)),
        io_thread_(std::bind(&pending_subscription::io_run, this)) {}
protected:

    void TearDown() {
        work_.reset();
        io_thread_.join();
        io_.stop();
    }

    void io_run() {
        io_.run();
    }

    boost::asio::io_service io_;
    std::shared_ptr<boost::asio::io_service::work> work_;
    std::thread io_thread_;
};

TEST_F(pending_subscription, send_multiple_subscriptions)
{
    std::promise<bool> trigger_notifications;

    boost::asio::ip::udp::socket udp_socket(io_,
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 30490));
    udp_socket.set_option(boost::asio::socket_base::reuse_address(true));

    std::thread receive_thread([&](){
        std::atomic<bool> keep_receiving(true);
        std::function<void()> receive;
        std::vector<std::uint8_t> receive_buffer(4096);
        std::vector<vsomeip::event_t> its_received_events;

        const std::function<void(const boost::system::error_code&, std::size_t)> receive_cbk = [&](
                const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                keep_receiving = false;
                ADD_FAILURE() << __func__ << " error: " << error.message();
                return;
            }
            #if 0
            std::stringstream str;
            for (size_t i = 0; i < bytes_transferred; i++) {
                str << std::hex << std::setw(2) << std::setfill('0') << std::uint32_t(receive_buffer[i]) << " ";
            }
            std::cout << __func__ << " received: " << std::dec << bytes_transferred << " bytes: " << str.str() << std::endl;
            #endif

            vsomeip::deserializer its_deserializer(&receive_buffer[0], bytes_transferred, 0);
            vsomeip::service_t its_service = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_SERVICE_POS_MIN],
                                                                   receive_buffer[VSOMEIP_SERVICE_POS_MAX]);
            vsomeip::method_t its_method = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_METHOD_POS_MIN],
                                                                 receive_buffer[VSOMEIP_METHOD_POS_MAX]);
            if (its_service == vsomeip::sd::service && its_method == vsomeip::sd::method) {
                vsomeip::sd::message_impl sd_msg;
                EXPECT_TRUE(sd_msg.deserialize(&its_deserializer));
                EXPECT_EQ(2u, sd_msg.get_entries().size());
                for (auto e : sd_msg.get_entries()) {
                    EXPECT_TRUE(e->is_eventgroup_entry());
                    EXPECT_EQ(vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK, e->get_type());
                    EXPECT_EQ(3u, e->get_ttl());
                    EXPECT_EQ(pending_subscription_test::service.service_id, e->get_service());
                    EXPECT_EQ(pending_subscription_test::service.instance_id, e->get_instance());
                    if (e->get_type() == vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK) {
                        std::shared_ptr<vsomeip::sd::eventgroupentry_impl> its_casted_entry =
                                std::static_pointer_cast<vsomeip::sd::eventgroupentry_impl>(e);
                        EXPECT_TRUE(its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id ||
                                    its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id+1);
                    }
                }
                EXPECT_EQ(0u, sd_msg.get_options().size());
            } else { // non-sd-message
                vsomeip::message_impl msg;
                EXPECT_TRUE(msg.deserialize(&its_deserializer));
                if (msg.get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
                    EXPECT_EQ(vsomeip::message_type_e::MT_RESPONSE, msg.get_message_type());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(pending_subscription_test::service.shutdown_method_id, msg.get_method());
                    EXPECT_EQ(0x2222, msg.get_client());
                } else if (msg.get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {
                    its_received_events.push_back(msg.get_method());
                    if (its_received_events.size() == 2) {
                        EXPECT_EQ(pending_subscription_test::service.event_id, its_received_events[0]);
                        EXPECT_EQ(static_cast<vsomeip::event_t>(pending_subscription_test::service.event_id + 1u), its_received_events[1]);
                    }
                    EXPECT_EQ(1u, msg.get_payload()->get_length());
                    EXPECT_EQ(0xDD, *msg.get_payload()->get_data());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(0x0, msg.get_client());
                }
            }

            static int called = 0;
            if (++called == 15) { // all subscribeAcks received
                trigger_notifications.set_value(true);
            }
            if (called == 18) { // events were received as well
                keep_receiving = false;
            }
            if (!error && keep_receiving) {
                receive();
            }
        };

        receive = [&]() {
            udp_socket.async_receive(boost::asio::buffer(receive_buffer, receive_buffer.capacity()),
                    receive_cbk);
        };

        receive();
        while(keep_receiving) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    std::thread send_thread([&]() {
        try {
            std::uint8_t its_subscribe_message[] = {
                0xff, 0xff, 0x81, 0x00,
                0x00, 0x00, 0x00, 0x40, // length
                0x00, 0x00, 0x00, 0x01,
                0x01, 0x01, 0x02, 0x00,
                0xc0, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x20, // length entries array
                0x06, 0x00, 0x00, 0x10,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x03,
                0x00, 0x00, 0x10, 0x00, // eventgroup
                0x06, 0x00, 0x00, 0x10,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x03,
                0x00, 0x00, 0x10, 0x01, // eventgroup 2
                0x00, 0x00, 0x00, 0x0c, // length options array
                0x00, 0x09, 0x04, 0x00,
                0xff, 0xff, 0xff, 0xff, // ip address
                0x00, 0x11, 0x77, 0x1a
            };
            boost::asio::ip::address its_local_address =
                    boost::asio::ip::address::from_string(std::string(local_address));
            std::memcpy(&its_subscribe_message[64], &its_local_address.to_v4().to_bytes()[0], 4);

            boost::asio::ip::udp::socket::endpoint_type target_sd(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30490);
            for (int var = 0; var < 15; ++var) {
                udp_socket.send_to(boost::asio::buffer(its_subscribe_message), target_sd);
                ++its_subscribe_message[11];
            }


            if (std::future_status::timeout == trigger_notifications.get_future().wait_for(std::chrono::seconds(10))) {
                ADD_FAILURE() << "Didn't receive all SubscribeAcks within time";
            } else {
                // call notify method
                std::uint8_t trigger_notifications_call[] = {
                    0x11, 0x22, 0x42, 0x42,
                    0x00, 0x00, 0x00, 0x08,
                    0x22, 0x22, 0x00, 0x01,
                    0x01, 0x00, 0x01, 0x00 };
                boost::asio::ip::udp::socket::endpoint_type target_service(
                        boost::asio::ip::address::from_string(std::string(remote_address)),
                        30001);
                udp_socket.send_to(boost::asio::buffer(trigger_notifications_call), target_service);
            }

            // call shutdown method
            std::uint8_t shutdown_call[] = {
                0x11, 0x22, 0x14, 0x04,
                0x00, 0x00, 0x00, 0x08,
                0x22, 0x22, 0x00, 0x01,
                0x01, 0x00, 0x00, 0x00 };
            boost::asio::ip::udp::socket::endpoint_type target_service(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30001);
            udp_socket.send_to(boost::asio::buffer(shutdown_call), target_service);
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

TEST_F(pending_subscription, send_alternating_subscribe_unsubscribe)
{
    std::promise<bool> trigger_notifications;

    boost::asio::ip::udp::socket udp_socket(io_,
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 30490));
    udp_socket.set_option(boost::asio::socket_base::reuse_address(true));

    std::thread receive_thread([&](){
        const std::uint32_t expected_acks(8);
        std::atomic<std::uint32_t> acks_received(0);

        const std::uint32_t expected_responses(1);
        std::atomic<std::uint32_t> responses_received(0);

        const std::uint32_t expected_notifications(2);
        std::atomic<std::uint32_t> notifications_received(0);

        bool triggered_notifications(false);

        std::function<void()> receive;
        std::vector<std::uint8_t> receive_buffer(4096);
        std::vector<vsomeip::event_t> its_received_events;

        const std::function<void(const boost::system::error_code&, std::size_t)> receive_cbk = [&](
                const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                acks_received = expected_acks;
                responses_received = expected_responses;
                ADD_FAILURE() << __func__ << " error: " << error.message();
                return;
            }
            #if 0
            std::stringstream str;
            for (size_t i = 0; i < bytes_transferred; i++) {
                str << std::hex << std::setw(2) << std::setfill('0') << std::uint32_t(receive_buffer[i]) << " ";
            }
            std::cout << __func__ << " received: " << std::dec << bytes_transferred << " bytes: " << str.str() << std::endl;
            #endif

            vsomeip::deserializer its_deserializer(&receive_buffer[0], bytes_transferred, 0);
            vsomeip::service_t its_service = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_SERVICE_POS_MIN],
                                                                   receive_buffer[VSOMEIP_SERVICE_POS_MAX]);
            vsomeip::method_t its_method = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_METHOD_POS_MIN],
                                                                 receive_buffer[VSOMEIP_METHOD_POS_MAX]);
            if (its_service == vsomeip::sd::service && its_method == vsomeip::sd::method) {
                vsomeip::sd::message_impl sd_msg;
                EXPECT_TRUE(sd_msg.deserialize(&its_deserializer));
                EXPECT_EQ(2u, sd_msg.get_entries().size());
                for (auto e : sd_msg.get_entries()) {
                    EXPECT_TRUE(e->is_eventgroup_entry());
                    EXPECT_EQ(vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK, e->get_type());
                    EXPECT_EQ(16u, e->get_ttl());
                    EXPECT_EQ(pending_subscription_test::service.service_id, e->get_service());
                    EXPECT_EQ(pending_subscription_test::service.instance_id, e->get_instance());
                    if (e->get_type() == vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK) {
                        std::shared_ptr<vsomeip::sd::eventgroupentry_impl> its_casted_entry =
                                std::static_pointer_cast<vsomeip::sd::eventgroupentry_impl>(e);
                        EXPECT_TRUE(its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id ||
                                    its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id+1);
                    }
                }
                EXPECT_EQ(0u, sd_msg.get_options().size());
                acks_received++;
            } else { // non-sd-message
                vsomeip::message_impl msg;
                EXPECT_TRUE(msg.deserialize(&its_deserializer));
                if (msg.get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
                    EXPECT_EQ(vsomeip::message_type_e::MT_RESPONSE, msg.get_message_type());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(pending_subscription_test::service.shutdown_method_id, msg.get_method());
                    EXPECT_EQ(0x2222, msg.get_client());
                    responses_received++;
                } else if (msg.get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {
                    its_received_events.push_back(msg.get_method());
                    if (its_received_events.size() == 2) {
                        EXPECT_EQ(pending_subscription_test::service.event_id, its_received_events[0]);
                        EXPECT_EQ(static_cast<vsomeip::event_t>(pending_subscription_test::service.event_id + 1u), its_received_events[1]);
                    }
                    EXPECT_EQ(1u, msg.get_payload()->get_length());
                    EXPECT_EQ(0xDD, *msg.get_payload()->get_data());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(0x0, msg.get_client());
                    notifications_received++;
                }
            }


            if (!triggered_notifications && acks_received == expected_acks) { // all subscribeAcks received
                trigger_notifications.set_value(true);
                triggered_notifications = true;
            }

            if (!error && (acks_received != expected_acks ||
                          responses_received != expected_responses ||
                          notifications_received != expected_notifications)) {
                receive();
            }
        };

        receive = [&]() {
            udp_socket.async_receive(boost::asio::buffer(receive_buffer, receive_buffer.capacity()),
                    receive_cbk);
        };

        receive();
        while(acks_received < expected_acks ||
                responses_received < expected_responses ||
                notifications_received < expected_notifications) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        EXPECT_EQ(expected_acks, acks_received);
        EXPECT_EQ(expected_responses, responses_received);
        EXPECT_EQ(expected_notifications, notifications_received);
    });

    std::thread send_thread([&]() {
        try {
            std::uint8_t its_subscribe_message[] = {
                0xff, 0xff, 0x81, 0x00,
                0x00, 0x00, 0x00, 0x40, // length
                0x00, 0x00, 0x00, 0x01,
                0x01, 0x01, 0x02, 0x00,
                0xc0, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x20, // length entries array
                0x06, 0x00, 0x00, 0x10,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x10, // 16 seconds TTL
                0x00, 0x00, 0x10, 0x00, // eventgroup
                0x06, 0x00, 0x00, 0x10,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x10, // 16 seconds TTL
                0x00, 0x00, 0x10, 0x01, // eventgroup 2
                0x00, 0x00, 0x00, 0x0c, // length options array
                0x00, 0x09, 0x04, 0x00,
                0xff, 0xff, 0xff, 0xff, // ip address
                0x00, 0x11, 0x77, 0x1a
            };

            boost::asio::ip::address its_local_address =
                    boost::asio::ip::address::from_string(std::string(local_address));
            std::memcpy(&its_subscribe_message[64], &its_local_address.to_v4().to_bytes()[0], 4);

            boost::asio::ip::udp::socket::endpoint_type target_sd(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30490);
            for (int var = 0; var < 15; ++var) {
                udp_socket.send_to(boost::asio::buffer(its_subscribe_message), target_sd);
                ++its_subscribe_message[11];
                if (its_subscribe_message[11] % 2) {
                    its_subscribe_message[35] = 16;
                    its_subscribe_message[51] = 16;
                } else {
                    its_subscribe_message[35] = 0;
                    its_subscribe_message[51] = 0;
                }
            }

            if (std::future_status::timeout == trigger_notifications.get_future().wait_for(std::chrono::seconds(10))) {
                ADD_FAILURE() << "Didn't receive all SubscribeAcks within time";
            } else {
                // call notify method
                std::uint8_t trigger_notifications_call[] = {
                    0x11, 0x22, 0x42, 0x42,
                    0x00, 0x00, 0x00, 0x08,
                    0x22, 0x22, 0x00, 0x01,
                    0x01, 0x00, 0x01, 0x00 };
                boost::asio::ip::udp::socket::endpoint_type target_service(
                        boost::asio::ip::address::from_string(std::string(remote_address)),
                        30001);
                udp_socket.send_to(boost::asio::buffer(trigger_notifications_call), target_service);
            }

            // call shutdown method
            std::uint8_t shutdown_call[] = {
                0x11, 0x22, 0x14, 0x04,
                0x00, 0x00, 0x00, 0x08,
                0x22, 0x22, 0x00, 0x01,
                0x01, 0x00, 0x00, 0x00 };
            boost::asio::ip::udp::socket::endpoint_type target_service(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30001);
            udp_socket.send_to(boost::asio::buffer(shutdown_call), target_service);
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

TEST_F(pending_subscription, send_multiple_unsubscriptions)
{
    std::promise<bool> trigger_notifications;

    boost::asio::ip::udp::socket udp_socket(io_,
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 30490));
    udp_socket.set_option(boost::asio::socket_base::reuse_address(true));

    std::thread receive_thread([&](){
        const std::uint32_t expected_acks(2);
        std::atomic<std::uint32_t> acks_received(0);

        const std::uint32_t expected_responses(1);
        std::atomic<std::uint32_t> responses_received(0);

        const std::uint32_t expected_notifications(2);
        std::atomic<std::uint32_t> notifications_received(0);

        bool triggered_notifications(false);

        std::function<void()> receive;
        std::vector<std::uint8_t> receive_buffer(4096);
        std::vector<vsomeip::event_t> its_received_events;

        const std::function<void(const boost::system::error_code&, std::size_t)> receive_cbk = [&](
                const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                acks_received = expected_acks;
                responses_received = expected_responses;
                ADD_FAILURE() << __func__ << " error: " << error.message();
                return;
            }
            #if 0
            std::stringstream str;
            for (size_t i = 0; i < bytes_transferred; i++) {
                str << std::hex << std::setw(2) << std::setfill('0') << std::uint32_t(receive_buffer[i]) << " ";
            }
            std::cout << __func__ << " received: " << std::dec << bytes_transferred << " bytes: " << str.str() << std::endl;
            #endif

            vsomeip::deserializer its_deserializer(&receive_buffer[0], bytes_transferred, 0);
            vsomeip::service_t its_service = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_SERVICE_POS_MIN],
                                                                   receive_buffer[VSOMEIP_SERVICE_POS_MAX]);
            vsomeip::method_t its_method = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_METHOD_POS_MIN],
                                                                 receive_buffer[VSOMEIP_METHOD_POS_MAX]);
            if (its_service == vsomeip::sd::service && its_method == vsomeip::sd::method) {
                vsomeip::sd::message_impl sd_msg;
                EXPECT_TRUE(sd_msg.deserialize(&its_deserializer));
                EXPECT_EQ(2u, sd_msg.get_entries().size());
                for (auto e : sd_msg.get_entries()) {
                    EXPECT_TRUE(e->is_eventgroup_entry());
                    EXPECT_EQ(vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK, e->get_type());
                    EXPECT_EQ(16u, e->get_ttl());
                    EXPECT_EQ(pending_subscription_test::service.service_id, e->get_service());
                    EXPECT_EQ(pending_subscription_test::service.instance_id, e->get_instance());
                    if (e->get_type() == vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK) {
                        std::shared_ptr<vsomeip::sd::eventgroupentry_impl> its_casted_entry =
                                std::static_pointer_cast<vsomeip::sd::eventgroupentry_impl>(e);
                        EXPECT_TRUE(its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id ||
                                    its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id+1);
                    }
                }
                EXPECT_EQ(0u, sd_msg.get_options().size());
                acks_received++;
            } else { // non-sd-message
                vsomeip::message_impl msg;
                EXPECT_TRUE(msg.deserialize(&its_deserializer));
                if (msg.get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
                    EXPECT_EQ(vsomeip::message_type_e::MT_RESPONSE, msg.get_message_type());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(pending_subscription_test::service.shutdown_method_id, msg.get_method());
                    EXPECT_EQ(0x2222, msg.get_client());
                    responses_received++;
                } else if (msg.get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {
                    its_received_events.push_back(msg.get_method());
                    if (its_received_events.size() == 2) {
                        EXPECT_EQ(pending_subscription_test::service.event_id, its_received_events[0]);
                        EXPECT_EQ(static_cast<vsomeip::event_t>(pending_subscription_test::service.event_id + 1u), its_received_events[1]);
                    }
                    EXPECT_EQ(1u, msg.get_payload()->get_length());
                    EXPECT_EQ(0xDD, *msg.get_payload()->get_data());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(0x0, msg.get_client());
                    notifications_received++;
                }
            }


            if (!triggered_notifications && acks_received == expected_acks) { // all subscribeAcks received
                trigger_notifications.set_value(true);
                triggered_notifications = true;
            }

            if (!error && (acks_received != expected_acks ||
                          responses_received != expected_responses ||
                          notifications_received != expected_notifications)) {
                receive();
            }
        };

        receive = [&]() {
            udp_socket.async_receive(boost::asio::buffer(receive_buffer, receive_buffer.capacity()),
                    receive_cbk);
        };

        receive();
        while(acks_received < expected_acks ||
                responses_received < expected_responses ||
                notifications_received < expected_notifications) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        EXPECT_EQ(expected_acks, acks_received);
        EXPECT_EQ(expected_responses, responses_received);
        EXPECT_EQ(expected_notifications, notifications_received);
    });

    std::thread send_thread([&]() {
        try {
            std::uint8_t its_subscribe_message[] = {
                0xff, 0xff, 0x81, 0x00,
                0x00, 0x00, 0x00, 0x40, // length
                0x00, 0x00, 0x00, 0x01,
                0x01, 0x01, 0x02, 0x00,
                0xc0, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x20, // length entries array
                0x06, 0x00, 0x00, 0x10,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x10, // 16 seconds TTL
                0x00, 0x00, 0x10, 0x00, // eventgroup
                0x06, 0x00, 0x00, 0x10,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x10, // 16 seconds TTL
                0x00, 0x00, 0x10, 0x01, // eventgroup 2
                0x00, 0x00, 0x00, 0x0c, // length options array
                0x00, 0x09, 0x04, 0x00,
                0xff, 0xff, 0xff, 0xff, // ip address
                0x00, 0x11, 0x77, 0x1a
            };

            boost::asio::ip::address its_local_address =
                    boost::asio::ip::address::from_string(std::string(local_address));
            std::memcpy(&its_subscribe_message[64], &its_local_address.to_v4().to_bytes()[0], 4);

            boost::asio::ip::udp::socket::endpoint_type target_sd(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30490);
            for (int var = 0; var < 15; ++var) {
                if (its_subscribe_message[11] == 15 || its_subscribe_message[11] == 0x1) {
                    its_subscribe_message[35] = 16;
                    its_subscribe_message[51] = 16;
                } else {
                    its_subscribe_message[35] = 0;
                    its_subscribe_message[51] = 0;
                }
                udp_socket.send_to(boost::asio::buffer(its_subscribe_message), target_sd);
                ++its_subscribe_message[11];
            }

            if (std::future_status::timeout == trigger_notifications.get_future().wait_for(std::chrono::seconds(10))) {
                ADD_FAILURE() << "Didn't receive all SubscribeAcks within time";
            } else {
                // call notify method
                std::uint8_t trigger_notifications_call[] = {
                    0x11, 0x22, 0x42, 0x42,
                    0x00, 0x00, 0x00, 0x08,
                    0x22, 0x22, 0x00, 0x01,
                    0x01, 0x00, 0x01, 0x00 };
                boost::asio::ip::udp::socket::endpoint_type target_service(
                        boost::asio::ip::address::from_string(std::string(remote_address)),
                        30001);
                udp_socket.send_to(boost::asio::buffer(trigger_notifications_call), target_service);
            }

            // call shutdown method
            std::uint8_t shutdown_call[] = {
                0x11, 0x22, 0x14, 0x04,
                0x00, 0x00, 0x00, 0x08,
                0x22, 0x22, 0x00, 0x01,
                0x01, 0x00, 0x00, 0x00 };
            boost::asio::ip::udp::socket::endpoint_type target_service(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30001);
            udp_socket.send_to(boost::asio::buffer(shutdown_call), target_service);
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

TEST_F(pending_subscription, send_alternating_subscribe_nack_unsubscribe)
{
    std::promise<bool> trigger_notifications;

    boost::asio::ip::udp::socket udp_socket(io_,
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 30490));
    udp_socket.set_option(boost::asio::socket_base::reuse_address(true));

    std::thread receive_thread([&](){
        const std::uint32_t expected_acks(8);
        std::atomic<std::uint32_t> acks_received(0);

        const std::uint32_t expected_nacks(8);
        std::atomic<std::uint32_t> nacks_received(0);

        const std::uint32_t expected_responses(1);
        std::atomic<std::uint32_t> responses_received(0);

        const std::uint32_t expected_notifications(2);
        std::atomic<std::uint32_t> notifications_received(0);

        bool triggered_notifications(false);

        std::function<void()> receive;
        std::vector<std::uint8_t> receive_buffer(4096);
        std::vector<vsomeip::event_t> its_received_events;

        const std::function<void(const boost::system::error_code&, std::size_t)> receive_cbk = [&](
                const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                acks_received = expected_acks;
                responses_received = expected_responses;
                nacks_received = expected_nacks;
                ADD_FAILURE() << __func__ << " error: " << error.message();
                return;
            }
            #if 0
            std::stringstream str;
            for (size_t i = 0; i < bytes_transferred; i++) {
                str << std::hex << std::setw(2) << std::setfill('0') << std::uint32_t(receive_buffer[i]) << " ";
            }
            std::cout << __func__ << " received: " << std::dec << bytes_transferred << " bytes: " << str.str() << std::endl;
            #endif

            vsomeip::deserializer its_deserializer(&receive_buffer[0], bytes_transferred, 0);
            vsomeip::service_t its_service = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_SERVICE_POS_MIN],
                                                                   receive_buffer[VSOMEIP_SERVICE_POS_MAX]);
            vsomeip::method_t its_method = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_METHOD_POS_MIN],
                                                                 receive_buffer[VSOMEIP_METHOD_POS_MAX]);
            if (its_service == vsomeip::sd::service && its_method == vsomeip::sd::method) {
                vsomeip::sd::message_impl sd_msg;
                EXPECT_TRUE(sd_msg.deserialize(&its_deserializer));
                EXPECT_EQ(2u, sd_msg.get_entries().size());
                for (auto e : sd_msg.get_entries()) {
                    EXPECT_TRUE(e->is_eventgroup_entry());
                    EXPECT_EQ(vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK, e->get_type());
                    if (e->get_ttl()) {
                        EXPECT_EQ(16u, e->get_ttl());
                        acks_received++;
                    } else {
                        EXPECT_EQ(0u, e->get_ttl());
                        nacks_received++;
                    }
                    EXPECT_EQ(pending_subscription_test::service.service_id, e->get_service());
                    EXPECT_EQ(pending_subscription_test::service.instance_id, e->get_instance());
                    if (e->get_type() == vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK) {
                        std::shared_ptr<vsomeip::sd::eventgroupentry_impl> its_casted_entry =
                                std::static_pointer_cast<vsomeip::sd::eventgroupentry_impl>(e);
                        EXPECT_TRUE(its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id ||
                                    its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id+1);
                    }
                }
                EXPECT_EQ(0u, sd_msg.get_options().size());
            } else { // non-sd-message
                vsomeip::message_impl msg;
                EXPECT_TRUE(msg.deserialize(&its_deserializer));
                if (msg.get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
                    EXPECT_EQ(vsomeip::message_type_e::MT_RESPONSE, msg.get_message_type());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(pending_subscription_test::service.shutdown_method_id, msg.get_method());
                    EXPECT_EQ(0x2222, msg.get_client());
                    responses_received++;
                } else if (msg.get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {
                    its_received_events.push_back(msg.get_method());
                    if (its_received_events.size() == 2) {
                        EXPECT_EQ(pending_subscription_test::service.event_id, its_received_events[0]);
                        EXPECT_EQ(static_cast<vsomeip::event_t>(pending_subscription_test::service.event_id + 1u), its_received_events[1]);
                    }
                    EXPECT_EQ(1u, msg.get_payload()->get_length());
                    EXPECT_EQ(0xDD, *msg.get_payload()->get_data());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(0x0, msg.get_client());
                    notifications_received++;
                }
            }


            if (!triggered_notifications && acks_received == expected_acks &&
                    nacks_received == expected_nacks) { // all subscribeAcks received
                trigger_notifications.set_value(true);
                triggered_notifications = true;
            }

            if (!error && (acks_received != expected_acks ||
                          responses_received != expected_responses ||
                          notifications_received != expected_notifications ||
                          nacks_received != expected_nacks)) {
                receive();
            }
        };

        receive = [&]() {
            udp_socket.async_receive(boost::asio::buffer(receive_buffer, receive_buffer.capacity()),
                    receive_cbk);
        };

        receive();
        while(acks_received < expected_acks ||
                responses_received < expected_responses ||
                notifications_received < expected_notifications) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        EXPECT_EQ(expected_acks, acks_received);
        EXPECT_EQ(expected_responses, responses_received);
        EXPECT_EQ(expected_notifications, notifications_received);
    });

    std::thread send_thread([&]() {
        try {
            std::uint8_t its_subscribe_message[] = {
                0xff, 0xff, 0x81, 0x00,
                0x00, 0x00, 0x00, 0x40, // length
                0x00, 0x00, 0x00, 0x01,
                0x01, 0x01, 0x02, 0x00,
                0xc0, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x20, // length entries array
                0x06, 0x00, 0x00, 0x10,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x10, // 16 seconds TTL
                0x00, 0x00, 0x10, 0x00, // eventgroup
                0x06, 0x00, 0x00, 0x10,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x10, // 16 seconds TTL
                0x00, 0x00, 0x10, 0x01, // eventgroup 2
                0x00, 0x00, 0x00, 0x0c, // length options array
                0x00, 0x09, 0x04, 0x00,
                0xff, 0xff, 0xff, 0xff, // ip address
                0x00, 0x11, 0x77, 0x1a
            };

            boost::asio::ip::address its_local_address =
                    boost::asio::ip::address::from_string(std::string(local_address));
            std::memcpy(&its_subscribe_message[64], &its_local_address.to_v4().to_bytes()[0], 4);

            boost::asio::ip::udp::socket::endpoint_type target_sd(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30490);
            for (int var = 0; var < 15; ++var) {
                udp_socket.send_to(boost::asio::buffer(its_subscribe_message), target_sd);
                ++its_subscribe_message[11];
                if (its_subscribe_message[11] % 2) {
                    its_subscribe_message[35] = 16;
                    its_subscribe_message[51] = 16;
                } else {
                    its_subscribe_message[35] = 0;
                    its_subscribe_message[51] = 0;
                }
            }

            if (std::future_status::timeout == trigger_notifications.get_future().wait_for(std::chrono::seconds(10))) {
                ADD_FAILURE() << "Didn't receive all SubscribeAcks within time";
            } else {
                // call notify method
                std::uint8_t trigger_notifications_call[] = {
                    0x11, 0x22, 0x42, 0x42,
                    0x00, 0x00, 0x00, 0x08,
                    0x22, 0x22, 0x00, 0x01,
                    0x01, 0x00, 0x01, 0x00 };
                boost::asio::ip::udp::socket::endpoint_type target_service(
                        boost::asio::ip::address::from_string(std::string(remote_address)),
                        30001);
                udp_socket.send_to(boost::asio::buffer(trigger_notifications_call), target_service);
            }

            // call shutdown method
            std::uint8_t shutdown_call[] = {
                0x11, 0x22, 0x14, 0x04,
                0x00, 0x00, 0x00, 0x08,
                0x22, 0x22, 0x00, 0x01,
                0x01, 0x00, 0x00, 0x00 };
            boost::asio::ip::udp::socket::endpoint_type target_service(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30001);
            udp_socket.send_to(boost::asio::buffer(shutdown_call), target_service);
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

TEST_F(pending_subscription, send_alternating_subscribe_unsubscribe_same_port)
{
    std::promise<bool> trigger_notifications;
    std::promise<void> tcp_connected;
    boost::asio::ip::udp::socket udp_socket(io_,
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 30490));
    udp_socket.set_option(boost::asio::socket_base::reuse_address(true));
    boost::asio::ip::tcp::socket tcp_socket(io_,
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 30490));
    tcp_socket.set_option(boost::asio::socket_base::reuse_address(true));

    std::thread receive_thread([&](){
        const std::uint32_t expected_acks(8);
        std::atomic<std::uint32_t> acks_received(0);

        const std::uint32_t expected_responses(1);
        std::atomic<std::uint32_t> responses_received(0);

        const std::uint32_t expected_notifications(2);
        std::atomic<std::uint32_t> notifications_received(0);

        bool triggered_notifications(false);

        std::function<void()> receive;
        std::vector<std::uint8_t> receive_buffer(4096);
        std::vector<vsomeip::event_t> its_received_events;

        boost::system::error_code ec;
        tcp_socket.connect(boost::asio::ip::tcp::endpoint(
                boost::asio::ip::address::from_string(remote_address), 40001), ec);
        ASSERT_EQ(0, ec.value());
        tcp_connected.set_value();

        const std::function<void(const boost::system::error_code&, std::size_t)> receive_cbk = [&](
                const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                acks_received = expected_acks;
                responses_received = expected_responses;
                ADD_FAILURE() << __func__ << " error: " << error.message();
                return;
            }
            #if 0
            std::stringstream str;
            for (size_t i = 0; i < bytes_transferred; i++) {
                str << std::hex << std::setw(2) << std::setfill('0') << std::uint32_t(receive_buffer[i]) << " ";
            }
            std::cout << __func__ << " received: " << std::dec << bytes_transferred << " bytes: " << str.str() << std::endl;
            #endif

            vsomeip::deserializer its_deserializer(&receive_buffer[0], bytes_transferred, 0);
            vsomeip::service_t its_service = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_SERVICE_POS_MIN],
                                                                   receive_buffer[VSOMEIP_SERVICE_POS_MAX]);
            vsomeip::method_t its_method = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_METHOD_POS_MIN],
                                                                 receive_buffer[VSOMEIP_METHOD_POS_MAX]);
            if (its_service == vsomeip::sd::service && its_method == vsomeip::sd::method) {
                vsomeip::sd::message_impl sd_msg;
                EXPECT_TRUE(sd_msg.deserialize(&its_deserializer));
                EXPECT_EQ(2u, sd_msg.get_entries().size());
                for (auto e : sd_msg.get_entries()) {
                    EXPECT_TRUE(e->is_eventgroup_entry());
                    EXPECT_EQ(vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK, e->get_type());
                    EXPECT_EQ(16u, e->get_ttl());
                    EXPECT_EQ(pending_subscription_test::service.service_id, e->get_service());
                    EXPECT_EQ(pending_subscription_test::service.instance_id, e->get_instance());
                    if (e->get_type() == vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK) {
                        std::shared_ptr<vsomeip::sd::eventgroupentry_impl> its_casted_entry =
                                std::static_pointer_cast<vsomeip::sd::eventgroupentry_impl>(e);
                        EXPECT_TRUE(its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id ||
                                    its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id+1);
                    }
                }
                EXPECT_EQ(0u, sd_msg.get_options().size());
                acks_received++;
            } else { // non-sd-message
                vsomeip::message_impl msg;
                EXPECT_TRUE(msg.deserialize(&its_deserializer));
                if (msg.get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
                    EXPECT_EQ(vsomeip::message_type_e::MT_RESPONSE, msg.get_message_type());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(pending_subscription_test::service.shutdown_method_id, msg.get_method());
                    EXPECT_EQ(0x2222, msg.get_client());
                    responses_received++;
                } else if (msg.get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {
                    its_received_events.push_back(msg.get_method());
                    if (its_received_events.size() == 2) {
                        EXPECT_EQ(pending_subscription_test::service.event_id, its_received_events[0]);
                        EXPECT_EQ(static_cast<vsomeip::event_t>(pending_subscription_test::service.event_id + 1u), its_received_events[1]);
                    }
                    EXPECT_EQ(1u, msg.get_payload()->get_length());
                    EXPECT_EQ(0xDD, *msg.get_payload()->get_data());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(0x0, msg.get_client());
                    notifications_received++;
                }
            }


            if (!triggered_notifications && acks_received == expected_acks) { // all subscribeAcks received
                trigger_notifications.set_value(true);
                triggered_notifications = true;
            }

            if (!error && (acks_received != expected_acks ||
                          responses_received != expected_responses ||
                          notifications_received != expected_notifications)) {
                receive();
            }
        };

        receive = [&]() {
            udp_socket.async_receive(boost::asio::buffer(receive_buffer, receive_buffer.capacity()),
                    receive_cbk);
        };

        receive();
        while(acks_received < expected_acks ||
                responses_received < expected_responses ||
                notifications_received < expected_notifications) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        EXPECT_EQ(expected_acks, acks_received);
        EXPECT_EQ(expected_responses, responses_received);
        EXPECT_EQ(expected_notifications, notifications_received);
    });

    std::thread send_thread([&]() {
        if (std::future_status::timeout == tcp_connected.get_future().wait_for(std::chrono::seconds(10))) {
            ADD_FAILURE() << "Didn't establish tcp connection within time";
        }

        try {
            std::uint8_t its_subscribe_message[] = {
                0xff, 0xff, 0x81, 0x00,
                0x00, 0x00, 0x00, 0x4C, // length
                0x00, 0x00, 0x00, 0x01,
                0x01, 0x01, 0x02, 0x00,
                0xc0, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x20, // length entries array
                0x06, 0x00, 0x00, 0x20,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x10, // 16 seconds TTL
                0x00, 0x00, 0x10, 0x00, // eventgroup
                0x06, 0x00, 0x00, 0x20,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x10, // 16 seconds TTL
                0x00, 0x00, 0x10, 0x01, // eventgroup 2
                0x00, 0x00, 0x00, 0x18, // length options array
                0x00, 0x09, 0x04, 0x00,
                0xff, 0xff, 0xff, 0xff, // ip address
                0x00, 0x11, 0x77, 0x1a,
                0x00, 0x09, 0x04, 0x00,
                0xff, 0xff, 0xff, 0xff, // ip address
                0x00, 0x06, 0x77, 0x1a
            };

            boost::asio::ip::address its_local_address =
                    boost::asio::ip::address::from_string(std::string(local_address));
            std::memcpy(&its_subscribe_message[64], &its_local_address.to_v4().to_bytes()[0], 4);
            std::memcpy(&its_subscribe_message[76], &its_local_address.to_v4().to_bytes()[0], 4);

            boost::asio::ip::udp::socket::endpoint_type target_sd(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30490);
            for (int var = 0; var < 15; ++var) {
                udp_socket.send_to(boost::asio::buffer(its_subscribe_message), target_sd);
                ++its_subscribe_message[11];
                if (its_subscribe_message[11] % 2) {
                    its_subscribe_message[35] = 16;
                    its_subscribe_message[51] = 16;
                } else {
                    its_subscribe_message[35] = 0;
                    its_subscribe_message[51] = 0;
                }
            }

            if (std::future_status::timeout == trigger_notifications.get_future().wait_for(std::chrono::seconds(10))) {
                ADD_FAILURE() << "Didn't receive all SubscribeAcks within time";
            } else {
                // call notify method
                std::uint8_t trigger_notifications_call[] = {
                    0x11, 0x22, 0x42, 0x42,
                    0x00, 0x00, 0x00, 0x08,
                    0x22, 0x22, 0x00, 0x01,
                    0x01, 0x00, 0x01, 0x00 };
                boost::asio::ip::udp::socket::endpoint_type target_service(
                        boost::asio::ip::address::from_string(std::string(remote_address)),
                        30001);
                udp_socket.send_to(boost::asio::buffer(trigger_notifications_call), target_service);
            }

            // call shutdown method
            std::uint8_t shutdown_call[] = {
                0x11, 0x22, 0x14, 0x04,
                0x00, 0x00, 0x00, 0x08,
                0x22, 0x22, 0x00, 0x01,
                0x01, 0x00, 0x00, 0x00 };
            boost::asio::ip::udp::socket::endpoint_type target_service(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30001);
            udp_socket.send_to(boost::asio::buffer(shutdown_call), target_service);
        } catch (...) {
            ASSERT_FALSE(true);
        }

    });

    send_thread.join();
    receive_thread.join();
    boost::system::error_code ec;
    tcp_socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
    udp_socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
    tcp_socket.close(ec);
    udp_socket.close(ec);
}

/*
 * @test Send a subscription as single message and afterwards send a
 * resubscription containing a new subscription in the same message and check
 * to receive initial event
 */
TEST_F(pending_subscription, subscribe_resubscribe_mixed)
{
    std::promise<void> first_initial_event_received;
    std::promise<void> second_initial_event_received;

    boost::asio::ip::udp::socket udp_socket(io_,
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 30490));
    udp_socket.set_option(boost::asio::socket_base::reuse_address(true));

    std::thread receive_thread([&](){
        std::atomic<bool> keep_receiving(true);
        std::function<void()> receive;
        std::vector<std::uint8_t> receive_buffer(4096);
        std::vector<vsomeip::event_t> its_received_events;

        const std::function<void(const boost::system::error_code&, std::size_t)> receive_cbk = [&](
                const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                keep_receiving = false;
                ADD_FAILURE() << __func__ << " error: " << error.message();
                return;
            }
            #if 0
            std::stringstream str;
            for (size_t i = 0; i < bytes_transferred; i++) {
                str << std::hex << std::setw(2) << std::setfill('0') << std::uint32_t(receive_buffer[i]) << " ";
            }
            std::cout << __func__ << " received: " << std::dec << bytes_transferred << " bytes: " << str.str() << std::endl;
            #endif

            vsomeip::deserializer its_deserializer(&receive_buffer[0], bytes_transferred, 0);
            vsomeip::service_t its_service = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_SERVICE_POS_MIN],
                                                                   receive_buffer[VSOMEIP_SERVICE_POS_MAX]);
            vsomeip::method_t its_method = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_METHOD_POS_MIN],
                                                                 receive_buffer[VSOMEIP_METHOD_POS_MAX]);
            if (its_service == vsomeip::sd::service && its_method == vsomeip::sd::method) {
                vsomeip::sd::message_impl sd_msg;
                EXPECT_TRUE(sd_msg.deserialize(&its_deserializer));
                EXPECT_GE(2u, sd_msg.get_entries().size());
                for (auto e : sd_msg.get_entries()) {
                    EXPECT_TRUE(e->is_eventgroup_entry());
                    EXPECT_EQ(vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK, e->get_type());
                    EXPECT_EQ(3u, e->get_ttl());
                    EXPECT_EQ(pending_subscription_test::service.service_id, e->get_service());
                    EXPECT_EQ(pending_subscription_test::service.instance_id, e->get_instance());
                    if (e->get_type() == vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK) {
                        std::shared_ptr<vsomeip::sd::eventgroupentry_impl> its_casted_entry =
                                std::static_pointer_cast<vsomeip::sd::eventgroupentry_impl>(e);
                        EXPECT_TRUE(its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id ||
                                    its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id+1);
                    }
                }
                EXPECT_EQ(0u, sd_msg.get_options().size());
            } else { // non-sd-message
                vsomeip::message_impl msg;
                EXPECT_TRUE(msg.deserialize(&its_deserializer));
                if (msg.get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
                    EXPECT_EQ(vsomeip::message_type_e::MT_RESPONSE, msg.get_message_type());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(pending_subscription_test::service.shutdown_method_id, msg.get_method());
                    EXPECT_EQ(0x2222, msg.get_client());
                } else if (msg.get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {
                    its_received_events.push_back(msg.get_method());
                    if (its_received_events.size() == 2) {
                        EXPECT_EQ(static_cast<vsomeip::event_t>(pending_subscription_test::service.event_id + 1u), its_received_events[0]);
                        EXPECT_EQ(pending_subscription_test::service.event_id, its_received_events[1]);
                    }
                    EXPECT_EQ(1u, msg.get_payload()->get_length());
                    EXPECT_EQ(0xDD, *msg.get_payload()->get_data());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(0x0, msg.get_client());
                }
            }

            static int called = 0;
            if (++called == 2) {
                EXPECT_EQ(1u, its_received_events.size());
                EXPECT_EQ(static_cast<vsomeip::event_t>(pending_subscription_test::service.event_id + 1u), its_received_events[0]);
                // all subscribeAcks and one initial event of first event received
                first_initial_event_received.set_value();
            }
            if (called == 4) { // events were received as well
                // all subscribeAcks and one initial event of second event received
                EXPECT_EQ(2u, its_received_events.size());
                EXPECT_EQ(static_cast<vsomeip::event_t>(pending_subscription_test::service.event_id + 1u), its_received_events[0]);
                EXPECT_EQ(pending_subscription_test::service.event_id, its_received_events[1]);
                keep_receiving = false;
                second_initial_event_received.set_value();
            }
            if (!error && keep_receiving) {
                receive();
            }
        };

        receive = [&]() {
            udp_socket.async_receive(boost::asio::buffer(receive_buffer, receive_buffer.capacity()),
                    receive_cbk);
        };

        receive();
        while(keep_receiving) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    std::thread send_thread([&]() {
        try {
            // call notify method to ensure to receive initial events
            std::uint8_t trigger_notifications_call[] = {
                0x11, 0x22, 0x42, 0x42,
                0x00, 0x00, 0x00, 0x08,
                0x22, 0x22, 0x00, 0x01,
                0x01, 0x00, 0x01, 0x00 };
            boost::asio::ip::udp::socket::endpoint_type target_service(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30001);
            udp_socket.send_to(boost::asio::buffer(trigger_notifications_call), target_service);

            std::uint8_t its_subscribe_message[] = {
                0xff, 0xff, 0x81, 0x00,
                0x00, 0x00, 0x00, 0x30, // length
                0x00, 0x00, 0x00, 0x01,
                0x01, 0x01, 0x02, 0x00,
                0xc0, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x10, // length entries array
                0x06, 0x00, 0x00, 0x10,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x03,
                0x00, 0x00, 0x10, 0x01, // eventgroup
                0x00, 0x00, 0x00, 0x0c, // length options array
                0x00, 0x09, 0x04, 0x00,
                0xff, 0xff, 0xff, 0xff, // ip address
                0x00, 0x11, 0x77, 0x1a
            };
            boost::asio::ip::address its_local_address =
                    boost::asio::ip::address::from_string(std::string(local_address));
            std::memcpy(&its_subscribe_message[48], &its_local_address.to_v4().to_bytes()[0], 4);

            boost::asio::ip::udp::socket::endpoint_type target_sd(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30490);

            udp_socket.send_to(boost::asio::buffer(its_subscribe_message), target_sd);



            if (std::future_status::timeout == first_initial_event_received.get_future().wait_for(std::chrono::seconds(10))) {
                ADD_FAILURE() << "Didn't receive all SubscribeAck of first subscription within time";
            }

            // send second subscription with resubscription and new subscription
            std::uint8_t its_subscribe_resubscribe_message[] = {
                0xff, 0xff, 0x81, 0x00,
                0x00, 0x00, 0x00, 0x40, // length
                0x00, 0x00, 0x00, 0x02,
                0x01, 0x01, 0x02, 0x00,
                0xc0, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x20, // length entries array
                0x06, 0x00, 0x00, 0x10,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x03,
                0x00, 0x00, 0x10, 0x00, // eventgroup
                0x06, 0x00, 0x00, 0x10,
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x03,
                0x00, 0x00, 0x10, 0x01, // eventgroup 2
                0x00, 0x00, 0x00, 0x0c, // length options array
                0x00, 0x09, 0x04, 0x00,
                0xff, 0xff, 0xff, 0xff, // ip address
                0x00, 0x11, 0x77, 0x1a
            };
            std::memcpy(&its_subscribe_resubscribe_message[64], &its_local_address.to_v4().to_bytes()[0], 4);
            udp_socket.send_to(boost::asio::buffer(its_subscribe_resubscribe_message), target_sd);

            if (std::future_status::timeout == second_initial_event_received.get_future().wait_for(std::chrono::seconds(10))) {
                ADD_FAILURE() << "Didn't receive all SubscribeAck of second subscription within time";
            }
            // call shutdown method
            std::uint8_t shutdown_call[] = {
                0x11, 0x22, 0x14, 0x04,
                0x00, 0x00, 0x00, 0x08,
                0x22, 0x22, 0x00, 0x01,
                0x01, 0x00, 0x00, 0x00 };
            udp_socket.send_to(boost::asio::buffer(shutdown_call), target_service);
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

/*
 * @test Send a SD message containing a Subscription followed by a StopSubscribe
 * Subscribe entry to the same service. Check to receive an initial event
 */
TEST_F(pending_subscription, send_subscribe_stop_subscribe_subscribe)
{
    std::promise<bool> trigger_notifications;
    std::promise<void> tcp_connected;
    boost::asio::ip::udp::socket udp_socket(io_,
            boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 30490));
    udp_socket.set_option(boost::asio::socket_base::reuse_address(true));
    boost::asio::ip::tcp::socket tcp_socket(io_,
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 30490));
    tcp_socket.set_option(boost::asio::socket_base::reuse_address(true));

    std::thread receive_thread([&](){
        const std::uint32_t expected_acks(1);
        std::atomic<std::uint32_t> acks_received(0);

        const std::uint32_t expected_responses(1);
        std::atomic<std::uint32_t> responses_received(0);

        const std::uint32_t expected_notifications(1);
        std::atomic<std::uint32_t> notifications_received(0);

        bool triggered_notifications(false);

        std::function<void()> receive;
        std::vector<std::uint8_t> receive_buffer(4096);
        std::vector<vsomeip::event_t> its_received_events;

        boost::system::error_code ec;
        tcp_socket.connect(boost::asio::ip::tcp::endpoint(
                boost::asio::ip::address::from_string(remote_address), 40001), ec);
        ASSERT_EQ(0, ec.value());
        tcp_connected.set_value();

        const std::function<void(const boost::system::error_code&, std::size_t)> receive_cbk = [&](
                const boost::system::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                acks_received = expected_acks;
                responses_received = expected_responses;
                ADD_FAILURE() << __func__ << " error: " << error.message();
                return;
            }
            #if 0
            std::stringstream str;
            for (size_t i = 0; i < bytes_transferred; i++) {
                str << std::hex << std::setw(2) << std::setfill('0') << std::uint32_t(receive_buffer[i]) << " ";
            }
            std::cout << __func__ << " received: " << std::dec << bytes_transferred << " bytes: " << str.str() << std::endl;
            #endif

            vsomeip::deserializer its_deserializer(&receive_buffer[0], bytes_transferred, 0);
            vsomeip::service_t its_service = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_SERVICE_POS_MIN],
                                                                   receive_buffer[VSOMEIP_SERVICE_POS_MAX]);
            vsomeip::method_t its_method = VSOMEIP_BYTES_TO_WORD(receive_buffer[VSOMEIP_METHOD_POS_MIN],
                                                                 receive_buffer[VSOMEIP_METHOD_POS_MAX]);
            if (its_service == vsomeip::sd::service && its_method == vsomeip::sd::method) {
                vsomeip::sd::message_impl sd_msg;
                EXPECT_TRUE(sd_msg.deserialize(&its_deserializer));
                EXPECT_EQ(1u, sd_msg.get_entries().size());
                for (auto e : sd_msg.get_entries()) {
                    EXPECT_TRUE(e->is_eventgroup_entry());
                    EXPECT_EQ(vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK, e->get_type());
                    EXPECT_EQ(16u, e->get_ttl());
                    EXPECT_EQ(pending_subscription_test::service.service_id, e->get_service());
                    EXPECT_EQ(pending_subscription_test::service.instance_id, e->get_instance());
                    if (e->get_type() == vsomeip::sd::entry_type_e::SUBSCRIBE_EVENTGROUP_ACK) {
                        std::shared_ptr<vsomeip::sd::eventgroupentry_impl> its_casted_entry =
                                std::static_pointer_cast<vsomeip::sd::eventgroupentry_impl>(e);
                        EXPECT_TRUE(its_casted_entry->get_eventgroup() == pending_subscription_test::service.eventgroup_id);
                    }
                }
                EXPECT_EQ(0u, sd_msg.get_options().size());
                acks_received++;
            } else { // non-sd-message
                vsomeip::message_impl msg;
                EXPECT_TRUE(msg.deserialize(&its_deserializer));
                if (msg.get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
                    EXPECT_EQ(vsomeip::message_type_e::MT_RESPONSE, msg.get_message_type());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(pending_subscription_test::service.shutdown_method_id, msg.get_method());
                    EXPECT_EQ(0x2222, msg.get_client());
                    responses_received++;
                } else if (msg.get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {
                    its_received_events.push_back(msg.get_method());
                    EXPECT_EQ(1u, its_received_events.size());
                    EXPECT_EQ(1u, msg.get_payload()->get_length());
                    EXPECT_EQ(0xDD, *msg.get_payload()->get_data());
                    EXPECT_EQ(pending_subscription_test::service.service_id, msg.get_service());
                    EXPECT_EQ(0x0, msg.get_client());
                    notifications_received++;
                }
            }


            if (!triggered_notifications && acks_received == expected_acks) { // all subscribeAcks received
                trigger_notifications.set_value(true);
                triggered_notifications = true;
            }

            if (!error && (acks_received != expected_acks ||
                          responses_received != expected_responses ||
                          notifications_received != expected_notifications)) {
                receive();
            }
        };

        receive = [&]() {
            udp_socket.async_receive(boost::asio::buffer(receive_buffer, receive_buffer.capacity()),
                    receive_cbk);
        };

        receive();
        while(acks_received < expected_acks ||
                responses_received < expected_responses ||
                notifications_received < expected_notifications) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        EXPECT_EQ(expected_acks, acks_received);
        EXPECT_EQ(expected_responses, responses_received);
        EXPECT_EQ(expected_notifications, notifications_received);
    });

    std::thread send_thread([&]() {
        if (std::future_status::timeout == tcp_connected.get_future().wait_for(std::chrono::seconds(10))) {
            ADD_FAILURE() << "Didn't establish tcp connection within time";
        }

        try {
            std::uint8_t its_subscribe_message[] = {
                0xff, 0xff, 0x81, 0x00,
                0x00, 0x00, 0x00, 0x50, // length
                0x00, 0x00, 0x00, 0x01,
                0x01, 0x01, 0x02, 0x00,
                0xc0, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x30, // length entries array
                0x06, 0x00, 0x00, 0x10, // subscribe Eventgroup entry
                0x11, 0x22, 0x00, 0x01, // service / instance
                0x00, 0x00, 0x00, 0x10, // 16 seconds TTL
                0x00, 0x00, 0x10, 0x00, // eventgroup
                0x06, 0x00, 0x00, 0x10, // Stop subscribe Eventgroup entry
                0x11, 0x22, 0x00, 0x01,
                0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x10, 0x00,
                0x06, 0x00, 0x00, 0x10, // subscribe Eventgroup entry
                0x11, 0x22, 0x00, 0x01,
                0x00, 0x00, 0x00, 0x10,
                0x00, 0x00, 0x10, 0x00,
                0x00, 0x00, 0x00, 0x0c, // length options array
                0x00, 0x09, 0x04, 0x00,
                0xff, 0xff, 0xff, 0xff, // ip address
                0x00, 0x11, 0x77, 0x1a
            };

            boost::asio::ip::address its_local_address =
                    boost::asio::ip::address::from_string(std::string(local_address));
            std::memcpy(&its_subscribe_message[80], &its_local_address.to_v4().to_bytes()[0], 4);

            boost::asio::ip::udp::socket::endpoint_type target_sd(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30490);
            udp_socket.send_to(boost::asio::buffer(its_subscribe_message), target_sd);

            if (std::future_status::timeout == trigger_notifications.get_future().wait_for(std::chrono::seconds(10))) {
                ADD_FAILURE() << "Didn't receive all SubscribeAcks within time";
            } else {
                // call notify method
                std::uint8_t trigger_notifications_call[] = {
                    0x11, 0x22, 0x42, 0x42,
                    0x00, 0x00, 0x00, 0x08,
                    0x22, 0x22, 0x00, 0x01,
                    0x01, 0x00, 0x01, 0x00 };
                boost::asio::ip::udp::socket::endpoint_type target_service(
                        boost::asio::ip::address::from_string(std::string(remote_address)),
                        30001);
                udp_socket.send_to(boost::asio::buffer(trigger_notifications_call), target_service);
            }

            // call shutdown method
            std::uint8_t shutdown_call[] = {
                0x11, 0x22, 0x14, 0x04,
                0x00, 0x00, 0x00, 0x08,
                0x22, 0x22, 0x00, 0x01,
                0x01, 0x00, 0x00, 0x00 };
            boost::asio::ip::udp::socket::endpoint_type target_service(
                    boost::asio::ip::address::from_string(std::string(remote_address)),
                    30001);
            udp_socket.send_to(boost::asio::buffer(shutdown_call), target_service);
        } catch (...) {
            ASSERT_FALSE(true);
        }

    });
    send_thread.join();
    receive_thread.join();
    boost::system::error_code ec;
    tcp_socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
    udp_socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
    tcp_socket.close(ec);
    udp_socket.close(ec);
}

#ifndef _WIN32
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    if(argc < 4) {
        std::cerr << "Please pass an target and local IP address and test mode to this binary like: "
                << argv[0] << " 10.0.3.1 10.0.3.202 SUBSCRIBE" << std::endl;
        std::cerr << "Testmodes are [SUBSCRIBE, SUBSCRIBE_UNSUBSCRIBE, UNSUBSCRIBE]" << std::endl;
        exit(1);
    }
    remote_address = argv[1];
    local_address = argv[2];
    std::string its_testmode = argv[3];
    if (its_testmode == std::string("SUBSCRIBE")) {
        ::testing::GTEST_FLAG(filter) = "*send_multiple_subscriptions";
    } else if (its_testmode == std::string("SUBSCRIBE_UNSUBSCRIBE")) {
        ::testing::GTEST_FLAG(filter) = "*send_alternating_subscribe_unsubscribe";
    } else if (its_testmode == std::string("UNSUBSCRIBE")) {
        ::testing::GTEST_FLAG(filter) = "*send_multiple_unsubscriptions";
    } else if (its_testmode == std::string("SUBSCRIBE_UNSUBSCRIBE_NACK")) {
        ::testing::GTEST_FLAG(filter) = "*send_alternating_subscribe_nack_unsubscribe";
    } else if (its_testmode == std::string("SUBSCRIBE_UNSUBSCRIBE_SAME_PORT")) {
        ::testing::GTEST_FLAG(filter) = "*send_alternating_subscribe_unsubscribe_same_port";
    } else if (its_testmode == std::string("SUBSCRIBE_RESUBSCRIBE_MIXED")) {
        ::testing::GTEST_FLAG(filter) = "*subscribe_resubscribe_mixed";
    } else if (its_testmode == std::string("SUBSCRIBE_STOPSUBSCRIBE_SUBSCRIBE")) {
        ::testing::GTEST_FLAG(filter) = "*send_subscribe_stop_subscribe_subscribe";
    }
    return RUN_ALL_TESTS();
}
#endif
