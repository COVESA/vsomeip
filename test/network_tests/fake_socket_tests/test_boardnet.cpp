// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sample_configurations.hpp"

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/ecu_setup.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/command_record.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/service_state.hpp"
#include "helpers/someip_gate.hpp"
#include "helpers/command_gate.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <utility>

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
        std::scoped_lock lock(mtx_);
        client_releases_[client_id] = std::make_shared<std::promise<void>>();
        client_order_.push_back(client_id);
    }

    // Release a specific client
    void release_client(uint16_t client_id) {
        std::unique_lock lock(mtx_);
        auto it = client_releases_.find(client_id);
        if (it != client_releases_.end()) {
            it->second->set_value();
            client_releases_.erase(it); // Remove after releasing
        }
    }

    // Wait until a specific client is blocked
    bool wait_for_client_blocked(uint16_t client_id, std::chrono::milliseconds timeout = std::chrono::milliseconds(3000)) {
        std::unique_lock lock(mtx_);
        return cv_.wait_for(lock, timeout, [this, client_id]() { return blocked_clients_.find(client_id) != blocked_clients_.end(); });
    }

    // Wait until N SUBSCRIBE messages have been received (without blocking them)
    bool wait_for_subscribe_count(size_t count, std::chrono::milliseconds timeout = std::chrono::milliseconds(3000)) {
        std::unique_lock lock(mtx_);
        return cv_subscribe_.wait_for(lock, timeout, [this, count]() { return subscribe_counter_ >= count; });
    }

    // Handler that will be called for each message
    bool operator()(command_message const& _cmd) {
        if (_cmd.id_ != protocol::id_e::SUBSCRIBE_ACK_ID) {
            return false; // Does not block, continues processing
        }

        std::unique_lock lock(mtx_);

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

    interface boardnet_interface_{0x3344};
    service_instance service_instance_{boardnet_interface_.instance_};
    event_ids offered_field_{boardnet_interface_.fields_[0]};
    event_ids offered_event_{boardnet_interface_.events_[0]};

    message_checker field_checker_{std::nullopt, boardnet_interface_.instance_, boardnet_interface_.fields_[0].event_id_,
                                   vsomeip::message_type_e::MT_NOTIFICATION, std::vector<unsigned char>{}};

    boost::asio::ip::udp::endpoint const ecu_one_sd_comm_{boost::asio::ip::make_address("160.48.199.65"), 30490};
    boost::asio::ip::udp::endpoint const ecu_two_sd_comm_{boost::asio::ip::make_address("160.48.199.99"), 30490};

    /// Endpoint used for SOME/IP communication between client and server (hosts).
    boost::asio::ip::udp::endpoint const ecu_one_client_port_{boost::asio::ip::make_address("160.48.199.65"), 30491};

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

    field_checker_.payload_ = {0x5, 0x3};
    EXPECT_TRUE(router_one_->message_record_.wait_for_last(field_checker_)) << "Failed to receive field event."
                                                                            << "\nRecord: " << router_one_->message_record_;

    // restarting ecu_blue
    TEST_LOG << "Stopping router two";
    stop_application(router_two_name_);
    router_one_->availability_record_.clear();
    router_one_->message_record_.clear();
    TEST_LOG << "Restarting router two";
    start_application(router_two_name_, "ecu_two.json");

    ASSERT_TRUE(await_service(router_one_));
    EXPECT_TRUE(router_one_->message_record_.wait_for_last(field_checker_)) << "Failed to receive field event."
                                                                            << "\nRecord: " << router_one_->message_record_;
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
    EXPECT_TRUE(ecu_one_client_two->message_record_.wait_for_last(expected_message)) << ecu_one_client_two->message_record_;
}

TEST_F(test_boardnet_helper, subscription_without_own_offer) {
    // Regression test for race condition where a subscription nack is sent due to service info not being ready to accept remote
    // subscriptioins. This occured with previous implementation that controlled if remote subscriptions were able to be accecpted if the
    // offer sent by the ECU was also received by the own offering ECU (check_sent_messages). On some non-deterministic scenarios, partner
    // ECUs could receive the the offer and send a subscribe without the offering ECU handled its own offer, this would lead a subscription
    // nack.

    // To replicate the behavior of the offering ECU "delaying/not receiving" its own offer, we're blocking all multicast joins for this
    // router.
    ignore_router_all_multicast_joins(router_two_name_, true);

    // Create router and clients.
    start_all_apps();

    // Client requests service and subscribes to it.
    ecu_one_client_->request_service(service_instance_);
    ecu_one_client_->subscribe_field(offered_field_);

    // Server offers service.
    ecu_two_server_->offer(boardnet_interface_);

    // Previously, this would fail as a subscription nack would be sent.
    EXPECT_FALSE(wait_for_sd_message(ecu_one_sd_comm_, {sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK, 0}, std::chrono::seconds(1)));
}

TEST_F(test_boardnet_helper, offer_service_before_event) {
    /// Test subscription handling when the event/field offering is delayed from the service offer.

    start_all_apps();

    // Client requests service and subscribes to it
    ecu_one_client_->request_service(service_instance_);
    ecu_one_client_->subscribe_field(offered_field_);

    // Server offers service. SD offers are sent.
    ecu_two_server_->offer(boardnet_interface_.instance_);

    // Client receives service availability and tries to subscribe to field, NACK must be received.
    EXPECT_TRUE(wait_for_sd_message(ecu_one_sd_comm_, {sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK, 0}));
    EXPECT_FALSE(
            wait_for_sd_message(ecu_one_sd_comm_, {sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK, 3}, std::chrono::milliseconds(500)));

    // Release service
    ecu_one_client_->release_service(service_instance_);
    clear_sd_message_record(ecu_one_sd_comm_);

    // Offer events and fields
    for (auto const& event : boardnet_interface_.events_) {
        ecu_two_server_->offer_event(event);
    }
    for (auto const& field : boardnet_interface_.fields_) {
        ecu_two_server_->offer_field(field);
    }

    // Request and subscribe again.
    ecu_one_client_->request_service(service_instance_);
    ecu_one_client_->subscribe_field(offered_field_);

    ecu_two_server_->send_event(offered_field_, {0x5, 0x3});

    // ACK must be received, no NACKs.
    EXPECT_FALSE(wait_for_sd_message(ecu_one_sd_comm_, {sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK, 0}, std::chrono::seconds(1)));
    EXPECT_TRUE(ecu_one_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    field_checker_.payload_ = {0x5, 0x3};
    EXPECT_TRUE(ecu_one_client_->message_record_.wait_for_last(field_checker_)) << "Failed to receive field event."
                                                                                << "\nRecord: " << ecu_one_client_->message_record_;
}

TEST_F(test_boardnet_helper, offer_event_before_service) {
    /// Test subscription handling when the event/field are offered before the service.

    start_all_apps();

    // Client requests service and subscribes to it
    ecu_one_client_->request_service(service_instance_);
    ecu_one_client_->subscribe_field(offered_field_);

    // Offer events and fields
    for (auto const& event : boardnet_interface_.events_) {
        ecu_two_server_->offer_event(event);
    }
    for (auto const& field : boardnet_interface_.fields_) {
        ecu_two_server_->offer_field(field);
    }

    // No ACK or NACK should be received, SD must not offer service only based on event and field.
    EXPECT_FALSE(wait_for_sd_message(ecu_one_sd_comm_, {sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK, 0}, std::chrono::seconds(1)));
    EXPECT_FALSE(wait_for_sd_message(ecu_one_sd_comm_, {sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK, 3}, std::chrono::seconds(1)));

    // Server offers service.
    ecu_two_server_->offer(boardnet_interface_.instance_);

    // Successful subscription.
    EXPECT_FALSE(wait_for_sd_message(ecu_one_sd_comm_, {sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK, 0}, std::chrono::seconds(1)));
    EXPECT_TRUE(wait_for_sd_message(ecu_one_sd_comm_, {sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK, 3}));
    EXPECT_TRUE(ecu_one_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)));

    ecu_two_server_->send_event(offered_field_, {0x5, 0x3});

    field_checker_.payload_ = {0x5, 0x3};
    EXPECT_TRUE(ecu_one_client_->message_record_.wait_for_last(field_checker_)) << "Failed to receive field event."
                                                                                << "\nRecord: " << ecu_one_client_->message_record_;
}

TEST_F(test_boardnet_helper, initial_event_received_after_availability_flap) {

    /**
     * Regression test for the race condition in routing_manager_impl::subscribe()
     * where the `if (inserted)` guard around `discovery_->subscribe()` prevents
     * the SD subscribe from being sent when the subscriber is already present in
     * the eventgroups_ set (inserted=false).
     *
     * The subscriber ends up in the set WITHOUT a corresponding SD subscribe
     * having been sent when subscribe() is called during a brief availability
     * flap: add_routing_info adds the service → del_routing_info immediately
     * removes it.  Between those two operations the ROUTING_INFO(ADD_SERVICE)
     * is dispatched to the client.  When the client's pending subscription fires
     * (on seeing ADD_SERVICE), the SUBSCRIBE command arrives at
     * routing_manager_impl AFTER del_routing_info has already removed the
     * service from services_.  So:
     *   - add_subscriber()  → inserted=true  (set was cleared by del_routing_info)
     *   - find_service()    → null            (service gone from services_)
     *   - discovery_->subscribe() NOT called
     *   - subscriber STUCK in eventgroups_ set
     *
     * When the service truly comes back and the client re-subscribes:
     *   - add_subscriber()  → inserted=false  (already in set from the flap)
     *   - if (inserted) block SKIPPED entirely
     *   - discovery_->subscribe() NOT called  ← root cause
     *   - server never delivers the initial event → timeout
     *
     * The gate defers ROUTING_INFO(ADD_SERVICE) delivery to the client until
     * AFTER del_routing_info has removed the service from services_, making
     * the race deterministic without valgrind.
     */

    // -------------------------------------------------------------------------
    // Phase 1: establish baseline subscription and receive initial event
    // -------------------------------------------------------------------------
    router_two_ = start_application(router_two_name_, "ecu_two.json");
    ecu_two_server_ = start_application(ecu_two_server_name_, "ecu_two.json");
    router_one_ = start_application(router_one_name_, "ecu_one.json");

    auto gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(ecu_one_client_name_, router_one_name_, socket_role::client, gate->get_data_pipe()));

    ecu_one_client_ = start_application(ecu_one_client_name_, "ecu_one.json");

    ASSERT_TRUE(successfully_registered(router_two_));
    ASSERT_TRUE(successfully_registered(router_one_));
    ASSERT_TRUE(successfully_registered(ecu_two_server_));
    ASSERT_TRUE(successfully_registered(ecu_one_client_));

    ecu_one_client_->request_service(service_instance_);
    ecu_two_server_->offer(boardnet_interface_);
    ecu_two_server_->send_event(offered_field_, {0x01});

    ecu_one_client_->subscribe_field(offered_field_);
    ASSERT_TRUE(ecu_one_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field_)))
            << "Initial subscription must be acknowledged";

    message_checker const initial_checker{std::nullopt, service_instance_, offered_field_.event_id_,
                                          vsomeip::message_type_e::MT_NOTIFICATION, std::vector<unsigned char>{0x01}};
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for(initial_checker)) << "Client must receive initial event with value {0x01}";

    // -------------------------------------------------------------------------
    // Phase 2: trigger the availability flap
    //
    // Sequence at routing_manager_impl (router_one) side:
    //   (a) stop_offer (first)  → del_routing_info: subscriber cleared, service gone
    //       ROUTING_INFO(DEL_SERVICE) → client sees UNAVAILABLE  [delivered normally]
    //       NOTE: we then wait > cyclic_offer_delay before (c). Otherwise the
    //       SD STOP_OFFER graceful window would suppress the
    //       second OFFER and router_one would never emit ADD_SERVICE.
    //   (b) Arm gate to block on first ROUTING_INFO from router_one
    //   (c) re-offer            → router_one calls add_routing_info:
    //       service in services_; ROUTING_INFO(ADD_SERVICE) → blocked by gate
    //   (d) wait_for_blocked()  → ADD_SERVICE held in gate; service IS in services_
    //   (e) stop_offer (second) → del_routing_info: service GONE from services_;
    //       ROUTING_INFO(DEL_SERVICE) also buffered in gate
    //   (f) sleep(200ms)        → del_routing_info guaranteed to have completed
    //   (g) gate releases → client processes ADD_SERVICE + DEL_SERVICE:
    //       ADD_SERVICE → on_availability(true) → SUBSCRIBE sent
    //         ↳ routing_manager_impl::subscribe():
    //             inserted = add_subscriber()  → TRUE  (set cleared by del_routing_info (e))
    //             find_service()               → NULL  (service gone from (e))
    //             discovery_->subscribe()  NOT called
    //             subscriber STUCK in eventgroups_ set
    //       DEL_SERVICE → on_availability(false)
    // -------------------------------------------------------------------------

    ecu_one_client_->availability_record_.clear();
    ecu_one_client_->message_record_.clear();

    // (a) First stop_offer: DEL_SERVICE delivered to client normally.
    ecu_two_server_->stop_offer(service_instance_);
    ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)))
            << "Client must see service as unavailable after stop_offer";

    // Wait out the SD STOP_OFFER graceful window: an offer within cyclic_offer_delay
    // would be suppressed and no ADD_SERVICE would reach router_one (gate would never trigger).
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // (b) Arm gate to block on the first ROUTING_INFO from router_one.
    gate->block_at(protocol::id_e::ROUTING_INFO_ID);

    // (c) Re-offer: router_one calls add_routing_info and sends ADD_SERVICE.
    ecu_two_server_->offer(boardnet_interface_);

    // (d) Wait until ADD_SERVICE is captured inside the gate.
    ASSERT_TRUE(gate->wait_for_blocked()) << "Gate must have captured the ADD_SERVICE ROUTING_INFO";

    // (e) Stop again: del_routing_info runs; service is now GONE from services_.
    ecu_two_server_->stop_offer(service_instance_);

    // (f) Give router_one's SD thread time to process the STOP_OFFER and complete
    // del_routing_info so the service is definitely absent from services_.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // (g) Release gate: client receives ADD_SERVICE (stale) + DEL_SERVICE.
    //     ADD_SERVICE → on_availability(true) → SUBSCRIBE → inserted=true, find_service=null
    //                                         → subscriber stuck, no SD subscribe sent
    //     DEL_SERVICE → on_availability(false)
    ecu_one_client_->availability_record_.clear();
    gate->block(false);

    ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_any(service_availability::available(service_instance_)))
            << "Client must briefly see stale availability (ADD_SERVICE processed)";
    ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)))
            << "Client must end up unavailable (DEL_SERVICE processed)";

    // -------------------------------------------------------------------------
    // Phase 3: real offer — verify the initial event IS delivered
    // -------------------------------------------------------------------------
    ecu_one_client_->message_record_.clear();

    ecu_two_server_->offer(boardnet_interface_);
    ecu_two_server_->send_event(offered_field_, {0x02});

    message_checker const resubscribe_checker{std::nullopt, service_instance_, offered_field_.event_id_,
                                              vsomeip::message_type_e::MT_NOTIFICATION, std::vector<unsigned char>{0x02}};
    EXPECT_TRUE(ecu_one_client_->message_record_.wait_for(resubscribe_checker))
            << "Client must receive initial event after availability flap.";
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

    field_checker_.payload_ = {0x5, 0x3};
    EXPECT_TRUE(ecu_one_client_->message_record_.wait_for_last(field_checker_)) << "Failed to receive field event."
                                                                                << "\nRecord: " << ecu_one_client_->message_record_;
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

    field_checker_.payload_ = {0x5, 0x3};
    EXPECT_TRUE(router_one_->message_record_.wait_for_last(field_checker_)) << "Failed to receive field event."
                                                                            << "\nRecord: " << router_one_->message_record_;
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

    field_checker_.payload_ = {0x5, 0x3};
    EXPECT_TRUE(ecu_one_client_->message_record_.wait_for_last(field_checker_)) << "Failed to receive field event."
                                                                                << "\nRecord: " << ecu_one_client_->message_record_;
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

    field_checker_.payload_ = {0x5, 0x3};
    EXPECT_TRUE(router_one_->message_record_.wait_for_last(field_checker_)) << "Failed to receive field event."
                                                                            << "\nRecord: " << router_one_->message_record_;
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
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for_any(field_checker_)) << "Failed to receive field event."
                                                                               << "\nRecord: " << ecu_one_client_->message_record_;
    send_event();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for_any(event_checker_)) << "Failed to receive event."
                                                                               << "\nRecord: " << ecu_one_client_->message_record_;
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
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for_any(event_checker_)) << "Failed to receive event."
                                                                               << "\nRecord: " << ecu_one_client_->message_record_;

    subscribe_to_field();
    send_field();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for_any(field_checker_)) << "Failed to receive field event."
                                                                               << "\nRecord: " << ecu_one_client_->message_record_;
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
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for_any(event_checker_)) << "Failed to receive event."
                                                                               << "\nRecord: " << ecu_one_client_->message_record_;

    subscribe_to_field();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for_any(field_checker_)) << "Failed to receive field event."
                                                                               << "\nRecord: " << ecu_one_client_->message_record_;
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
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for_any(event_checker_)) << "Failed to receive event."
                                                                               << "\nRecord: " << ecu_one_client_->message_record_;
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for_any(field_checker_)) << "Failed to receive field event."
                                                                               << "\nRecord: " << ecu_one_client_->message_record_;
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
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for_any(event_checker_)) << "Failed to receive event."
                                                                               << "\nRecord: " << ecu_one_client_->message_record_;

    offer_field();
    send_field();
    ASSERT_TRUE(ecu_one_client_->message_record_.wait_for_any(field_checker_)) << "Failed to receive field event."
                                                                               << "\nRecord: " << ecu_one_client_->message_record_;
    ;
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
                        std::scoped_lock lock(blocker.mtx_);
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

TEST_F(test_boardnet_helper, udp_connection_refused_recv) {
    // This test checks for a regression where vsomeip would be stuck in an availability loop when
    // an UDP endpoint would receive a connection refused error.
    //
    // This was usually caused by not observing the service provider's STOP OFFER and trying to send
    // messages to the UDP port where the service was previously offered.

    // Create client, server and routing hosts.
    start_all_apps();

    // Offer the test service.
    ecu_two_server_->offer(service_instance_);

    // Wait for it to become available.
    ecu_one_client_->request_service(service_instance_);
    ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    // Stop SD communication, simulating a shutdown without STOP OFFERs.
    ASSERT_TRUE(delay_boardnet_sending(ecu_two_sd_comm_, true));

    // Insert a "Connection refused" error in the client-server connection.
    ASSERT_TRUE(insert_udp_recv_error(ecu_one_client_port_, boost::asio::error::connection_refused));

    // Client receives unavailable.
    ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

    // Client does not receive available.
    ASSERT_FALSE(ecu_one_client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    // Offer the test service again.
    ASSERT_TRUE(delay_boardnet_sending(ecu_two_sd_comm_, false));

    // Client receives available.
    ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
}

TEST_F(test_boardnet_helper, udp_connection_refused_send) {
    // This test checks for a regression where vsomeip would be stuck in an availability loop when
    // an UDP endpoint would fail to send with a connection refused error.
    //
    // This was usually caused by not observing the service provider's STOP OFFER and trying to send
    // messages to the UDP port where the service was previously offered.

    // Create client, server and routing hosts.
    start_all_apps();

    // Offer the test service.
    ecu_two_server_->offer(service_instance_);

    // Wait for it to become available.
    ecu_one_client_->request_service(service_instance_);
    ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    // Stop SD communication, simulating a shutdown without STOP OFFERs.
    ASSERT_TRUE(delay_boardnet_sending(ecu_two_sd_comm_, true));

    // Insert a "Connection refused" error in the client-server connection.
    ASSERT_TRUE(insert_udp_send_error(ecu_one_client_port_, boost::asio::error::connection_refused));

    // Send a message to the service in order to trigger the error.
    request request_ = {service_instance_, 0x1111, vsomeip_v3::message_type_e::MT_REQUEST, {}};
    ecu_one_client_->send_request(request_);

    // Client receives unavailable.
    ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_last(service_availability::unavailable(service_instance_)));

    // Client does not receive available.
    ASSERT_FALSE(ecu_one_client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));

    // Offer the test service again.
    ASSERT_TRUE(delay_boardnet_sending(ecu_two_sd_comm_, false));

    // Client receives available.
    ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
}

struct guest_offering : public base_fake_socket_fixture {
    ecu_setup ecu_one_{"ecu_one", boardnet::ecu_one_config, *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", boardnet::ecu_two_config, *socket_manager_};
};

TEST_F(guest_offering, guests_provide_and_consume_interface) {
    // add anonymous guest client to both ecus
    ecu_one_.add_guest({"guest_client", std::nullopt});
    ecu_two_.add_guest({"guest_server", std::nullopt});
    // setup both configurations for all apps
    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    // ecu_two offers the service on the boardnet
    auto* server = ecu_two_.apps_["guest_server"];
    server->offer(interfaces::boardnet::service_3344);

    // ecu_one's dynamic client subscribes to it
    auto* client = ecu_one_.apps_["guest_client"];
    client->subscribe(interfaces::boardnet::service_3344);

    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::available(interfaces::boardnet::service_3344.instance_)));
    // wait_for_any, because we also subscribe for the event and this might come in last
    EXPECT_TRUE(client->subscription_record_.wait_for_any(
            event_subscription::successfully_subscribed_to(interfaces::boardnet::service_3344.fields_[0])));
}

TEST_F(guest_offering, replicate_vhal_behavior) {
    // Replication of VHAL behavior, where a service is stopped and offered again during high CPU loads or whatever other situation that
    // might introduce any kind of scheduling delay in vsomeip, the daemon could processes an incoming subscription request triggered by
    // offers sent previously of the stop offer, and internally the service is offered again, but any offer have yet to be sent by SD,
    // vsomeip would respond with a SubscribeACK and sends the initial event, though the other ecu will ignore it as at that moment it
    // doesn't receive the offer.

    // Create router and clients.
    ecu_one_.add_guest({"guest_client", std::nullopt});
    ecu_two_.add_guest({"guest_server", std::nullopt});

    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* server = ecu_two_.apps_["guest_server"];
    auto* client = ecu_one_.apps_["guest_client"];

    // Create and setup sd gates to replicate high scheduling delays in network stack.
    std::shared_ptr<someip_gate> router_one_multicast_sd_gate = someip_gate::create();
    std::shared_ptr<someip_gate> router_one_sending_sd_gate = someip_gate::create();

    ASSERT_TRUE(setup_data_pipe(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::any(), ecu_one_.sd_endpoint().port()),
                                router_one_name_, socket_role::client, router_one_multicast_sd_gate->get_data_pipe()));
    ASSERT_TRUE(
            setup_data_pipe(ecu_one_.sd_endpoint(), router_one_name_, socket_role::server, router_one_sending_sd_gate->get_data_pipe()));

    // Block the offer to ensure the no client request is sent before the service is offered, as this would be the trigger a unicast offer,
    // so offer and when the datagram reaches the "network stack", allow the client to request the service.
    router_one_multicast_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 3}, 1);
    // Server offers service.
    server->offer(interfaces::boardnet::service_3344);
    ASSERT_TRUE(router_one_multicast_sd_gate->wait_for_blocked());
    router_one_multicast_sd_gate->block(false);
    // Client requests service and subscriptions must be blocked.
    router_one_sending_sd_gate->block_at({sd::entry_type_e::SUBSCRIBE_EVENTGROUP, 3}, 1);
    client->request_service(interfaces::boardnet::service_3344.instance_);
    client->subscribe(interfaces::boardnet::service_3344);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::available(interfaces::boardnet::service_3344.instance_)));
    // Subscription has been blocked by sender gate.
    ASSERT_TRUE(router_one_sending_sd_gate->wait_for_blocked());

    // Server stops offering the service, client needs to see it.
    server->stop_offer(interfaces::boardnet::service_3344.instance_);
    EXPECT_TRUE(
            client->availability_record_.wait_for_last(service_availability::unavailable(interfaces::boardnet::service_3344.instance_)));

    // Server offers again the service, though it must be blocked as the client can not receive it.
    router_one_multicast_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 3}, 1);
    server->offer(interfaces::boardnet::service_3344);
    // Set initial event.
    server->send_event(interfaces::boardnet::service_3344.fields_[0], {0x5, 0x3});
    // Offer blocked by receiving ecu one multicast sd endpoint.
    ASSERT_TRUE(router_one_multicast_sd_gate->wait_for_blocked(std::chrono::seconds(3)));

    // Release the client subscription and wait for the sub ack.
    router_one_sending_sd_gate->block(false);
    EXPECT_TRUE(wait_for_sd_message(ecu_one_.sd_endpoint(), {sd::entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK, 3}));

    // Initial event never received as the client endpoint has been destroyed when the stop offer was received.
    message expected_message{client_session{0, 1},
                             interfaces::boardnet::service_3344.instance_,
                             interfaces::boardnet::service_3344.fields_[0].event_id_,
                             vsomeip::message_type_e::MT_NOTIFICATION,
                             {0x5, 0x3}};
    EXPECT_FALSE(client->message_record_.wait_for_last(expected_message));
}

struct server_offering_multiple_fields : public base_fake_socket_fixture {

    // Custom interface with 10 fields
    std::vector<interface::event_spec> const fields_specs_{
            {0x8002, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE}, {0x8003, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE},
            {0x8004, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE}, {0x8005, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE},
            {0x8006, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE}, {0x8007, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE},
            {0x8008, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE}, {0x8009, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE},
            {0x800a, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE}, {0x800b, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE},
    };
    interface multi_field_service_{0x3344, {}, fields_specs_};
    ecu_config ecu_one_config_extended_{boardnet::ecu_one_config};
    ecu_config ecu_two_config_extended_{boardnet::ecu_two_config};

    ecu_setup ecu_one_{"ecu_one", ecu_one_config_extended_.add_interface({multi_field_service_}), *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", ecu_two_config_extended_.add_interface({multi_field_service_}), *socket_manager_};

    void prepare_ecus_and_apps() {
        ecu_one_.add_guest({"guest_server", 0x1337});
        ecu_two_.add_guest({"guest_client", 0x1338});

        ecu_one_.prepare();
        ecu_two_.prepare();

        ecu_one_.start_apps();
        ecu_two_.start_apps();
    }
};

TEST_F(server_offering_multiple_fields, guests_provide_and_consume_multiple_fields) {
    // Purpose is purely to test the trivial scenario where there is a consumer that is interested in
    // multiple fields of the same service, and the provider offers all these fields together.
    // This is to verify that there are no issues with the handling of multiple fields in the
    // same service, and that the client can receive events from all these fields without any problem.

    prepare_ecus_and_apps();

    auto* server = ecu_one_.apps_["guest_server"];
    server->offer(multi_field_service_);

    // We need it to set the respective payloads for the events of each field,
    // so we send them before subscribing, they should be cached and delivered once the subscription is done
    for (size_t i = 0; i < fields_specs_.size(); ++i) {
        std::vector<unsigned char> payload{static_cast<unsigned char>(0x10 + i), static_cast<unsigned char>(i)};
        server->send_event(multi_field_service_.fields_[i], payload);
    }

    // ecu_one's dynamic client subscribes to all 10 fields
    auto* client = ecu_two_.apps_["guest_client"];
    client->request_service(multi_field_service_.instance_);

    // Assert because it no longer makes sense to further continue with the test steps
    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::available(interfaces::boardnet::service_3344.instance_)));

    // Note that the provider will be registering all the events
    // otherwise, initial events for all fields won't be received
    client->subscribe(multi_field_service_);

    // Verify client receives all 10 events using message_checker for flexible matching
    // These should be the cached events from before subscription
    for (size_t i = 0; i < fields_specs_.size(); ++i) {
        std::vector<unsigned char> expected_payload{static_cast<unsigned char>(0x10 + i), static_cast<unsigned char>(i)};
        message_checker checker{std::nullopt, multi_field_service_.instance_, multi_field_service_.fields_[i].event_id_,
                                vsomeip::message_type_e::MT_NOTIFICATION, expected_payload};
        EXPECT_TRUE(client->message_record_.wait_for_any(checker))
                << "Failed to receive event for field 0x" << std::hex << (multi_field_service_.fields_[0].event_id_ + i)
                << "\nRecord: " << client->message_record_.to_string();
    }
}

TEST_F(server_offering_multiple_fields, test_sd_unicat_gate_early_loading) {
    // Depict example where we setup a sending sd gate for unicast early loading (app not started).
    ecu_one_.add_guest({"guest_client", std::nullopt});
    ecu_two_.add_guest({"guest_server", std::nullopt});

    ecu_one_.prepare();
    ecu_two_.prepare();

    // Create the gate and prepare the pipe, will be exchanged as soon as the SD unicast endpoint from ecu two is binded.
    std::shared_ptr<someip_gate> router_two_sd_gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(ecu_two_.sd_endpoint(), router_two_name_, socket_role::server, router_two_sd_gate->get_data_pipe()));

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* client = ecu_one_.apps_["guest_client"];
    auto* server = ecu_two_.apps_["guest_server"];

    // Block the pipe when ecu two tries to send an offer.
    router_two_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 3}, 1);
    client->request_service(multi_field_service_.instance_);
    client->subscribe(multi_field_service_);
    server->offer(multi_field_service_);

    // Guarantee the gate has been blocked.
    ASSERT_TRUE(router_two_sd_gate->wait_for_blocked());
    // Check that no offer has been received.
    EXPECT_FALSE(client->availability_record_.wait_for_last(service_availability::available(multi_field_service_.instance_)));
    // Release the gate, message pushes through.
    router_two_sd_gate->block(false);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::available(multi_field_service_.instance_)));

    server->stop_offer(multi_field_service_.instance_);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::unavailable(multi_field_service_.instance_)));
}

TEST_F(server_offering_multiple_fields, test_sd_multicast_gate_late_loading) {
    // Depict example where we setup a receiving sd gate for multicast with late loading (app already started).
    ecu_one_.add_guest({"guest_client", std::nullopt});
    ecu_two_.add_guest({"guest_server", std::nullopt});

    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* server = ecu_two_.apps_["guest_server"];
    auto* client = ecu_one_.apps_["guest_client"];

    // Create the gate and exchange the pipe immediatly.
    std::shared_ptr<someip_gate> router_one_sd_gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::any(), ecu_one_.sd_endpoint().port()),
                                router_one_name_, socket_role::client, router_one_sd_gate->get_data_pipe()));

    // Block the pipe when ecu one receved an offer.
    router_one_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 3}, 1);

    client->request_service(multi_field_service_.instance_);
    client->subscribe(multi_field_service_);
    server->offer(multi_field_service_);

    // Guarantee the gate has been blocked.
    ASSERT_TRUE(router_one_sd_gate->wait_for_blocked());
    // Check that no offer has been received.
    EXPECT_FALSE(client->availability_record_.wait_for_last(service_availability::available(multi_field_service_.instance_)));
    // Release the gate, message is consumed and processed.
    router_one_sd_gate->block(false);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::available(multi_field_service_.instance_)));

    server->stop_offer(multi_field_service_.instance_);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::unavailable(multi_field_service_.instance_)));
}

TEST_F(server_offering_multiple_fields, graceful_stop_offer) {
    // Tests the graceful stop offer mechanism, where after a STOP OFFER, the routing manager starts a timer during which new offers
    // for the same service are not propagated immediately, but deferred until the timer expires.

    prepare_ecus_and_apps();
    auto* server = ecu_one_.apps_["guest_server"];
    auto* client = ecu_two_.apps_["guest_client"];

    client->request_service(multi_field_service_.instance_);
    server->offer(multi_field_service_);
    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::available(multi_field_service_.instance_)));

    server->stop_offer(multi_field_service_.instance_);
    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::unavailable(multi_field_service_.instance_)));

    client->availability_record_.clear();

    // Offer inside the graceful stop offer window: must NOT propagate immediately.
    server->offer(multi_field_service_);
    EXPECT_FALSE(client->availability_record_.wait_for_last(service_availability::available(multi_field_service_.instance_),
                                                            std::chrono::milliseconds(500)))
            << "Offer must not propagate during graceful stop-offer window.";

    // No further user-side calls: the routing manager must auto-emit the
    // deferred offer when the timer expires. Wait for offer delay cycle + initial wait phase.
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::available(multi_field_service_.instance_),
                                                           std::chrono::milliseconds(2600)))
            << "Pending in-window offer was not auto-emitted after graceful timeout.";
}

struct tcp_notifications : public base_fake_socket_fixture {

    // Custom interface with a service that has events being notified via tcp and udp
    std::vector<interface::event_spec> const event_specs_both_{{0x8001, 0x1, vsomeip::reliability_type_e::RT_RELIABLE},
                                                               {0x8002, 0x2, vsomeip::reliability_type_e::RT_UNRELIABLE}};
    interface both_interface{0x3345, {}, event_specs_both_};
    // Second service to check if offering two services via tcp does not cause issues
    interface second_interface{0x3346, {}, event_specs_both_};

    ecu_config ecu_one_config_tcp_cfg{boardnet::ecu_one_config};
    ecu_config ecu_two_config_tcp_cfg{boardnet::ecu_two_config};

    ecu_setup ecu_one_{"ecu_one", ecu_one_config_tcp_cfg, *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", ecu_two_config_tcp_cfg.add_interface({both_interface, second_interface}), *socket_manager_};

    event_ids tcp_offered_field{both_interface.fields_[0]};
    event_ids udp_offered_field{both_interface.fields_[1]};

    event_ids second_interface_tcp_offered_field{second_interface.fields_[0]};
};

TEST_F(tcp_notifications, test_tcp_and_udp_boardnet_initial_event) {

    // Test steps:
    // 1. start applications (ECU one (Router+Client) , ECU two (Router+Server))
    // 2. Server offers the TCP+UDP boardnet interface and notifies
    // 3. Both ECU one apps (Router+Client) subscribe to the service,
    //    with the router subscribing via TCP and the client via UDP
    // 4. Both client should receive the notification

    // 1.
    ecu_one_.add_app(ecu_one_client_name_);
    ecu_two_.add_app(ecu_two_server_name_);
    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one_ = ecu_one_.router_;
    auto* ecu_one_client_ = ecu_one_.apps_[ecu_one_client_name_];
    auto* ecu_two_server_ = ecu_two_.apps_[ecu_two_server_name_];

    // 2.
    std::vector<unsigned char> event_payload = {0x5, 0x3};

    ecu_two_server_->offer(both_interface);

    ecu_two_server_->send_event(tcp_offered_field, event_payload);
    ecu_two_server_->send_event(udp_offered_field, event_payload);
    ecu_two_server_->offer(second_interface);
    ecu_two_server_->send_event(second_interface_tcp_offered_field, event_payload);

    // 3.
    router_one_->request_service(both_interface.instance_);
    ecu_one_client_->request_service(both_interface.instance_);
    ecu_one_client_->request_service(second_interface.instance_);

    router_one_->subscribe_event({tcp_offered_field});
    ecu_one_client_->subscribe_event({udp_offered_field});
    ecu_one_client_->subscribe_event({second_interface_tcp_offered_field});

    // 4.
    message_checker tcp_checker{std::nullopt, both_interface.instance_, tcp_offered_field.event_id_,
                                vsomeip::message_type_e::MT_NOTIFICATION, event_payload};
    EXPECT_TRUE(router_one_->message_record_.wait_for_any(tcp_checker))
            << "Failed to receive tcp field" << "\nRecord: " << router_one_->message_record_;

    message_checker udp_checker{std::nullopt, both_interface.instance_, udp_offered_field.event_id_,
                                vsomeip::message_type_e::MT_NOTIFICATION, event_payload};

    EXPECT_TRUE(ecu_one_client_->message_record_.wait_for_any(udp_checker))
            << "Failed to receive udp field" << "\nRecord: " << ecu_one_client_->message_record_;

    message_checker second_checker{std::nullopt, second_interface.instance_, second_interface_tcp_offered_field.event_id_,
                                   vsomeip::message_type_e::MT_NOTIFICATION, event_payload};

    EXPECT_TRUE(ecu_one_client_->message_record_.wait_for_any(second_checker))
            << "Failed to receive second field" << "\nRecord: " << ecu_one_client_->message_record_;
}

static ecu_config change_ecu_one_cfg_name(std::string name) {
    ecu_config cfg{boardnet::ecu_one_config};
    std::get<local_tcp_config>(*cfg.routing_config_).router_name_ = name;
    cfg.apps_[0].name_ = name;
    return cfg;
}

struct tcp_offers : public base_fake_socket_fixture {

    // Custom interface with a service that has events being notified via tcp and udp
    std::vector<interface::event_spec> const event_specs_both_{{0x8001, 0x1, vsomeip::reliability_type_e::RT_RELIABLE},
                                                               {0x8002, 0x2, vsomeip::reliability_type_e::RT_UNRELIABLE}};
    interface both_interface{0x3345, {}, event_specs_both_};
    // Second service to check if offering two services via tcp does not cause issues
    interface second_interface{0x3346, {}, event_specs_both_};

    ecu_config ecu_one_config_tcp_cfg{change_ecu_one_cfg_name("rrouter_one")};
    ecu_config ecu_two_config_tcp_cfg{boardnet::ecu_two_config};

    ecu_setup ecu_one_{"ecu_one", ecu_one_config_tcp_cfg, *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", ecu_two_config_tcp_cfg.add_interface({both_interface, second_interface}), *socket_manager_};

    event_ids tcp_offered_field{both_interface.fields_[0]};
    event_ids udp_offered_field{both_interface.fields_[1]};

    event_ids second_interface_tcp_offered_field{second_interface.fields_[0]};
};

// Regression test for the auxiliary io_context pre-reservation bug.
//
// Root cause:
//   ecu_setup::start_one() calls sm_.add(router_name_ + "_auxiliary_context_") immediately
//   after the router is assigned, leaving a nullptr slot in the alphabetically-sorted
//   std::map<std::string, io_context*> inside socket_manager.
//   socket_manager::try_add() always picks the *first* nullptr entry it finds.
//   If ECU two starts first, the map looks like:
//
//     "router_two"                    -> <ptr>   (assigned)
//     "router_two_auxiliary_context_" -> nullptr (reserved — waiting for ECU two's aux context)
//     "rrouter_one"                   -> nullptr (registered for ECU one's router)
//
//   When ECU one's router ("rrouter_one") creates its first socket, try_add() finds
//   "router_two_auxiliary_context_" (nullptr) before "rrouter_one" (nullptr) because
//   at position 1: 'o' (0x6f) < 'r' (0x72).  ECU one's io_context is mis-assigned to
//   the auxiliary slot; await_assignment("rrouter_one") times out; ecu_one_.router_ = nullptr.
//
//   WHY "rrouter_one" AND NOT "router_one":
//     "router_one" shares the prefix "router_" and then 'o' < 't', so it sorts BEFORE
//     "router_two_auxiliary_context_" → unaffected by the bug.
//     "rrouter_one" has 'r' at position 1 vs 'o' in "router_..." → sorts AFTER the slot
//     → its io_context is stolen → bug is deterministically exposed.
TEST_F(tcp_offers, auxiliary_context_slot_does_not_steal_router_io_context) {
    // ECU two starts FIRST: router_two is assigned, then immediately pre-registers the
    // "router_two_auxiliary_context_" nullptr slot.
    ecu_two_.add_app(ecu_two_server_name_);
    ecu_two_.prepare();
    ecu_two_.start_apps();
    auto* ecu_two_server_ = ecu_two_.apps_[ecu_two_server_name_];

    // ECU one starts SECOND: "rrouter_one"'s io_context arrives in try_add() and — with
    // the bug — is stolen into "router_two_auxiliary_context_" instead.
    ecu_one_.add_app(ecu_one_client_name_);
    ecu_one_.prepare();
    ecu_one_.start_apps();

    auto* router_one_ = ecu_one_.router_;

    ecu_two_server_->offer(both_interface);

    // Verify end-to-end: router_one_ must be able to see the offered service.
    router_one_->request_service(both_interface.instance_);

    ASSERT_TRUE(router_one_->availability_record_.wait_for_last(service_availability::available(both_interface.instance_)))
            << "router_one_ (\"rrouter_one\") did not receive service availability — "
               "likely because its io_context was not properly assigned.";
}

struct test_someip_gate : public base_fake_socket_fixture {
    // TCP interface with one reliable field (0x8001) and one unreliable field (0x8002).
    // Used by blocks_notification and blocks_notification_matching_payload.
    std::vector<interface::event_spec> const event_specs_both_{{0x8001, 0x1, vsomeip::reliability_type_e::RT_RELIABLE},
                                                               {0x8002, 0x2, vsomeip::reliability_type_e::RT_UNRELIABLE}};
    interface both_interface{0x3345, {}, event_specs_both_};

    // UDP-only service (0x3347). Used by blocks_request_then_response.
    interface const udp_svc_{0x3347, {interface::event_spec{0x8001, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE}}, {}};

    ecu_config ecu_one_cfg_{boardnet::ecu_one_config};
    ecu_config ecu_two_cfg_{boardnet::ecu_two_config};

    ecu_setup ecu_one_{"ecu_one", ecu_one_cfg_, *socket_manager_};
    // both_interface gets unreliable=30501, reliable=30502; udp_svc_ gets unreliable=30503.
    ecu_setup ecu_two_{"ecu_two", ecu_two_cfg_.add_interface({both_interface, udp_svc_}), *socket_manager_};

    event_ids tcp_offered_field{both_interface.fields_[0]};

    vsomeip::method_t const method_ = 0x0001;
    service_instance const si_{udp_svc_.instance_};
    // router_two's UDP unicast endpoint for udp_svc_ (port 30503).
    boost::asio::ip::udp::endpoint const svc_ep_{boardnet::ecu_two_config.unicast_ip_, 30503};
};

TEST_F(test_someip_gate, blocks_notification) {
    // Depict example where a someip_gate installed on the boardnet connection (early
    // loading) blocks the first notification of a field, then releases it.
    ecu_one_.add_app(ecu_one_client_name_);
    ecu_two_.add_app(ecu_two_server_name_);

    ecu_one_.prepare();
    ecu_two_.prepare();

    // Install the gate before the connection forms so the pipe is in place
    // once router_one connects to router_two's boardnet server.
    auto gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(router_one_name_, router_two_name_, socket_role::client, gate->get_data_pipe()));

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one = ecu_one_.router_;
    auto* ecu_two_server_ = ecu_two_.apps_[ecu_two_server_name_];

    std::vector<unsigned char> const payload{0x5, 0x3};

    ecu_two_server_->offer(both_interface);
    ecu_two_server_->send_event(tcp_offered_field, payload);

    // Arm the gate: block the very first matching notification.
    gate->block_at({.service_ = both_interface.instance_.service_,
                    .method_ = tcp_offered_field.event_id_,
                    .type_ = vsomeip::message_type_e::MT_NOTIFICATION});

    router_one->request_service(both_interface.instance_);
    router_one->subscribe_event({tcp_offered_field});

    // The subscription triggers the initial field delivery — the gate must intercept it.
    ASSERT_TRUE(gate->wait_for_blocked());

    message_checker checker{std::nullopt, both_interface.instance_, tcp_offered_field.event_id_, vsomeip::message_type_e::MT_NOTIFICATION,
                            payload};

    // Gate is holding the notification — it must not have arrived yet.
    EXPECT_FALSE(router_one->message_record_.wait_for(checker, std::chrono::milliseconds(500)));

    // Release the gate; the buffered notification is pushed through.
    gate->block(false);
    EXPECT_TRUE(router_one->message_record_.wait_for(checker));
}

TEST_F(test_someip_gate, blocks_notification_matching_payload) {
    // Depict example where a someip_gate with a payload predicate is used to selectively
    // block only notifications whose first payload byte is 0xFF, while letting others pass.
    ecu_one_.add_app(ecu_one_client_name_);
    ecu_two_.add_app(ecu_two_server_name_);

    ecu_one_.prepare();
    ecu_two_.prepare();

    // Install the gate in early loading — no block_at yet, so the gate is transparent.
    auto gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(router_one_name_, router_two_name_, socket_role::client, gate->get_data_pipe()));

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one = ecu_one_.router_;
    auto* ecu_two_server_ = ecu_two_.apps_[ecu_two_server_name_];

    std::vector<unsigned char> const payload_safe{0x5, 0x3};
    std::vector<unsigned char> const payload_blocked{0xFF, 0x3};

    // Let the initial field event (payload_safe) pass through with no active trigger.
    ecu_two_server_->offer(both_interface);
    ecu_two_server_->send_event(tcp_offered_field, payload_safe);
    router_one->request_service(both_interface.instance_);
    router_one->subscribe_event({tcp_offered_field});

    message_checker safe_checker{std::nullopt, both_interface.instance_, tcp_offered_field.event_id_,
                                 vsomeip::message_type_e::MT_NOTIFICATION, payload_safe};
    ASSERT_TRUE(router_one->message_record_.wait_for(safe_checker));

    // Arm the gate with a payload predicate: only block notifications starting with 0xFF.
    gate->block_at({.service_ = both_interface.instance_.service_,
                    .method_ = tcp_offered_field.event_id_,
                    .type_ = vsomeip::message_type_e::MT_NOTIFICATION,
                    .payload_ = [](std::shared_ptr<vsomeip::payload> p) { return p && p->get_length() > 0 && p->get_data()[0] == 0xFF; }});

    ecu_two_server_->send_event(tcp_offered_field, payload_blocked);

    ASSERT_TRUE(gate->wait_for_blocked());

    message_checker blocked_checker{std::nullopt, both_interface.instance_, tcp_offered_field.event_id_,
                                    vsomeip::message_type_e::MT_NOTIFICATION, payload_blocked};

    // Gate is holding the 0xFF notification — it must not have arrived yet.
    EXPECT_FALSE(router_one->message_record_.wait_for(blocked_checker, std::chrono::milliseconds(500)));

    // Release the gate; the buffered notification is pushed through.
    gate->block(false);
    EXPECT_TRUE(router_one->message_record_.wait_for(blocked_checker));
}

TEST_F(test_someip_gate, lets_through_n_notifications) {
    // Verifies the count-based triggering: block_at(trigger, N) lets the first
    // N-1 messages through and blocks on the Nth.
    //
    // 1. Arm gate with count=3 → lets 2 through, blocks on the 3rd.
    // 2. Send 3 notifications with distinct payloads.
    // 3. Client receives the first two.
    // 4. Third is held back until gate is released.

    ecu_one_.add_app(ecu_one_client_name_);
    ecu_two_.add_app(ecu_two_server_name_);
    ecu_one_.prepare();
    ecu_two_.prepare();

    auto gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(router_one_name_, router_two_name_, socket_role::client, gate->get_data_pipe()));

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one = ecu_one_.router_;
    auto* ecu_two_server_ = ecu_two_.apps_[ecu_two_server_name_];

    ecu_two_server_->offer(both_interface);
    router_one->request_service(both_interface.instance_);
    router_one->subscribe_event({tcp_offered_field});
    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::available(both_interface.instance_)));
    ASSERT_TRUE(router_one->subscription_record_.wait_for_any(event_subscription::successfully_subscribed_to(tcp_offered_field)));
    router_one->message_record_.clear();

    // 1. Arm: let first 2 through, block on the 3rd.
    someip_gate::trigger const trigger{.service_ = both_interface.instance_.service_,
                                       .method_ = tcp_offered_field.event_id_,
                                       .type_ = vsomeip::message_type_e::MT_NOTIFICATION};
    gate->block_at(trigger, 3);

    const std::vector<unsigned char> p1{0x11}, p2{0x22}, p3{0x33};
    const auto& ev = tcp_offered_field; // TCP (reliable) field
    const auto& si = both_interface.instance_;

    message_checker c1{std::nullopt, si, ev.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, p1};
    message_checker c2{std::nullopt, si, ev.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, p2};
    message_checker c3{std::nullopt, si, ev.event_id_, vsomeip::message_type_e::MT_NOTIFICATION, p3};

    // 2. Send 3 notifications.
    ecu_two_server_->send_event(ev, p1);
    ecu_two_server_->send_event(ev, p2);
    ecu_two_server_->send_event(ev, p3);

    // Gate must have triggered on the 3rd.
    ASSERT_TRUE(gate->wait_for_blocked());

    // 3. First two must arrive.
    EXPECT_TRUE(router_one->message_record_.wait_for(c1));
    EXPECT_TRUE(router_one->message_record_.wait_for(c2));

    // 4. Third is buffered — must not arrive yet.
    EXPECT_FALSE(router_one->message_record_.wait_for(c3, std::chrono::milliseconds(300)));

    // Release: 3rd notification is forwarded.
    gate->block(false);
    EXPECT_TRUE(router_one->message_record_.wait_for(c3));
}

TEST_F(test_someip_gate, blocks_request_then_response) {
    // Verify that a someip_gate installed on router_two's service socket can first block
    // an incoming REQUEST, and then, once the request is released, block the outgoing
    // RESPONSE before it reaches the client.

    ecu_one_.add_app(ecu_one_client_name_);
    ecu_two_.add_app(ecu_two_server_name_);

    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one = ecu_one_.router_;
    auto* ecu_two_server_ = ecu_two_.apps_[ecu_two_server_name_];

    std::vector<unsigned char> const req_payload{0x01};
    std::vector<unsigned char> const rsp_payload{0x42};

    request const req{si_, method_, vsomeip::message_type_e::MT_REQUEST, false /* UDP */, req_payload};

    // Server offers the service and registers a handler that replies with rsp_payload.
    ecu_two_server_->offer(si_);
    ecu_two_server_->answer_request(req, [rsp_payload] { return rsp_payload; });

    // Client discovers the service via SD.
    router_one->request_service(si_);
    ASSERT_TRUE(router_one->availability_record_.wait_for_last(service_availability::available(si_)));

    // Install both gates after apps have started so that the UDP socket at svc_ep_ is
    // already bound and replace_pipe is called directly (not deferred to pending map).
    auto request_gate = someip_gate::create();
    auto response_gate = someip_gate::create();

    // request_gate on the receiver pipe: blocks REQUESTs arriving at router_two's socket.
    ASSERT_TRUE(setup_data_pipe(svc_ep_, router_two_name_, socket_role::client, request_gate->get_data_pipe()));
    // response_gate on the sender pipe: blocks RESPONSEs leaving router_two's socket.
    ASSERT_TRUE(setup_data_pipe(svc_ep_, router_two_name_, socket_role::server, response_gate->get_data_pipe()));

    // --- Phase 1: block the REQUEST ---
    request_gate->block_at({.service_ = si_.service_, .method_ = method_, .type_ = vsomeip::message_type_e::MT_REQUEST});

    router_one->send_request(req);
    ASSERT_TRUE(request_gate->wait_for_blocked());

    message_checker rsp_checker{std::nullopt, si_, method_, vsomeip::message_type_e::MT_RESPONSE, rsp_payload};
    // Request is held at the gate — no response can have arrived at the client yet.
    EXPECT_FALSE(router_one->message_record_.wait_for(rsp_checker, std::chrono::milliseconds(500)));

    // --- Phase 2: arm the response gate, then release the request ---
    // Arm response_gate before releasing the request to avoid a race where the response
    // is sent before the gate is armed.
    response_gate->block_at({.service_ = si_.service_, .method_ = method_, .type_ = vsomeip::message_type_e::MT_RESPONSE});

    // Release the request: it reaches the server, which replies; the reply is intercepted
    // by response_gate before it leaves router_two.
    request_gate->block(false);
    ASSERT_TRUE(response_gate->wait_for_blocked());

    // Response is still held — client must not have received it.
    EXPECT_FALSE(router_one->message_record_.wait_for(rsp_checker, std::chrono::milliseconds(500)));

    // Release the response gate — the response is now forwarded to the client.
    response_gate->block(false);
    EXPECT_TRUE(router_one->message_record_.wait_for(rsp_checker));
}

ecu_config configure_initial_delay(ecu_config cfg, std::uint32_t min, std::uint32_t max) {
    cfg.service_discovery_.initial_delay_min_ = min;
    cfg.service_discovery_.initial_delay_max_ = max;
    return cfg;
}

const interface service_3344_instance_2{0x3344,
                                        {interface::event_spec{0x8001, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE}},
                                        {interface::event_spec{0x8002, 0x1, vsomeip::reliability_type_e::RT_UNRELIABLE}},
                                        0x2};

ecu_config configure_initial_delay_with_second_instance(ecu_config cfg, std::uint32_t min, std::uint32_t max) {
    cfg = configure_initial_delay(std::move(cfg), min, max);
    cfg.add_interface({service_3344_instance_2}, 30502);
    return cfg;
}

struct increased_initial_delay_with_multiple_instances : public base_fake_socket_fixture {
    ecu_setup ecu_one_{"ecu_one", boardnet::ecu_one_config, *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", configure_initial_delay_with_second_instance(boardnet::ecu_two_config, 10000, 10000), *socket_manager_};
};

// This test verifies whether vSomeIP properly sents StopOffer messages during the initial_phase_wait or not.
// To ensure we never leave the initial_phase, we configure the initial_delay to 10 seconds.
TEST_F(increased_initial_delay_with_multiple_instances, sends_stop_offer_after_find_triggered_offer) {
    ecu_one_.add_guest({"guest_client", 0x1338});
    ecu_two_.add_guest({"guest_server", 0x1337});

    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* client = ecu_one_.apps_["guest_client"];
    auto* server = ecu_two_.apps_["guest_server"];

    // Offer and request the service
    server->offer(interfaces::boardnet::service_3344);
    client->request_service(interfaces::boardnet::service_3344.instance_);
    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::available(interfaces::boardnet::service_3344.instance_),
                                                           std::chrono::seconds(2)));

    // Prepare Service Discovery Gate
    std::shared_ptr<someip_gate> router_one_sd_gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::any(), ecu_one_.sd_endpoint().port()),
                                router_one_name_, socket_role::client, router_one_sd_gate->get_data_pipe()));
    router_one_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 0}, 1);

    // Stop offering the service
    server->stop_offer(interfaces::boardnet::service_3344.instance_);

    // Verify whether a StopService message was blocked or not, if it wasn't, we can safely assume it was not sent either.
    EXPECT_TRUE(router_one_sd_gate->wait_for_blocked(std::chrono::seconds(2)));
}

// This test verifies whether vSomeIP properly sents StopOffer messages during the initial_phase_wait for each service-instance.
TEST_F(increased_initial_delay_with_multiple_instances, sends_stop_offer_for_each_service_instance) {
    ecu_one_.add_guest({"guest_client", 0x1338});
    ecu_two_.add_guest({"guest_server", 0x1337});

    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* client = ecu_one_.apps_["guest_client"];
    auto* server = ecu_two_.apps_["guest_server"];

    const auto first_instance = interfaces::boardnet::service_3344.instance_;
    const auto second_instance = service_3344_instance_2.instance_;

    server->offer(interfaces::boardnet::service_3344);
    server->offer(service_3344_instance_2);
    client->request_service(first_instance);
    client->request_service(second_instance);
    ASSERT_TRUE(client->availability_record_.wait_for_any(service_availability::available(first_instance)));
    ASSERT_TRUE(client->availability_record_.wait_for_any(service_availability::available(second_instance)));

    std::shared_ptr<someip_gate> router_one_sd_gate = someip_gate::create();
    ASSERT_TRUE(setup_data_pipe(boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::any(), ecu_one_.sd_endpoint().port()),
                                router_one_name_, socket_role::client, router_one_sd_gate->get_data_pipe()));

    router_one_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 0}, 1);
    server->stop_offer(first_instance);
    ASSERT_TRUE(router_one_sd_gate->wait_for_blocked(std::chrono::seconds(2)));
    router_one_sd_gate->block(false);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::unavailable(first_instance)));

    router_one_sd_gate->block_at({sd::entry_type_e::OFFER_SERVICE, 0}, 1);
    server->stop_offer(second_instance);
    ASSERT_TRUE(router_one_sd_gate->wait_for_blocked(std::chrono::seconds(2)));
    router_one_sd_gate->block(false);
    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::unavailable(second_instance)));
}

struct sd_header_validation : public base_fake_socket_fixture {
    ecu_setup ecu_one_{"ecu_one", boardnet::ecu_one_config, *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", boardnet::ecu_two_config, *socket_manager_};

    void prepare_ecus_and_apps() {
        ecu_one_.add_guest({"guest_client", 0x1338});
        ecu_two_.add_guest({"guest_server", 0x1337});

        ecu_one_.prepare();
        ecu_two_.prepare();

        ecu_one_.start_apps();
        ecu_two_.start_apps();
    }
};

TEST_F(sd_header_validation, prs_someipsd_00154_sd_offer_with_nonzero_client_id_is_rejected) {
    // SD messages shall have a Client-ID set to 0x0000.
    // Verify that an incoming SD OFFER message carrying a non-zero Client-ID is silently
    // discarded by the receiver (ECU one), while an identical message with Client-ID = 0x0000
    // is accepted and causes normal service availability signalling.

    prepare_ecus_and_apps();

    auto* client = ecu_one_.apps_["guest_client"];
    client->request_service(interfaces::boardnet::service_3344.instance_);

    // construct_offer() builds a well-formed SD OFFER with Client-ID = 0x0000.
    // We then overwrite bytes 8–9 (VSOMEIP_CLIENT_POS_MIN) with a non-zero value to
    // simulate a non-compliant sender.
    auto malformed_offer = construct_offer(interfaces::boardnet::service_3344.events_[0], boardnet::ecu_two_config.unicast_ip_, 30501);
    // SOME/IP header: bytes 8–9 are the Client-ID (big-endian).
    malformed_offer[VSOMEIP_CLIENT_POS_MIN] = 0xDE;
    malformed_offer[VSOMEIP_CLIENT_POS_MIN + 1] = 0xAD;

    send_someip_sd_message(malformed_offer, ecu_two_.sd_endpoint(), ecu_one_.sd_endpoint());

    // ECU one must NOT process the offer; service availability must NOT be reported.
    EXPECT_FALSE(client->availability_record_.wait_for_last(service_availability::available(interfaces::boardnet::service_3344.instance_)))
            << "SD OFFER with non-zero Client-ID (0xDEAD) was incorrectly accepted";
    ;

    // --- Valid offer: Client-ID = 0x0000 ---
    // The identical offer with the correct Client-ID must be accepted and trigger
    // service availability on ECU one.
    auto valid_offer = construct_offer(interfaces::boardnet::service_3344.events_[0], boardnet::ecu_two_config.unicast_ip_, 30501);

    send_someip_sd_message(valid_offer, ecu_two_.sd_endpoint(), ecu_one_.sd_endpoint());

    EXPECT_TRUE(client->availability_record_.wait_for_last(service_availability::available(interfaces::boardnet::service_3344.instance_)))
            << "SD OFFER with Client-ID = 0x0000 was not accepted";
}

struct length_field_too_big : public base_fake_socket_fixture {
    interface interface_tcp_{0x1234, {}, {{0x8002, 0x1, vsomeip::reliability_type_e::RT_RELIABLE}}};
    interface interface_udp_{0x1235};

    method_t method_ = 0x8001;
    request request_tcp_{interface_tcp_.instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message expected_reply_tcp_{
            client_session{0x6310 /*client id*/, 1}, interface_tcp_.instance_, method_, vsomeip::message_type_e::MT_RESPONSE, {}};

    request request_udp_{interface_udp_.instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message expected_reply_udp_{
            client_session{0x6310 /*client id*/, 1}, interface_udp_.instance_, method_, vsomeip::message_type_e::MT_RESPONSE, {}};

    ecu_config ecu_one_config_{boardnet::ecu_one_config};
    ecu_config ecu_two_config_{boardnet::ecu_two_config};

    ecu_setup ecu_one_{"ecu_one", ecu_one_config_, *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", ecu_two_config_.add_interface({interface_tcp_, interface_udp_}), *socket_manager_};

    boost::asio::ip::udp::endpoint src_ep_ = boost::asio::ip::udp::endpoint(ecu_one_.config().unicast_ip_, 30491);
    boost::asio::ip::udp::endpoint dst_ep_ = boost::asio::ip::udp::endpoint(ecu_two_.config().unicast_ip_, 30502);
    boost::asio::ip::udp::endpoint multicast_ep_ =
            boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string("224.244.224.245"), 30490);

    [[nodiscard]] bool successfully_registered(app* _app) {
        return _app && _app->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED);
    }
};

TEST_F(length_field_too_big, length_field_too_big_tcp) {
    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one_ = ecu_one_.router_;
    auto* router_two_ = ecu_two_.router_;

    router_two_->offer(interface_tcp_);
    router_two_->answer_request(request_tcp_, [] { return std::vector<unsigned char>{}; });
    router_one_->request_service(interface_tcp_.instance_);

    auto bad_request_payload = construct_someip_raw_message(static_cast<uint16_t>(interface_tcp_.instance_.service_), // service id
                                                            static_cast<uint16_t>(method_), // method id
                                                            static_cast<uint32_t>(0x04000000), // spoofed length
                                                            static_cast<uint16_t>(router_one_->get_client()), // client id
                                                            static_cast<uint16_t>(0x0001), // session id
                                                            static_cast<uint8_t>(0x01), // proto version
                                                            static_cast<uint8_t>(0xff), // iface version
                                                            static_cast<uint8_t>(0x00), // message type
                                                            static_cast<uint8_t>(0x00) // return code
    );

    ASSERT_TRUE(router_one_->availability_record_.wait_for_last(service_availability::available(interface_tcp_.instance_)));

    inject_message_tcp("router_one", "router_two", bad_request_payload);

    // will go AVAILABLE -> UNAVAILABLE -> AVAILABLE due to connection restart
    ASSERT_TRUE(router_one_->availability_record_.wait_for_last(service_availability::unavailable(interface_tcp_.instance_)));
    ASSERT_TRUE(router_one_->availability_record_.wait_for_last(service_availability::available(interface_tcp_.instance_)));
    ASSERT_FALSE(router_one_->message_record_.wait_for_last(expected_reply_tcp_, std::chrono::milliseconds(500)))
            << router_one_->message_record_.to_string();

    auto good_request_payload = construct_someip_raw_message(static_cast<uint16_t>(interface_tcp_.instance_.service_), // service id
                                                             static_cast<uint16_t>(method_), // method id
                                                             static_cast<uint32_t>(0x00000008), // correct length
                                                             static_cast<uint16_t>(router_one_->get_client()), // client id
                                                             static_cast<uint16_t>(0x0001), // session id
                                                             static_cast<uint8_t>(0x01), // proto version
                                                             static_cast<uint8_t>(0xff), // iface version
                                                             static_cast<uint8_t>(0x00), // message type
                                                             static_cast<uint8_t>(0x00) // return code
    );
    inject_message_tcp("router_one", "router_two", good_request_payload);
    ASSERT_TRUE(router_one_->message_record_.wait_for_last(expected_reply_tcp_)) << router_one_->message_record_.to_string();
}

TEST_F(length_field_too_big, length_field_too_big_udp) {
    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one_ = ecu_one_.router_;
    auto* router_two_ = ecu_two_.router_;

    router_two_->offer(interface_udp_);
    router_two_->answer_request(request_udp_, [] { return std::vector<unsigned char>{}; });
    router_one_->request_service(interface_udp_.instance_);

    auto bad_request_payload = construct_someip_raw_message(static_cast<uint16_t>(interface_udp_.instance_.service_), // service id
                                                            static_cast<uint16_t>(method_), // method id
                                                            static_cast<uint32_t>(0x04000000), // spoofed length
                                                            static_cast<uint16_t>(router_one_->get_client()), // client id
                                                            static_cast<uint16_t>(0x0001), // session id
                                                            static_cast<uint8_t>(0x01), // proto version
                                                            static_cast<uint8_t>(0xff), // iface version
                                                            static_cast<uint8_t>(0x00), // message type
                                                            static_cast<uint8_t>(0x00) // return code
    );

    ASSERT_TRUE(router_one_->availability_record_.wait_for_last(service_availability::available(interface_udp_.instance_)));

    inject_message_udp(src_ep_, dst_ep_, bad_request_payload);

    // no availability should be reported on UDP connections
    ASSERT_FALSE(router_one_->availability_record_.wait_for_last(service_availability::unavailable(interface_udp_.instance_),
                                                                 std::chrono::milliseconds(100)));
    ASSERT_FALSE(router_one_->message_record_.wait_for_last(expected_reply_udp_, std::chrono::milliseconds(500)))
            << router_one_->message_record_.to_string();

    auto good_request_payload = construct_someip_raw_message(static_cast<uint16_t>(interface_udp_.instance_.service_), // service id
                                                             static_cast<uint16_t>(method_), // method id
                                                             static_cast<uint32_t>(0x00000008), // correct length
                                                             static_cast<uint16_t>(router_one_->get_client()), // client id
                                                             static_cast<uint16_t>(0x0001), // session id
                                                             static_cast<uint8_t>(0x01), // proto version
                                                             static_cast<uint8_t>(0xff), // iface version
                                                             static_cast<uint8_t>(0x00), // message type
                                                             static_cast<uint8_t>(0x00) // return code
    );
    inject_message_udp(src_ep_, dst_ep_, good_request_payload);
    ASSERT_TRUE(router_one_->message_record_.wait_for_last(expected_reply_udp_)) << router_one_->message_record_.to_string();
}

TEST_F(length_field_too_big, direct_consume_multicast) {
    ecu_one_.prepare();
    ecu_two_.prepare();

    ecu_one_.start_apps();
    ecu_two_.start_apps();

    auto* router_one_ = ecu_one_.router_;
    auto* router_two_ = ecu_two_.router_;

    ASSERT_TRUE(successfully_registered(router_one_));
    ASSERT_TRUE(successfully_registered(router_two_));

    router_one_->request_service(interface_udp_.instance_);

    ASSERT_TRUE(await_multicast_join(multicast_ep_.address()));
    auto valid_offer = construct_offer(interface_udp_.events_[0], ecu_two_.config().unicast_ip_, 30501);
    inject_message_udp_multicast(ecu_two_.sd_endpoint(), multicast_ep_, valid_offer);
    ASSERT_TRUE(router_one_->availability_record_.wait_for_last(service_availability::available(interface_udp_.instance_)));
}
}
