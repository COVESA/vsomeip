// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/app.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/command_record.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/service_state.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdlib>

namespace vsomeip_v3::testing {

// Generic handler to block specific ACKs by client_id
class ack_blocker {
public:
    std::map<uint16_t, std::shared_ptr<std::promise<void>>> client_releases_;
    std::set<uint16_t> blocked_clients_; // Track which clients are currently blocked
    std::vector<uint16_t> client_order_; // Order of clients to map ACKs to
    size_t ack_counter_{0};
    size_t subscribe_counter_{0};
    std::mutex mtx_;
    std::condition_variable cv_;
    std::condition_variable cv_subscribe_;

    // Register a promise to block a specific client
    void block_client(uint16_t client_id) {
        std::lock_guard<std::mutex> lock(mtx_);
        client_releases_[client_id] = std::make_shared<std::promise<void>>();
        client_order_.push_back(client_id);
    }

    // Release a specific client
    void release_client(uint16_t client_id) {
        std::unique_lock<std::mutex> lock(mtx_);
        auto it = client_releases_.find(client_id);
        if (it != client_releases_.end()) {
            it->second->set_value();
            client_releases_.erase(it); // Remove after releasing
        }
    }

    // Wait until a specific client is blocked
    bool wait_for_client_blocked(uint16_t client_id, std::chrono::milliseconds timeout = std::chrono::milliseconds(3000)) {
        std::unique_lock<std::mutex> lock(mtx_);
        return cv_.wait_for(lock, timeout, [this, client_id]() { return blocked_clients_.find(client_id) != blocked_clients_.end(); });
    }

    // Wait until N SUBSCRIBE messages have been received (without blocking them)
    bool wait_for_subscribe_count(size_t count, std::chrono::milliseconds timeout = std::chrono::milliseconds(3000)) {
        std::unique_lock<std::mutex> lock(mtx_);
        return cv_subscribe_.wait_for(lock, timeout, [this, count]() { return subscribe_counter_ >= count; });
    }

    // Handler that will be called for each message
    bool operator()(command_message const& _cmd) {
        if (_cmd.id_ != protocol::id_e::SUBSCRIBE_ACK_ID) {
            return false; // Does not block, continues processing
        }

        std::unique_lock<std::mutex> lock(mtx_);

        // Map ACK by order: 1st ACK -> client_order_[0], 2nd ACK -> client_order_[1], etc.
        size_t ack_index = ack_counter_++;
        uint16_t client_id = 0;
        if (ack_index < client_order_.size()) {
            client_id = client_order_[ack_index];
        }

        if (client_id == 0) {
            return false; // No mapping for this ACK
        }

        auto it = client_releases_.find(client_id);
        if (it == client_releases_.end()) {
            return false; // No promise for this client
        }

        auto promise_ptr = it->second;
        TEST_LOG << "Blocking SUBSCRIBE_ACK #" << (ack_index + 1) << " for client 0x" << std::hex << client_id;

        // Mark as blocked and notify waiters
        blocked_clients_.insert(client_id);
        cv_.notify_all();

        // Unlock before waiting on the promise
        lock.unlock();
        promise_ptr->get_future().wait();

        // Re-lock to unmark as blocked
        lock.lock();
        blocked_clients_.erase(client_id);

        TEST_LOG << "Released SUBSCRIBE_ACK #" << (ack_index + 1) << " for client 0x" << std::hex << client_id;

        return false; // Does not drop the message
    }
};

std::string const router_one_name_ = "router_one";
std::string const router_two_name_ = "router_two";
std::string const ecu_two_server_name_ = "ecu_two_server";
std::string const ecu_one_client_name_ = "ecu_one_client";
std::string const ecu_one_server_name_ = "ecu_one_server";
std::string const ecu_two_client_name_ = "ecu_two_client";

struct test_boardnet_helper : public base_fake_socket_fixture {
    test_boardnet_helper() { }
    ~test_boardnet_helper() {
        for (auto [name, vsip_app] : apps_) {
            stop_client(name);
        }
        for (auto env : env_vars_) {
            ::unsetenv(env.c_str());
        }
    }

    app* start_application(std::string _app_name, std::string _config) {
        auto env{"VSOMEIP_CONFIGURATION_" + _app_name};
        if (!env_vars_.count(env)) {
            ::setenv(env.c_str(), _config.c_str(), 1);
            env_vars_.insert(env);
        }
        create_app(_app_name);
        auto* vsip_app = start_client(_app_name);
        apps_[_app_name] = vsip_app;

        return vsip_app;
    }

    void stop_application(std::string _app_name) {
        stop_client(_app_name);
        apps_.erase(_app_name);
    }

    [[nodiscard]] bool successfully_registered(app* _app) {
        return _app && _app->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED);
    }

    void start_all_apps() {
        router_one_ = start_application(router_one_name_, "ecu_one.json");
        ecu_one_client_ = start_application(ecu_one_client_name_, "ecu_one.json");
        router_two_ = start_application(router_two_name_, "ecu_two.json");
        ecu_two_server_ = start_application(ecu_two_server_name_, "ecu_two.json");
        ASSERT_TRUE(successfully_registered(router_two_));
        ASSERT_TRUE(successfully_registered(router_one_));
        ASSERT_TRUE(successfully_registered(ecu_two_server_));
        ASSERT_TRUE(successfully_registered(ecu_one_client_));
    }

    [[nodiscard]] bool await_service(app* _client, std::optional<service_availability> expected_availability = std::nullopt) {
        if (!expected_availability) {
            expected_availability = service_availability::available(service_instance_);
        }
        return _client->availability_record_.wait_for_last(*expected_availability);
    }

    [[nodiscard]] bool subscribe_to_event(app* _client) {
        _client->subscribe_event(offered_event_);
        return _client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_));
    }

    [[nodiscard]] bool subscribe_to_field(app* _client) {
        _client->subscribe_field(offered_field_);
        return _client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_));
    }

    interface boardnet_interface_{0x3344, vsomeip::reliability_type_e::RT_UNRELIABLE};
    service_instance service_instance_{boardnet_interface_.instance_};
    event_ids offered_field_{boardnet_interface_.field_two_};
    event_ids offered_event_{boardnet_interface_.event_one_};

    message first_expected_message_{client_session{0, 1},
                                    boardnet_interface_.instance_,
                                    boardnet_interface_.field_two_.event_id_,
                                    vsomeip::message_type_e::MT_NOTIFICATION,
                                    {}};
    boost::asio::ip::udp::endpoint const ecu_two_sd_comm_{boost::asio::ip::make_address("160.48.199.99"), 30490};

    std::map<std::string, app*> apps_;
    std::set<std::string> env_vars_;

    app* router_one_{};
    app* router_two_{};
    app* ecu_one_client_{};
    app* ecu_two_server_{};
};

TEST_F(test_boardnet_helper, test_boardnet_service_availability) {
    router_one_ = start_application(router_one_name_, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(router_one_));

    router_two_ = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(router_two_));

    router_one_->request_service(service_instance_);
    router_two_->offer(boardnet_interface_);

    ASSERT_TRUE(await_service(router_one_));

    router_two_->stop_offer(service_instance_);
    ASSERT_TRUE(await_service(router_one_, service_availability::unavailable(service_instance_)));
}

TEST_F(test_boardnet_helper, test_boardnet_initial_event) {
    router_two_ = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(router_two_));

    ecu_two_server_ = start_application(ecu_two_server_name_, "ecu_two.json");

    router_one_ = start_application(router_one_name_, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(router_one_));

    router_one_->request_service(service_instance_);
    ecu_two_server_->offer(boardnet_interface_);
    ecu_two_server_->send_event(offered_field_, {0x5, 0x3});

    ASSERT_TRUE(await_service(router_one_));

    router_one_->subscribe_event(offered_field_);

    auto next_expected_message = first_expected_message_;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(router_one_->message_record_.wait_for_last(next_expected_message));

    // restarting ecu_blue
    TEST_LOG << "Stopping router two";
    stop_application(router_two_name_);
    router_one_->availability_record_.clear();
    router_one_->message_record_.clear();
    TEST_LOG << "Restarting router two";
    start_application(router_two_name_, "ecu_two.json");

    ASSERT_TRUE(await_service(router_one_));
    EXPECT_TRUE(router_one_->message_record_.wait_for_last(next_expected_message));
}

TEST_F(test_boardnet_helper, property_mismatch_regression) {
    // Regression test for property mismatch issue, where if a two clients request the same service though with different minors, with only
    // one of them fully matching the offered service, both would get the service availability and be able to manipulate the service
    // interface. When the client that required the correct minor releases the service, the remaining one would no longer be able to use the
    // boardnet service.
    //
    // 1. start applications (ECU one (Router+Client one + Client two) , ECU two (Router+Server)).
    // 2. Server offers the service with minor 1.
    // 3. Client one requests the service with minor 1.
    // 4. Client two requests and subscribes the service with minor 2 (wrong minor).
    // 5. Both clients should see the service as available.
    // 6. Client one releases the service.
    // 7. Server sends a notification.
    // 8. Client two receives message.

    // Create router and clients.
    start_all_apps();
    // Create second client
    auto ecu_one_client_two = start_application("ecu_one_client_two", "ecu_one.json");
    ASSERT_TRUE(successfully_registered(ecu_one_client_two));

    service_instance si_right_minor{service_instance_.service_, service_instance_.instance_, 1, 1};
    service_instance si_wrong_minor{service_instance_.service_, service_instance_.instance_, 1, 2};
    event_ids event_right_minor{si_right_minor, 0x8002, 0x1, {vsomeip::reliability_type_e::RT_UNRELIABLE}};
    event_ids event_wrong_minor{si_wrong_minor, 0x8002, 0x1, {vsomeip::reliability_type_e::RT_UNRELIABLE}};

    // Server offers the service.
    ecu_two_server_->offer(si_right_minor);
    ecu_two_server_->offer_field(event_right_minor);

    // Client one requests the service using the right minor version.
    ecu_one_client_->request_service(si_right_minor);
    // Client two requests the service using the wrong minor version.
    ecu_one_client_two->request_service(si_wrong_minor);
    ecu_one_client_two->subscribe_event(event_wrong_minor);

    // Clients receive availability.
    ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_last(service_availability::available(si_right_minor)));
    ASSERT_TRUE(ecu_one_client_two->availability_record_.wait_for_last(service_availability::available(si_wrong_minor)));

    // Client one releases the service.
    ecu_one_client_->release_service(si_right_minor);

    // Server sends a notification
    ecu_two_server_->send_event(event_right_minor, {0x5, 0x3});
    message expected_message{
            client_session{0, 1}, si_right_minor, event_wrong_minor.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, {}};
    expected_message.payload_ = {0x5, 0x3};

    // Client two should still be able to receive notification.
    EXPECT_TRUE(ecu_one_client_two->message_record_.wait_for_last(expected_message));
}

struct test_field_routing : test_boardnet_helper { };

TEST_F(test_field_routing, test_routing_with_str_on_server_side) {

    // 1. start applications (ECU one (Router+Client) , ECU two (Router+Server))
    // 2. Server offers the boardnet interface
    // 3. Client subscribes to the boardnet interface
    // 4. ECU two is suspended
    // 5. Client should see the service as unavailable
    // 6. ECU two is suspended for 250ms
    // 7. ECU two is resumed
    // 8. ECU one should see the service as available again
    // 9. Client should auto subscribe again

    // 1.
    start_all_apps();

    // 2.
    ecu_two_server_->offer(boardnet_interface_);

    // 3.
    ecu_one_client_->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(ecu_one_client_));

    // 4.
    router_two_->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);

    // 5.
    // once we are suspended, ECU two sends STOP OFFERs to the 'boardnet'
    // ECU one, will process these offers, the routing info is removed by the router (ECU one)
    // and therefore, it is expected the router to notify the client about the service unavailability
    ASSERT_TRUE(await_service(ecu_one_client_, service_availability::unavailable(service_instance_)));

    // 6.
    // This delay is purposely added to simulate a 'suspend'
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // 7.
    router_two_->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

    // 8.
    // Note there is no need to re-offer the service
    // Once ECU two is resumed, it will re-offer the 'boardnet' service
    // The service should be automatically available without even to re-request the client side
    ASSERT_TRUE(await_service(ecu_one_client_));

    // 9.
    // Once the service is available again, the client should auto-subscribe again to the offered event
    ASSERT_TRUE(ecu_one_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));
}

TEST_F(test_field_routing, test_routing_with_str_on_client_side) {

    // 1. start applications (ECU one (Router+Client) , ECU two (Router+Server))
    // 2. Server offers the boardnet interface
    // 3. Client subscribes to the boardnet interface
    // 4. ECU one is suspended
    // 5. Client should see the service as unavailable
    // 6. ECU one is suspended for 250ms
    // 7. ECU one is resumed
    // 8. ECU one should see the service as available again
    // 9. Client should auto subscribe again

    // 1.
    start_all_apps();

    // 2.
    ecu_two_server_->offer(boardnet_interface_);

    // 3.
    ecu_one_client_->request_service(service_instance_);
    ASSERT_TRUE(subscribe_to_event(ecu_one_client_));

    // 4.
    router_one_->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);

    // 5.
    // even though ECU two did not suspend, the client should regardless see the service as unavailable
    // once we are suspended, del_routing_info is called for the 'boardnet' services
    ASSERT_TRUE(await_service(ecu_one_client_, service_availability::unavailable(service_instance_)));

    // 6.
    // This delay is purposely added to simulate a 'suspend'
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // 7.
    router_one_->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);

    // 8.
    // Boardnet offers by ECU two, will now process normally once resumed and service discovery is started again
    ASSERT_TRUE(await_service(ecu_one_client_));

    // 9.
    // Once the service is available again, the client should now be able to subscribe again
    ASSERT_TRUE(ecu_one_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));
}

TEST_F(test_field_routing, server_router_router_client) {
    start_all_apps();

    ecu_one_client_->subscribe(boardnet_interface_);
    ecu_two_server_->offer(boardnet_interface_);
    ecu_two_server_->send_event(offered_field_, {0x5, 0x3});

    ASSERT_TRUE(await_service(ecu_one_client_));

    auto next_expected_message = first_expected_message_;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(ecu_one_client_->message_record_.wait_for_last(next_expected_message));
}
TEST_F(test_field_routing, server_router_router) {
    router_two_ = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(router_two_));

    ecu_two_server_ = start_application(ecu_two_server_name_, "ecu_two.json");
    ASSERT_TRUE(ecu_two_server_ && ecu_two_server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    router_one_ = start_application(router_one_name_, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(router_one_));

    router_one_->subscribe(boardnet_interface_);
    ecu_two_server_->offer(boardnet_interface_);
    ecu_two_server_->send_event(offered_field_, {0x5, 0x3});

    ASSERT_TRUE(await_service(router_one_));

    auto next_expected_message = first_expected_message_;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(router_one_->message_record_.wait_for_last(next_expected_message));
}
TEST_F(test_field_routing, router_router_client) {
    router_two_ = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(router_two_));

    router_one_ = start_application(router_one_name_, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(router_one_));
    ecu_one_client_ = start_application(ecu_one_client_name_, "ecu_one.json");
    ASSERT_TRUE(ecu_one_client_ && ecu_one_client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));

    ecu_one_client_->subscribe(boardnet_interface_);
    router_two_->offer(boardnet_interface_);
    router_two_->send_event(offered_field_, {0x5, 0x3});

    ASSERT_TRUE(await_service(ecu_one_client_));

    auto next_expected_message = first_expected_message_;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(ecu_one_client_->message_record_.wait_for_last(next_expected_message)) << next_expected_message;
}
TEST_F(test_field_routing, router_router) {
    router_two_ = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(router_two_));

    router_one_ = start_application(router_one_name_, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(router_one_));

    router_one_->subscribe(boardnet_interface_);
    router_two_->offer(boardnet_interface_);
    router_two_->send_event(offered_field_, {0x5, 0x3});

    ASSERT_TRUE(await_service(router_one_));

    auto next_expected_message = first_expected_message_;
    next_expected_message.payload_ = {0x5, 0x3};
    EXPECT_TRUE(router_one_->message_record_.wait_for_last(next_expected_message)) << next_expected_message;
}

struct test_shadow_events : test_boardnet_helper {
    void offer_event() {
        ASSERT_EQ(offered_event_.eventgroup_id_, offered_field_.eventgroup_id_);
        ecu_two_server_->offer_event(offered_event_);
    }
    void offer_field() {
        ASSERT_EQ(offered_event_.eventgroup_id_, offered_field_.eventgroup_id_);
        ecu_two_server_->offer_field(offered_field_);
    }
    void subscribe_to_event() { ecu_one_client_->subscribe_event(offered_event_); }
    void subscribe_to_field() { ecu_one_client_->subscribe_field(offered_field_); }

    void send_event() { ecu_two_server_->send_event(offered_event_, event_payload_); }
    void send_field() { ecu_two_server_->send_event(offered_field_, field_payload_); }

    std::vector<unsigned char> const field_payload_{0x22, 0x22};
    std::vector<unsigned char> const event_payload_{0x11, 0x11};
    message_checker const field_checker_{std::nullopt, service_instance_, offered_field_.event_id_,
                                         vsomeip::message_type_e::MT_NOTIFICATION, field_payload_};
    message_checker const event_checker_{std::nullopt, service_instance_, offered_event_.event_id_,
                                         vsomeip::message_type_e::MT_NOTIFICATION, event_payload_};
};
TEST_F(test_shadow_events, permute_eventgroup_sub_canonical_order) {
    start_all_apps();
    offer_event();
    offer_field();
    ecu_two_server_->offer(service_instance_);

    ecu_one_client_->request_service(service_instance_);
    subscribe_to_event();
    subscribe_to_field();

    send_field();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for(field_checker_));
    send_event();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for(event_checker_));
}
TEST_F(test_shadow_events, permute_eventgroup_sub_subscribe_to_field_after_event) {
    start_all_apps();
    offer_event();
    offer_field();
    ecu_two_server_->offer(service_instance_);

    ecu_one_client_->request_service(service_instance_);
    subscribe_to_event();
    ASSERT_TRUE(ecu_one_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));

    send_event();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for(event_checker_));

    subscribe_to_field();
    send_field();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for(field_checker_));
}

TEST_F(test_shadow_events, permute_eventgroup_sub_subscribe_to_field_with_init_value_after_event) {
    start_all_apps();
    offer_event();
    offer_field();
    ecu_two_server_->offer(service_instance_);
    send_field();

    ecu_one_client_->request_service(service_instance_);
    subscribe_to_event();
    ASSERT_TRUE(ecu_one_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));

    send_event();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for(event_checker_));

    subscribe_to_field();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for(field_checker_));
}
TEST_F(test_shadow_events, permute_eventgroup_sub_subscribe_before_offer) {
    GTEST_SKIP() << "This test fails sporadically. This feels like a bug";
    start_all_apps();
    ecu_two_server_->offer(service_instance_);
    subscribe_to_event();
    subscribe_to_field();
    ecu_one_client_->request_service(service_instance_);
    ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    offer_event();
    offer_field();
    ASSERT_TRUE(ecu_one_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));
    ASSERT_TRUE(ecu_one_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    send_event();
    send_field();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for(event_checker_));
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for(field_checker_));
}

TEST_F(test_shadow_events, permute_eventgroup_sub_late_field_offering) {
    GTEST_SKIP() << "This test fails reliable. This feels like a bug";
    start_all_apps();
    offer_event();
    ecu_two_server_->offer(service_instance_);

    ecu_one_client_->request_service(service_instance_);
    subscribe_to_field();
    subscribe_to_event();
    ASSERT_TRUE(ecu_one_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));

    send_event();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for(event_checker_));

    offer_field();
    send_field();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for(field_checker_));
}
TEST_F(test_boardnet_helper, test_boardnet_subscription_selective_event) {
    /**
     * When we receive multiple selective event subscriptions simultaneously
     * for the same Service/Instance/Eventgroup,the first thread adds the
     * client to the map and uses that map to process the subscription,
     * but another thread may start processing the next subscription, adding
     * the client to the map and starting to process with the current state
     * of the map, causing the ACK subscription not to be sent because the
     * second subscription, when it started, did not have the client from the
     * first subscription marked as ACK, leaving one client always in PENDING.
     * To replicate the issue, it is necessary to ensure that the SUBSCRIBE_ACK
     * from the previous subscription is processed while the next subscription
     * is being processed, so that the client map for each subscription does
     * not yet have the ACK information from the previous client.
     **/

    router_two_ = start_application(router_two_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(router_two_)) << "Router_two did not register";

    auto ecu_two_client = start_application(ecu_two_client_name_, "ecu_two.json");
    ASSERT_TRUE(successfully_registered(ecu_two_client)) << "ECU_two_C1 did not register";

    router_one_ = start_application(router_one_name_, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(router_one_)) << "Router_one did not register";

    auto ecu_one_server = start_application(ecu_one_server_name_, "ecu_one.json");
    ASSERT_TRUE(successfully_registered(ecu_one_server)) << "ECU_one_C1 did not register";

    router_two_->request_service(service_instance_);
    ecu_two_client->request_service(service_instance_);

    ecu_one_server->offer(service_instance_);
    ecu_one_server->offer_event(offered_event_);

    // Wait for service availability on all clients
    ASSERT_TRUE(router_two_->availability_record_.wait_for_last(service_availability::available(service_instance_)))
            << "Router_two did not see service availability";
    ASSERT_TRUE(ecu_two_client->availability_record_.wait_for_last(service_availability::available(service_instance_)))
            << "ECU_two_C1 did not see service availability";

    // Get Client IDs from applications
    const uint16_t router_two_id = router_two_->get_client_id();

    // Create the blocker and set up the handler ONCE
    ack_blocker blocker;

    // Handler for SUBSCRIBE_ACK (ecu_one_server -> router_one_)
    set_custom_command_handler(ecu_one_server_name_, router_one_name_, std::ref(blocker), socket_role::client);

    // Handler for SUBSCRIBE tracking
    set_custom_command_handler(
            ecu_one_server_name_, router_one_name_,
            [&blocker](command_message const& _cmd) {
                if (_cmd.id_ == protocol::id_e::SUBSCRIBE_ID) {
                    {
                        std::lock_guard<std::mutex> lock(blocker.mtx_);
                        ++blocker.subscribe_counter_;
                        blocker.cv_subscribe_.notify_all();
                    }
                }
                return false; // Don't block
            },
            socket_role::server);

    // Register which clients we want to block
    blocker.block_client(router_two_id);

    // Subscribe all three clients - ACKs will be blocked automatically
    router_two_->subscribe_selective(offered_event_);

    // Wait for SUBSCRIBE ACK message of router_two_ to be blocked on ecu_one_server
    blocker.wait_for_client_blocked(router_two_id);

    ecu_two_client->subscribe_selective(offered_event_);

    // Wait for SUBSCRIBE message of ecu_two_client to be sent to ecu_one_server
    blocker.wait_for_subscribe_count(2);
    // Release client 1's SUBSCRIPTION ACK to allow it to proceed
    blocker.release_client(router_two_id);

    // Block SD messages from ECU TWO to ECU ONE to prevent more subscriptions to be sent
    ASSERT_TRUE(delay_boardnet_sending(ecu_two_sd_comm_, true));

    // Wait for subscription confirmations
    ASSERT_TRUE(router_two_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)))
            << "Router two subscription confirmation failed";

    ASSERT_TRUE(ecu_two_client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)))
            << "ECU two C1 subscription confirmation failed";

    // Unblock SD communication from ECU TWO to ECU ONE
    ASSERT_TRUE(delay_boardnet_sending(ecu_two_sd_comm_, false));

    ecu_one_server->stop_offer(service_instance_);
}
}
