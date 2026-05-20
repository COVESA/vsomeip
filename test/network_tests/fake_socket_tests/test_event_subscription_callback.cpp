// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/ecu_setup.hpp"
#include "helpers/command_gate.hpp"
#include "helpers/command_record.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/sockets/fake_tcp_socket_handle.hpp"
#include "helpers/service_state.hpp"
#include "helpers/availability_checker.hpp"
#include "sample_configurations.hpp"

#include "../../../implementation/utility/include/utility.hpp"

#include "sample_interfaces.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdlib>

namespace vsomeip_v3::testing {

static std::string const server_name_{"server"};
static std::string const client_name_{"client"};

struct base_event_subscription_callback : public base_fake_socket_fixture {
    interface interface_{0x1000,
                         {},
                         {interface::event_spec{0x8001, 0x8001, vsomeip::reliability_type_e::RT_UNRELIABLE},
                          interface::event_spec{0x8002, 0x8002, vsomeip::reliability_type_e::RT_UNRELIABLE},
                          interface::event_spec{0x8003, 0x8003, vsomeip::reliability_type_e::RT_UNRELIABLE}}};
    ecu_setup provider_ecu_{"provider", ecu_config{boardnet::ecu_one_config}.add_interface({interface_}), *socket_manager_};

    event_ids field_one_ = interface_.fields_[0];
    event_ids field_two_ = interface_.fields_[1];
    event_ids field_three_ = interface_.fields_[2];

    app* server_;
    app* client_;

    std::vector<unsigned char> field_one_payload_{0x2};
    std::vector<unsigned char> field_two_payload_{0x4};
    message_checker const field_one_checker_{std::nullopt, interface_.instance_, field_one_.event_id_,
                                             vsomeip::message_type_e::MT_NOTIFICATION, field_one_payload_};
    message_checker const field_two_checker_{std::nullopt, interface_.instance_, field_two_.event_id_,
                                             vsomeip::message_type_e::MT_NOTIFICATION, field_two_payload_};
};

struct test_event_subscription_callback_local : public base_event_subscription_callback {
    void start_apps() {
        provider_ecu_.add_app(client_name_);
        provider_ecu_.add_app(server_name_);
        provider_ecu_.prepare();

        ASSERT_TRUE(setup_data_pipe(client_name_, server_name_, socket_role::server, c2s_gate_->get_data_pipe()));
        ASSERT_TRUE(setup_data_pipe(client_name_, server_name_, socket_role::client, s2c_gate_->get_data_pipe()));
        ASSERT_TRUE(setup_data_pipe(client_name_, provider_ecu_.router_name_, socket_role::client, r2c_gate_->get_data_pipe()));

        provider_ecu_.start_apps();

        server_ = provider_ecu_.apps_[server_name_];
        client_ = provider_ecu_.apps_[client_name_];
    }

    std::shared_ptr<command_gate> c2s_gate_ = command_gate::create();
    std::shared_ptr<command_gate> s2c_gate_ = command_gate::create();
    std::shared_ptr<command_gate> r2c_gate_ = command_gate::create();
};

TEST_F(test_event_subscription_callback_local, subscriptions_do_no_leak_across_stop_start) {
    /**
     * Ensure that a subscriber is not kept across stop/start, by checking what the client receives.
     * 1. Start to offer two fields
     * 2. Client subscribes to field_one
     * 3. Server stops
     * 4. Client unsubscribes from field_one
     * 5. Restart server
     * 6. Client subscribes to field_two
     * 7. Await sub ack for field_two
     * 8. Server sets both fields to a value
     * 9. field_two is received by the client
     * 10. field_one is not received (subscription was not renewed after step 4)
     **/
    start_apps();
    server_->offer(interface_);

    client_->request_service(interface_.instance_);
    client_->subscribe_field(field_one_);
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_one_)));

    server_->stop();
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(interface_.instance_)));
    client_->unsubscribe_from_group(field_one_);

    server_->app_state_record_.clear();
    ASSERT_TRUE(server_->start());
    ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    client_->subscribe_field(field_two_);
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_two_)));
    server_->send_event(field_one_, field_one_payload_);
    server_->send_event(field_two_, field_two_payload_);

    EXPECT_TRUE(client_->message_record_.wait_for(field_two_checker_)) << client_->message_record_;
    // if received, it should have been received before the one checked above
    EXPECT_FALSE(client_->message_record_.wait_for(field_one_checker_, std::chrono::milliseconds(100)));
}

TEST_F(test_event_subscription_callback_local, subscriptions_callbacks_do_no_leak_across_stop_start) {
    /**
     * Ensure that pending subscription callbacks queued before a stop/start cycle
     * are not dispatched in the new server lifecycle.
     *
     * 1. Block the client-to-server pipe before any SUBSCRIBE can reach the server
     * 2. Client subscribes to field_one, field_two, field_three (all queued behind the gate)
     * 3. Offer the service and register a subscription handler for field_one that will:
     *    a. wait for a request message (proving the dispatcher is running)
     *    b. stop the server — pending subscription callbacks for field_two/three are now stale
     *    c. unsubscribe from field_one and field_two
     *    d. restart the server and re-register handlers for all three fields (counting subscriptions)
     *    e. re-offer the service
     *    f. send a final request from the client (proves the new lifecycle is live)
     * 4. Send a first request from the client (consumed by step 3a)
     * 5. Unblock the gate — the dispatcher executes step 3
     * 6. Await the final request on the server (proves step 3f completed)
     * 7. Await successful subscription to field_three (only field still subscribed)
     * 8. Assert subscription_count == 2 (one from step 3a, one from field_three in the new lifecycle)
     *    — field_one and field_two callbacks from the old lifecycle must not have fired
     **/
    start_apps();

    c2s_gate_->block_at(vsomeip_v3::protocol::id_e::SUBSCRIBE_ID);
    client_->request_service(interface_.instance_);
    client_->subscribe_field(field_one_);
    client_->subscribe_field(field_two_);
    client_->subscribe_field(field_three_);

    std::atomic<int> subscription_count = 0;

    std::vector<unsigned char> first_payload = {0x1, 0x1, 0x1};
    std::vector<unsigned char> final_payload = {0x2, 0x2, 0x2};
    method_t specific_method = 0x55;
    message_checker request_checker{std::nullopt, interface_.instance_, specific_method, vsomeip::message_type_e::MT_REQUEST,
                                    first_payload};

    server_->offer(interface_);
    // as soon as we unblock the gate the following code will be executed, note that this is only executed for field_one_ subscriptions!
    server_->register_group_subscription_handler(
            field_one_, [&, this, request_checker](client_t, uid_t, gid_t, const std::string&, bool _is_subscribed) {
                VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription for field_one_: " << _is_subscribed;
                if (_is_subscribed) {
                    ++subscription_count;
                    if (!server_->message_record_.wait_for(request_checker)) {
                        VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR server did not receive the request in time";
                        std::abort();
                    }
                    VSOMEIP_INFO << "[DISPATCHER_TAG] Stopping the server";
                    // at this point all handlers should be queued
                    server_->stop();
                    // at this point everything within the server should have been stopped (except this in-flight dispatcher task)

                    // unsubscribe to ensure that no new subscription is dispatched in the new server lifecycle
                    VSOMEIP_INFO << "[DISPATCHER_TAG] Unsubscribing from two out of three fields";
                    client_->unsubscribe_from_group(field_one_);
                    client_->unsubscribe_from_group(field_two_);
                    client_->availability_record_.clear();

                    if (!server_->start()) {
                        VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR server could not be restarted";
                        std::abort();
                    }
                    if (!server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED)) {
                        VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR server could not be restarted";
                        std::abort();
                    }
                    // handlers had been deregistered, ensure before offering anything that we would count any new subscription
                    server_->register_group_subscription_handler(
                            field_one_, [&, this](client_t, uid_t, gid_t, const std::string&, bool _is_subscribed) {
                                VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription for field_one_ in new lc: "
                                             << _is_subscribed;
                                if (_is_subscribed) {
                                    ++subscription_count;
                                }
                                return true;
                            });
                    server_->register_group_subscription_handler(
                            field_two_, [&, this](client_t, uid_t, gid_t, const std::string&, bool _is_subscribed) {
                                VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription for field_two_ in new lc: "
                                             << _is_subscribed;
                                if (_is_subscribed) {
                                    ++subscription_count;
                                }
                                return true;
                            });
                    server_->register_group_subscription_handler(
                            field_three_, [&, this](client_t, uid_t, gid_t, const std::string&, bool _is_subscribed) {
                                VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription for field_three_ in new lc: "
                                             << _is_subscribed;
                                if (_is_subscribed) {
                                    ++subscription_count;
                                }
                                return true;
                            });

                    // now re-offer and unblock the dispatching for this client (subscription wise)
                    server_->offer(interface_);

                    if (!client_->availability_record_.wait_for_last(service_availability::available(interface_.instance_))) {
                        VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR unavailable";
                        std::abort();
                    }
                    request request{.service_instance_ = interface_.instance_,
                                    .method_ = specific_method,
                                    .message_type_ = vsomeip::message_type_e::MT_REQUEST,
                                    .reliability_ = true,
                                    .payload_ = final_payload};
                    client_->send_request(request);
                }
                VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed << " returning";
                return true;
            });

    ASSERT_TRUE(c2s_gate_->wait_for_blocked());
    // because the pipe is blocked -> service is available -> request should be queued immediately
    request request{.service_instance_ = interface_.instance_,
                    .method_ = specific_method,
                    .message_type_ = vsomeip::message_type_e::MT_REQUEST,
                    .reliability_ = true,
                    .payload_ = first_payload};
    client_->send_request(request);

    c2s_gate_->block(false);
    // Now the test flow will continue in the callback of the subscription_handler

    // the request_checker had been copied into the handler, we can change the expected payload here without any implication for the
    // handler
    auto final_checker = request_checker;
    final_checker.payload_ = final_payload;
    EXPECT_TRUE(server_->message_record_.wait_for(final_checker, std::chrono::seconds(10)));
    EXPECT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_three_)));
    auto const invocation_count = subscription_count.load();
    EXPECT_EQ(2, invocation_count);
}

TEST_F(test_event_subscription_callback_local, a_broken_connection_to_the_router_does_not_lead_to_a_local_subscription_leak) {
    /**
     * Ensure that a subscriber is not kept across re-register of the service app, by checking what the client receives.
     * 1. Start to offer two fields
     * 2. Client subscribes to field_one
     * 3. Ensure server cannot re-register
     * 4. Break server to router connection
     * 5. Client sees the service as unavailable
     * 6. Client unsubscribes from field_one (not received by the service)
     * 7. Client subscribes to field_two
     * 8. Let the server re-register itself
     * 9. Subscription to field_two is reported successful
     * 10. Both fields are sent out
     * 11. Only field_two is received
     **/
    start_apps();
    server_->offer(interface_);

    client_->request_service(interface_.instance_);
    client_->subscribe_field(field_one_);
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_one_)));

    set_ignore_connections(provider_ecu_.router_name_, true);
    ASSERT_TRUE(disconnect(server_name_, boost::asio::error::connection_reset, provider_ecu_.router_name_, boost::asio::error::timed_out));
    ASSERT_TRUE(client_->availability_record_.wait_for_last(service_availability::unavailable(interface_.instance_)));

    client_->unsubscribe_from_group(field_one_);
    client_->subscribe_field(field_two_);
    set_ignore_connections(provider_ecu_.router_name_, false);

    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_two_)));
    server_->send_event(field_one_, field_one_payload_);
    server_->send_event(field_two_, field_two_payload_);

    EXPECT_TRUE(client_->message_record_.wait_for(field_two_checker_)) << client_->message_record_;
    // if received, it should have been received before the one checked above
    EXPECT_FALSE(client_->message_record_.wait_for(field_one_checker_, std::chrono::milliseconds(100)));
}

TEST_F(test_event_subscription_callback_local, check_dispatcher_ordering_when_subscription_callback_is_stuck) {
    /**
     * Ensure handler ordering when multiple dispatcher threads are running.
     *
     * 1. Offer service with a field
     * 2. Install a subscription handler for this field that will:
     *    if (sub):
     *      a. block until a request is received
     *      b. record the subscription
     *    if (unsub):
     *      c. record the unsub
     * 3. Install a message handler for a request that will:
     *      d. record the request
     * 4. Queue a SUBSCRIBE_ID, UNSUBSCRIBE_ID, REQUEST on the socket client -> server
     * 5. Ensure that the record shows: REQUEST, SUB, UNSUB
     *
     * (While the subscription is blocking the execution of the unsubscription,
     * a request can be executed)
     *
     **/
    start_apps();
    server_->offer(interface_);

    enum test_step { sub, unsub, message }; // enum is streamable by default
    auto reception = std::make_shared<attribute_recorder<test_step>>();
    server_->register_group_subscription_handler(field_one_, [reception](client_t, uid_t, gid_t, const std::string&, bool _is_subscribed) {
        VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed;
        if (_is_subscribed) {
            VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed << " blocking";
            if (!reception->wait_for_last(test_step::message)) {
                VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR server did not receive the request in time";
                std::abort();
            }
            reception->record(test_step::sub);
        } else {
            reception->record(test_step::unsub);
        }
        VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed << " returning";
        return true;
    });
    method_t specific_method = 0x55;
    server_->get_application()->register_message_handler(interface_.instance_.service_, interface_.instance_.instance_, specific_method,
                                                         [reception](auto) {
                                                             VSOMEIP_INFO << "[DISPATCHER_TAG] server received a message";
                                                             reception->record(test_step::message);
                                                         });

    c2s_gate_->block_at(vsomeip_v3::protocol::id_e::SUBSCRIBE_ID);

    client_->request_service(interface_.instance_);
    client_->subscribe_field(field_one_);
    ASSERT_TRUE(c2s_gate_->wait_for_blocked());
    client_->unsubscribe_from_group(field_one_);

    request request{.service_instance_ = interface_.instance_,
                    .method_ = specific_method,
                    .message_type_ = vsomeip::message_type_e::MT_REQUEST,
                    .reliability_ = true,
                    .payload_{}};
    client_->send_request(request);

    c2s_gate_->block(false);

    auto const expected = std::vector<test_step>{test_step::message, test_step::sub, test_step::unsub};
    ASSERT_TRUE(reception->wait_for([expected](auto const& record) { return record == expected; })) << *reception;
}

TEST_F(test_event_subscription_callback_local, a_broken_c2s_connection_does_not_lead_to_a_subscription_leak) {
    /**
     * Ensure that when a client to server connection error handler is executed while a dispatcher task
     * is in flight, that the corresponding continuation does not lead to an invalid subscription:
     *
     * 1. Offer two fields (different eventgroups)
     * 2. Subscribe to field_one
     * 3. Within the dispatcher "subscription_handler":
     *    a. ensure that the client does not receive routing info to the server
     *    b. break the connection
     *    c. ensure error handler deleted routing info to service
     *    d. unsubscribe from field_one - the server does not receive this due to "a."
     *    e. subscribe to field_two
     * 4. Await successful subscription to field_two
     * 5. Send data for field_one
     * 6. Send data for field_two
     * 7. Await message for field_two
     * 8. Ensure no message for field_one is received
     *
     **/
    // 1.
    start_apps();
    server_->offer(interface_);

    // To be able to write first "2." and then the dispatcher logic
    c2s_gate_->block_at(vsomeip_v3::protocol::id_e::SUBSCRIBE_ID);

    // 2.
    client_->request_service(interface_.instance_);
    client_->subscribe_field(field_one_);

    // 3.
    server_->register_group_subscription_handler(field_one_, [&, this](client_t, uid_t, gid_t, const std::string&, bool _is_subscribed) {
        VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed;
        if (_is_subscribed) {
            VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed << " blocking";
            r2c_gate_->block_at(vsomeip_v3::protocol::id_e::ROUTING_INFO_ID);
            set_ignore_connections(server_name_, true);
            if (!disconnect(client_name_, boost::asio::error::connection_reset, server_name_, boost::asio::error::timed_out)) {
                VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR on disconnect";
                std::abort();
            }
            if (!client_->availability_record_.wait_for_last(service_availability::unavailable(interface_.instance_))) {
                VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR unavailable";
                std::abort();
            }
            VSOMEIP_INFO << "[DISPATCHER_TAG] unsubscribing";
            client_->unsubscribe_from_group(field_one_);
            client_->subscribe_field(field_two_);
            set_ignore_connections(server_name_, false);
            r2c_gate_->block(false);
        }
        VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed << " returning";
        return true;
    });

    ASSERT_TRUE(c2s_gate_->wait_for_blocked());
    c2s_gate_->block(false);
    // Now the test flow will continue in the callback of the subscription_handler
    // Dispatcher thread unblocked the execution of the next handler
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_two_)));
    server_->send_event(field_one_, field_one_payload_);
    server_->send_event(field_two_, field_two_payload_);

    // Which will be awaited here...but adjust the waiting time as multiple things are awaited on the dispatcher thread
    EXPECT_TRUE(client_->message_record_.wait_for(field_two_checker_, std::chrono::seconds(10))) << client_->message_record_;
    // if received, it should have been received before the one checked above
    EXPECT_FALSE(client_->message_record_.wait_for(field_one_checker_, std::chrono::milliseconds(100)));
}

TEST_F(test_event_subscription_callback_local, a_broken_s2r_connection_does_not_lead_to_a_subscription_leak_from_dispatcher_thread) {
    /**
     * Ensure that when the server to router connection breaks while a dispatcher task
     * is in flight, that the corresponding continuation does not lead to an invalid subscription:
     *
     * 1. Offer two fields (different eventgroups)
     * 2. Subscribe to field_one
     * 3. Within the dispatcher "subscription_handler":
     *    a. ensure that the server cannot connect back to the router
     *    b. break the connection
     *    c. ensure error handler deleted routing info to service (within the client)
     *    d. unsubscribe from field_one - the server does not receive this due to "a."
     *    e. subscribe to field_two
     *    f. allow the server to connect to the router (which can now distribute the routing info)
     * 4. Await successful subscription to field_two
     * 5. Send data for field_one
     * 6. Send data for field_two
     * 7. Ensure exactly one message is sent from server to client
     * 8. Ensure the message is field_two
     *
     **/

    start_apps();
    server_->offer(interface_);

    c2s_gate_->block_at(vsomeip_v3::protocol::id_e::SUBSCRIBE_ID);
    client_->request_service(interface_.instance_);
    client_->subscribe_field(field_one_);

    // as soon as we unblock the gate the following code will be executed, note that this is only executed for field_one_ subscriptions!
    server_->register_group_subscription_handler(field_one_, [&, this](client_t, uid_t, gid_t, const std::string&, bool _is_subscribed) {
        VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed;
        if (_is_subscribed) {
            VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed << " blocking";
            set_ignore_connections(provider_ecu_.router_name_, true);
            if (!disconnect(server_name_, boost::asio::error::connection_reset, provider_ecu_.router_name_,
                            boost::asio::error::timed_out)) {
                VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR on disconnect";
                std::abort();
            }
            if (!client_->availability_record_.wait_for_last(service_availability::unavailable(interface_.instance_))) {
                VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR unavailable";
                std::abort();
            }
            client_->unsubscribe_from_group(field_one_);
            client_->subscribe_field(field_two_);
            set_ignore_connections(provider_ecu_.router_name_, false);
        }
        VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed << " returning";
        return true;
    });

    // Ensure that one notify_id was put on the wire (later)
    s2c_gate_->block_at(vsomeip_v3::protocol::id_e::SEND_ID);

    ASSERT_TRUE(c2s_gate_->wait_for_blocked());
    c2s_gate_->block(false);
    // Now the test flow will continue in the callback of the subscription_handler
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_two_)));
    server_->send_event(field_one_, field_one_payload_);
    server_->send_event(field_two_, field_two_payload_);

    // Ensure that one notify_id was put on the wire (now)
    EXPECT_TRUE(s2c_gate_->wait_for_blocked());
    // but also ensure that there was no second notify put on the wire
    s2c_gate_->block_at(vsomeip_v3::protocol::id_e::SEND_ID, 2);
    EXPECT_FALSE(s2c_gate_->wait_for_blocked(std::chrono::milliseconds(100)));
    // the one send message should be the expected subscription
    EXPECT_TRUE(client_->message_record_.wait_for(field_two_checker_)) << client_->message_record_;
}

TEST_F(test_event_subscription_callback_local, an_intermediate_unregistration_of_handler_does_not_lead_to_a_subscription_leak) {
    /**
     * Ensure that unregistering a subscription handler from within the dispatcher callback
     * does not cause a subscription leak for a subsequent subscription:
     *
     * 1. Offer two fields (different eventgroups)
     * 2. Subscribe to field_one
     * 3. Within the dispatcher "subscription_handler":
     *    a. unregister the subscription handler for field_one
     *    b. unsubscribe from field_one
     *    c. subscribe to field_two
     * 4. Await successful subscription to field_two
     * 5. Send data for field_one
     * 6. Send data for field_two
     * 7. Ensure exactly one message is sent from server to client
     * 8. Ensure the message is field_two
     *
     **/
    start_apps();
    server_->offer(interface_);

    c2s_gate_->block_at(vsomeip_v3::protocol::id_e::SUBSCRIBE_ID);
    client_->request_service(interface_.instance_);
    client_->subscribe_field(field_one_);

    // as soon as we unblock the gate the following code will be executed, note that this is only executed for field_one_ subscriptions!
    server_->register_group_subscription_handler(field_one_, [&, this](client_t, uid_t, gid_t, const std::string&, bool _is_subscribed) {
        VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed;
        if (_is_subscribed) {
            // executed immediately
            server_->unregister_group_subscription_handler(field_one_);
            client_->unsubscribe_from_group(field_one_);
            client_->subscribe_field(field_two_);
        }
        VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed << " returning";
        return true;
    });

    // Ensure that one notify_id was put on the wire (later)
    s2c_gate_->block_at(vsomeip_v3::protocol::id_e::SEND_ID);

    ASSERT_TRUE(c2s_gate_->wait_for_blocked());
    c2s_gate_->block(false);
    // Now the test flow will continue in the callback of the subscription_handler
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_two_)));
    server_->send_event(field_one_, field_one_payload_);
    server_->send_event(field_two_, field_two_payload_);

    // Ensure that one notify_id was put on the wire (now)
    EXPECT_TRUE(s2c_gate_->wait_for_blocked());
    // but also ensure that there was no second notify put on the wire
    s2c_gate_->block_at(vsomeip_v3::protocol::id_e::SEND_ID, 2);
    EXPECT_FALSE(s2c_gate_->wait_for_blocked(std::chrono::milliseconds(100)));
    // the one send message should be the expected subscription
    EXPECT_TRUE(client_->message_record_.wait_for(field_two_checker_)) << client_->message_record_;
}

TEST_F(test_event_subscription_callback_local, a_stuck_subscription_does_not_block_io) {
    /**
     * Ensure that a misbehaving subscription handler does not block IO threads.
     * N dispatcher threads are blocked, but IO threads must remain unblocked.
     * It is assumed that there are 2 io-threads and N = 6.
     *
     * 1. Install N clients
     * 2. Offer service with a field
     * 3. install a subscription handler that:
     *    a. will block until N subscriptions are recorded (N threads are blocked)
     *    b. accept the subscription
     * 4. Let all clients subscribe
     * 5. Ensure all subscriptions are confirmed
     *
     **/

    std::vector<std::string> clients = [] {
        std::vector<std::string> str;
        for (int i = 0; i < 6; ++i) {
            str.push_back("client_number_" + std::to_string(i));
        }
        return str;
    }();
    for (auto const& client : clients) {
        provider_ecu_.add_app(client);
    }
    start_apps();
    server_->offer(interface_);

    std::atomic<int> counter{0};
    attribute_recorder<int> record;
    // as soon as we unblock the gate the following code will be executed, note that this is only executed for field_one_ subscriptions!
    server_->register_group_subscription_handler(
            field_one_, [&counter, &record, max = clients.size()](client_t, uid_t, gid_t, const std::string&, bool _is_subscribed) {
                int n = ++counter;
                if (_is_subscribed) {
                    VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription number : " << n;
                    record.record(n);
                    return record.wait_for_any(static_cast<int>(max), std::chrono::seconds(20));
                }
                return true;
            });
    for (auto const& client : clients) {

        auto* app = provider_ecu_.apps_[client];
        app->request_service(interface_.instance_);
        app->subscribe_field(field_one_);
    }

    for (auto const& client : clients) {
        auto* app = provider_ecu_.apps_[client];
        EXPECT_TRUE(app->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(field_one_),
                                                           std::chrono::seconds(20)));
    }
}
struct test_event_subscription_callback_boardnet : public base_event_subscription_callback {
    void start_apps() {
        consumer_ecu_.add_app(client_name_);
        provider_ecu_.add_app(server_name_);

        consumer_ecu_.prepare();
        provider_ecu_.prepare();

        ASSERT_TRUE(setup_data_pipe(server_name_, provider_ecu_.router_name_, socket_role::client, r2s_gate_->get_data_pipe()));
        ASSERT_TRUE(setup_data_pipe(server_name_, provider_ecu_.router_name_, socket_role::server, s2r_gate_->get_data_pipe()));

        consumer_ecu_.start_apps();
        provider_ecu_.start_apps();

        server_ = provider_ecu_.apps_[server_name_];
        client_ = consumer_ecu_.apps_[client_name_];
    }

    ecu_setup consumer_ecu_{"consumer", boardnet::ecu_three_config, *socket_manager_};

    std::shared_ptr<command_gate> r2s_gate_ = command_gate::create();
    std::shared_ptr<command_gate> s2r_gate_ = command_gate::create();
};

TEST_F(test_event_subscription_callback_boardnet, a_broken_connection_to_the_router_does_not_lead_to_a_subscription_leak) {
    /**
     * Boardnet version of the test of "a_broken_s2r_connection_does_not_lead_to_a_subscription_leak_from_dispatcher_thread",
     * as the routing connection has a special token handling
     **/
    GTEST_SKIP() << "Sometimes the subscription for the event group two are not forwarded via the boardnet";

    start_apps();
    server_->offer(interface_);

    r2s_gate_->block_at(vsomeip_v3::protocol::id_e::SUBSCRIBE_ID);

    client_->request_service(interface_.instance_);
    client_->subscribe_field(field_one_);

    // as soon as we unblock the gate the following code will be executed, note that this is only executed for field_one_ subscriptions!
    server_->register_group_subscription_handler(field_one_, [&, this](client_t, uid_t, gid_t, const std::string&, bool _is_subscribed) {
        VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed;
        if (_is_subscribed) {
            VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed << " blocking";
            r2s_gate_->block_at(vsomeip_v3::protocol::id_e::UNSUBSCRIBE_ID);
            client_->unsubscribe_from_group(field_one_);
            client_->subscribe_field(field_two_);
            if (!r2s_gate_->wait_for_blocked()) {
                VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR unsub was not received";
                std::abort();
            }

            set_ignore_connections(provider_ecu_.router_name_, true);
            if (!disconnect(server_name_, boost::asio::error::connection_reset, provider_ecu_.router_name_,
                            boost::asio::error::timed_out)) {
                VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR disconnect did not work";
                std::abort();
            }
            if (!wait_for_connection_drop(server_name_, provider_ecu_.router_name_)) {
                VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR on wait_for_connection_drop";
                std::abort();
            }
            set_ignore_connections(provider_ecu_.router_name_, false);
            if (!await_connection(server_name_, provider_ecu_.router_name_)) {
                VSOMEIP_INFO << "[DISPATCHER_TAG] ERROR on await_connection";
                std::abort();
            }
            // Now we can be certain that the connection has been renewed and the pipe does now not contain a unsub message
            r2s_gate_->block(false);
        }
        VSOMEIP_INFO << "[DISPATCHER_TAG] server received a subscription: " << _is_subscribed << " returning";
        return true;
    });

    // Ensure that one notify_id was put on the wire (later)
    s2r_gate_->block_at(vsomeip_v3::protocol::id_e::NOTIFY_ID);

    ASSERT_TRUE(r2s_gate_->wait_for_blocked());
    r2s_gate_->block(false);
    // Now the test flow will continue in the callback of the subscription_handler
    ASSERT_TRUE(client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(field_two_)));
    server_->send_event(field_one_, field_one_payload_);
    server_->send_event(field_two_, field_two_payload_);

    // Ensure that one notify_id was put on the wire (now)
    EXPECT_TRUE(s2r_gate_->wait_for_blocked());
    // but also ensure that there was no second notify put on the wire
    s2r_gate_->block_at(vsomeip_v3::protocol::id_e::NOTIFY_ID, 2);
    EXPECT_FALSE(s2r_gate_->wait_for_blocked(std::chrono::milliseconds(100)));
    // the one send message should be the expected subscription
    EXPECT_TRUE(client_->message_record_.wait_for(field_two_checker_)) << client_->message_record_;
}
}
