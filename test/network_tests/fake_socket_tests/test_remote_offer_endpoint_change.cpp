// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sample_configurations.hpp"

#include "helpers/app.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/ecu_setup.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/service_state.hpp"
#include "helpers/someip_gate.hpp"

#include "internal.hpp"
#include "../../../implementation/message/include/serializer.hpp"
#include "../../../implementation/service_discovery/include/ipv4_option_impl.hpp"
#include "../../../implementation/service_discovery/include/message_impl.hpp"
#include "../../../implementation/service_discovery/include/serviceentry_impl.hpp"
#include "sample_interfaces.hpp"

#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace vsomeip_v3::testing {

// Test changed offer
//
// This test suite covers how a client host reacts when it receives
// an offer for a service instance that it had already received
// an offer before.
struct remote_offer_endpoint_change : public base_fake_socket_fixture {
    // service to be offered
    service_instance const service_{interfaces::boardnet::service_3344.instance_};
    event_ids const offered_field{service_, interfaces::boardnet::service_3344.fields_[0]};

    // ports used in the offers
    static constexpr vsomeip::port_t udp_a_ = 30501;
    static constexpr vsomeip::port_t udp_b_ = 30601;
    static constexpr vsomeip::port_t tcp_a_ = 30501;

    // methods used to check which endpoints the client still sends on. Each method uses its own
    // method id, so that the requests can be told apart on a message record or on a someip gate.
    static constexpr vsomeip::method_t method_a = 0x1111;
    static constexpr vsomeip::method_t method_b = 0x2222;

    // client ecu configuration
    ecu_config client_cfg_{boardnet::ecu_one_config};
    ecu_setup client_ecu_{"ecu_one", client_cfg_, *socket_manager_};

    // first provider ecu
    std::optional<ecu_setup> provider_;
    // second provider ecu
    std::optional<ecu_setup> second_provider_;

    // sd gate of the provider ecu
    std::shared_ptr<someip_gate> provider_sd_gate_;
    // client request gate; needed to know when the client sent a request
    std::shared_ptr<someip_gate> client_request_gate_;

    // attribute recorder to check if the first provider got a subscription from the client
    attribute_recorder<bool> provider_subscriptions;

    // attribute recorder to check if the second provider got a subscription from the client
    attribute_recorder<bool> second_provider_subscriptions;

    // Build an offering ecu_config from _base offering service_ with the given UDP/TCP ports (0 == absent).
    static ecu_config make_offering_cfg(ecu_config _base, vsomeip::port_t _udp, vsomeip::port_t _tcp) {
        _base.services_.clear();
        service_config svc;
        svc.si_ = interfaces::boardnet::service_3344.instance_;
        if (_udp != 0) {
            svc.unreliable_port_ = _udp;
        }
        if (_tcp != 0) {
            svc.reliable_port_ = reliable_port_config{_tcp, false};
        }
        svc.events_.push_back(event_config{0x8001, false, false});
        svc.event_groups_.push_back(eventgroup_config{0x1, {0x8001}});
        _base.services_.push_back(std::move(svc));
        return _base;
    }

    // Offer endpoint definition to create fake offers
    struct offer_endpoint {
        vsomeip::port_t port_{};
        bool reliable_{false};
    };

    // Build fake offers with custom address, service, endpoints and session id
    static std::vector<unsigned char> make_offer(boost::asio::ip::address _address, service_instance _service,
                                                 std::vector<offer_endpoint> _endpoints, vsomeip::session_t _session = 0x0001) {
        auto entry = std::make_shared<vsomeip_v3::sd::serviceentry_impl>();
        entry->set_type(vsomeip_v3::sd::entry_type_e::OFFER_SERVICE);
        entry->set_service(_service.service_);
        entry->set_instance(_service.instance_);
        entry->set_major_version(_service.major_);
        entry->set_minor_version(_service.minor_);
        entry->set_ttl(0x03);

        std::vector<std::shared_ptr<vsomeip_v3::sd::option_impl>> options;
        options.reserve(_endpoints.size());
        for (auto const& endpoint : _endpoints) {
            options.push_back(std::make_shared<vsomeip_v3::sd::ipv4_option_impl>(_address, endpoint.port_, endpoint.reliable_));
        }

        auto message = std::make_shared<vsomeip_v3::sd::message_impl>();
        message->set_session(_session);
        message->add_entry_data(entry, options);

        vsomeip_v3::serializer serializer{VSOMEIP_DEFAULT_BUFFER_SHRINK_THRESHOLD};
        serializer.serialize(message.get());
        return std::vector<unsigned char>(serializer.get_data(), serializer.get_data() + serializer.get_size());
    }

    // Start the provider ecu and offer the service
    app* start_provider(vsomeip::port_t _udp, vsomeip::port_t _tcp) {
        provider_.emplace(
                "ecu_two",
                make_offering_cfg(ecu_config{boardnet::ecu_two_config}.add_interface({interfaces::boardnet::service_3344}), _udp, _tcp),
                *socket_manager_);
        provider_->prepare();
        provider_->start_router();

        provider_->router_->register_group_subscription_handler(
                offered_field, [this](vsomeip::client_t, vsomeip::uid_t, vsomeip::gid_t, std::string const&, bool _subscribed) {
                    if (_subscribed) {
                        provider_subscriptions.record(true);
                    }
                    return true;
                });

        provider_->router_->offer(interfaces::boardnet::service_3344);
        return provider_->router_;
    }

    // Start the second provider ecu and offer the service
    app* start_second_provider(vsomeip::port_t _udp, vsomeip::port_t _tcp) {
        second_provider_.emplace("ecu_three", make_offering_cfg(boardnet::ecu_three_config, _udp, _tcp), *socket_manager_);
        second_provider_->prepare();
        second_provider_->start_router();

        second_provider_->router_->register_group_subscription_handler(
                offered_field, [this](vsomeip::client_t, vsomeip::uid_t, vsomeip::gid_t, std::string const&, bool _subscribed) {
                    if (_subscribed) {
                        second_provider_subscriptions.record(true);
                    }
                    return true;
                });

        second_provider_->router_->offer(interfaces::boardnet::service_3344);
        return second_provider_->router_;
    }

    // block the provider sd from sending any sd message
    void block_provider_sd() {
        provider_sd_gate_ = someip_gate::create();
        ASSERT_TRUE(set_udp_data_pipe({provider_->sd_endpoint(), provider_->router_name_}, udp_party::any(),
                                      provider_sd_gate_->get_data_pipe()));
        provider_sd_gate_->block(true);
    }

    // Start the client ecu and request the service
    app* start_client_and_request() {
        client_ecu_.prepare();
        client_ecu_.start_router();
        auto* client = client_ecu_.router_;
        // Watch the client's outgoing SD BEFORE requesting, so its FIND_SERVICE is captured (see wait_for_client_request).
        client_request_gate_ = someip_gate::create();
        EXPECT_TRUE(set_udp_data_pipe({client_ecu_.sd_endpoint(), client_ecu_.router_name_}, udp_party::any(),
                                      client_request_gate_->get_data_pipe()));
        client->request_service(service_);
        return client;
    }

    // Wait until the client sends a request
    [[nodiscard]] bool wait_for_client_request(std::chrono::milliseconds _timeout = std::chrono::seconds(2)) {
        return client_request_gate_
                && client_request_gate_->sd_record_.wait_for_any({sd::entry_type_e::FIND_SERVICE, vsomeip::DEFAULT_TTL}, _timeout);
    }

    // Client sends a request for the _method with the reliability
    void send_request(app* _client, vsomeip::method_t _method, bool _reliable) {
        _client->send_request(request{service_, _method, vsomeip::message_type_e::MT_REQUEST_NO_RETURN, _reliable, {}});
    }

    // Check if the provider received the request sent by the client
    [[nodiscard]] message_checker received_request(vsomeip::method_t _method) const {
        return message_checker{std::nullopt, service_, _method, vsomeip::message_type_e::MT_REQUEST_NO_RETURN, std::nullopt};
    }
};

//  Test 1. - Second offer adds an endpoint that did not exist before
//
//  Test steps:
//  1. Start the client and wait for the request to be sent
//  2. Send a fake offer to the client with just an udp endpoint,
//     expect the service to became available
//  3. Start the provider ECU without sending an sd offer to the client
//     (if provider ecu sent the offer, client would detect a reboot
//     and mark the service and unavailable)
//     The provider ECU offers the service in both udp and tcp endpoints
//  4. Send another fake offer, this time with udp and tcp endpoints
//  5. Expect the service to never become unavailable after adding the endpoint
//     and check that service provider receives both udp and tcp requests

TEST_F(remote_offer_endpoint_change, second_offer_adds_endpoint_) {
    // 1.
    auto* client = start_client_and_request();
    ASSERT_TRUE(wait_for_client_request()) << "Client did not send the service request";

    // 2.
    auto const provider_ip = boardnet::ecu_two_config.unicast_ip_;
    auto first_offer = make_offer(provider_ip, service_, {{udp_a_, false}}, 0x0001);
    send_someip_sd_message(first_offer, boost::asio::ip::udp::endpoint{provider_ip, 30490}, client_ecu_.sd_endpoint());

    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::available(service_)))
            << "Client did not detect the service as available after sending the first fake offer";
    client->availability_record_.clear();

    // 3.
    provider_.emplace(
            "ecu_two",
            make_offering_cfg(ecu_config{boardnet::ecu_two_config}.add_interface({interfaces::boardnet::service_3344}), udp_a_, tcp_a_),
            *socket_manager_);
    provider_->prepare();
    provider_->start_router();
    block_provider_sd();
    provider_->router_->offer(interfaces::boardnet::service_3344);
    ASSERT_TRUE(socket_manager_->await_acceptor(boost::asio::ip::tcp::endpoint{provider_ip, tcp_a_}))
            << "Provider tcp server endpoint did not start listening";

    // 4.
    auto second_offer = make_offer(provider_ip, service_, {{udp_a_, false}, {tcp_a_, true}}, 0x0002);
    send_someip_sd_message(second_offer, boost::asio::ip::udp::endpoint{provider_ip, 30490}, client_ecu_.sd_endpoint());

    // 5.
    EXPECT_FALSE(client->availability_record_.wait_for_any(service_availability::unavailable(service_), std::chrono::milliseconds(500)))
            << "Client received service unavailability after offering ECU added an endpoint to the offer";

    ASSERT_TRUE(await_connection(client_ecu_.router_name_, provider_->router_name_))
            << "Client did not connect to the tcp endpoint added by the second offer";

    send_request(client, method_a, false);
    EXPECT_TRUE(provider_->router_->message_record_.wait_for_any(received_request(method_a)))
            << "Provider did not receive an unreliable request on the udp endpoint kept by the second offer.\nRecord: "
            << provider_->router_->message_record_.to_string();

    send_request(client, method_b, true);
    EXPECT_TRUE(provider_->router_->message_record_.wait_for_any(received_request(method_b)))
            << "Provider did not receive a reliable request on the tcp endpoint added by the second offer.\nRecord: "
            << provider_->router_->message_record_.to_string();
}

// Test 2. - A second ECU offers the same service instance
//
//  Test steps:
//  1. Start the client and wait for the request to be sent
//  2. Start the provider ecu and offer the service and a field event
//  3. The client subscribes to the field event and the provider checks
//     that it received the subscription
//  4. The second provider offers the same service and field event
//  5. Expect that the second provider, up until 1 second, did not get a subscription

TEST_F(remote_offer_endpoint_change, second_ECU_offers_same_service) {
    // 1.
    auto* client = start_client_and_request();
    ASSERT_TRUE(wait_for_client_request()) << "Client did not send the service request";

    // 2.
    start_provider(udp_a_, 0);

    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::available(service_)))
            << "Client did not detect the service as available";

    // 3.
    client->subscribe_field(offered_field);
    ASSERT_TRUE(client->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_field)))
            << "Client did not get its field subscription acknowledged";
    ASSERT_TRUE(provider_subscriptions.wait_for_any(true)) << "First provider did not receive the client's subscription";

    // 4.
    start_second_provider(udp_a_, 0);

    // 5.
    EXPECT_FALSE(second_provider_subscriptions.wait_for_any(true, std::chrono::seconds(1)))
            << "Second provider received a subscription but it should not have";
}

// Test 3. - Second offer removes an endpoint that existed before
//
//  Test steps:
//  1. Start the client and wait for the request to be sent
//  2. Start the provider ECU by offering the service via tcp and udp,
//     expect the service to became available
//  3. Stop the sending of SD messages from the service provider
//  4. Send a fake offer to the client with just the tcp endpoint
//  5. Expect the service to not became unavailable (before the offer TTL expires)
//     and the service provider still receives tcp requests

TEST_F(remote_offer_endpoint_change, second_offer_removes_endpoint_) {
    // 1.
    auto* client = start_client_and_request();
    ASSERT_TRUE(wait_for_client_request()) << "Client did not send the service request";

    // 2.
    start_provider(udp_a_, tcp_a_);

    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::available(service_)))
            << "Client did not detect the service as available after the provider offered it via tcp and udp";
    client->availability_record_.clear();

    // 3.
    block_provider_sd();

    // 4.
    auto const provider_ip = boardnet::ecu_two_config.unicast_ip_;
    auto second_offer = make_offer(provider_ip, service_, {{tcp_a_, true}}, 0x0002);
    send_someip_sd_message(second_offer, boost::asio::ip::udp::endpoint{provider_ip, 30490}, client_ecu_.sd_endpoint());

    // 5.
    EXPECT_FALSE(client->availability_record_.wait_for_any(service_availability::unavailable(service_), std::chrono::milliseconds(500)))
            << "Client received service unavailability after offering ECU removed the udp endpoint from the offer";

    send_request(client, method_a, true);
    EXPECT_TRUE(provider_->router_->message_record_.wait_for_any(received_request(method_a)))
            << "Provider did not receive a reliable request on the remaining tcp endpoint after udp was removed.\nRecord: "
            << provider_->router_->message_record_.to_string();
}

// Test 4. - Second offer changes the udp endpoint port
//
//  Test steps:
//  1. Start the client and wait for the request to be sent
//  2. Start the provider ECU by offering the service via udp,
//     expect the service to became available
//  3. Stop the sending of SD messages from the service provider
//  4. Send a fake offer to the client with a new udp endpoint on a different port
//  5. Expect the service to first became unavailable and then available
//     and that the client does not send requests to the old port

TEST_F(remote_offer_endpoint_change, second_offer_changes_endpoint_port) {
    GTEST_SKIP() << "Fix with adjusted configuration of service disocvery";
    // 1.
    auto* client = start_client_and_request();
    ASSERT_TRUE(wait_for_client_request()) << "Client did not send the service request";

    // 2.
    start_provider(udp_a_, 0);

    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::available(service_)))
            << "Client did not detect the service as available after the provider offered it via udp";
    client->availability_record_.clear();

    // 3.
    block_provider_sd();

    // 4.
    auto const provider_ip = boardnet::ecu_two_config.unicast_ip_;
    auto second_offer = make_offer(provider_ip, service_, {{udp_b_, false}}, 0x0002);
    send_someip_sd_message(second_offer, boost::asio::ip::udp::endpoint{provider_ip, 30490}, client_ecu_.sd_endpoint());

    // 5.
    ASSERT_TRUE(client->availability_record_.wait_for_any(service_availability::unavailable(service_)))
            << "Client did not detect the service as unavailable after the offering ECU changed the udp endpoint port";
    ASSERT_TRUE(client->availability_record_.wait_for_last(service_availability::available(service_)))
            << "Client did not detect the service as available again after the offering ECU changed the udp endpoint port";

    send_request(client, method_b, false);
    EXPECT_FALSE(provider_->router_->message_record_.wait_for_any(received_request(method_b), std::chrono::milliseconds(500)))
            << "Client is still sending to the old udp endpoint port after the offer changed it to udp_b_";
}
} // namespace vsomeip_v3::testing
