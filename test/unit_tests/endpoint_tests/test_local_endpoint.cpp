// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "base_endpoint_fixture.hpp"
#include "mock_routing_host.hpp"

#include "../../../implementation/security/include/policy_manager_impl.hpp"
#include "../../../implementation/security/include/security.hpp"

#include "../../../implementation/endpoints/include/asio_timer.hpp"
#include "../../../implementation/endpoints/include/steady_clock.hpp"
#include "../../../implementation/endpoints/include/asio_tcp_socket.hpp"
#include "../../../implementation/endpoints/include/asio_udp_socket.hpp"
#include "../../../implementation/endpoints/include/asio_uds_acceptor.hpp"
#include "../../../implementation/endpoints/include/asio_uds_socket.hpp"
#include "../../../implementation/endpoints/include/local_endpoint.hpp"
#include "../../../implementation/endpoints/include/local_server.hpp"
#include "../../../implementation/endpoints/include/local_acceptor_uds_impl.hpp"
#include "../../../implementation/endpoints/include/local_socket_uds_impl.hpp"
#include "../../../implementation/configuration/include/configuration_impl.hpp"
#include "../../../implementation/protocol/include/protocol.hpp"
#include "../../../implementation/protocol/include/command_types.hpp"
#include "../../../implementation/protocol/include/serialize.hpp"
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/system/error_code.hpp>

#include <unistd.h>

namespace vsomeip_v3::testing {

class stub_factory : public abstract_socket_factory {
public:
    std::shared_ptr<abstract_netlink_connector> create_netlink_connector(boost::asio::io_context&, const boost::asio::ip::address&,
                                                                         const boost::asio::ip::address&, bool) override {
        return nullptr;
    }

    virtual std::unique_ptr<tcp_socket> create_tcp_socket(boost::asio::io_context& _io) override {
        return std::make_unique<asio_tcp_socket>(_io);
    }
    virtual std::unique_ptr<tcp_acceptor> create_tcp_acceptor(boost::asio::io_context& _io) override {
        return std::make_unique<asio_tcp_acceptor>(_io);
    }
    virtual std::unique_ptr<udp_socket> create_udp_socket(boost::asio::io_context& _io) override {
        return std::make_unique<asio_udp_socket>(_io);
    }
    virtual std::unique_ptr<abstract_timer> create_timer(boost::asio::io_context& _io) override {
        return std::make_unique<asio_timer>(_io);
    }
    std::shared_ptr<abstract_clock> get_clock() override {
        static auto clock = std::make_shared<steady_clock>();
        return clock;
    }
#if defined(__linux__) || defined(__QNX__)
    std::unique_ptr<uds_socket> create_uds_socket(boost::asio::io_context& _io) override { return std::make_unique<asio_uds_socket>(_io); }
    std::unique_ptr<uds_acceptor> create_uds_acceptor(boost::asio::io_context& _io) override {
        return std::make_unique<asio_uds_acceptor>(_io);
    }
#endif
};

struct test_uds_local_endpoint : base_endpoint_fixture {
    test_uds_local_endpoint() : factory_(std::make_shared<stub_factory>()) {
        delegate_->impl_ = factory_;

        static constexpr char const* path = "uds_local_endpoint_config.json";
        configuration_ = std::make_shared<vsomeip_v3::cfg::configuration_impl>(path);
        configuration_->load("stub");

        ON_CALL(*server_routing_host_, get_policy_manager()).WillByDefault(::testing::Return(policy_manager_));
        ON_CALL(*server_routing_host_, get_security()).WillByDefault(::testing::Return(security_));
        ON_CALL(*client_routing_host_, get_policy_manager()).WillByDefault(::testing::Return(policy_manager_));
        ON_CALL(*client_routing_host_, get_security()).WillByDefault(::testing::Return(security_));
    }

    auto create_server() {
        auto acceptor = std::make_shared<local_acceptor_uds_impl>(io_, server_endpoint_, configuration_);
        boost::system::error_code ec;
        acceptor->init(ec, std::nullopt);
        if (ec) {
            throw std::logic_error("server could not be set-up");
        }
        return std::make_shared<local_server>(io_, std::move(acceptor), configuration_, server_routing_host_, false, "host-env");
    }
    auto create_client_ep(std::optional<vsomeip_sec_client_t> _expected_sec_client = {}) {
        return local_endpoint::create_client_ep(
                local_endpoint_context{io_, configuration_, client_routing_host_},
                local_endpoint_params{server_,
                                      client_,
                                      "",
                                      std::make_unique<local_socket_uds_impl>(io_, boost::asio::local::stream_protocol::endpoint{},
                                                                              server_endpoint_, socket_role_e::CLIENT),
                                      {},
                                      ILLEGAL_PORT,
                                      _expected_sec_client});
    }
    auto create_client_config_command() {
        std::vector<byte_t> msg;
        auto serialize_cmd = [&](protocol::config_command_data const& _cmd) {
            msg.resize(protocol::wire_size(_cmd));
            protocol::serialize(_cmd, msg.data());
        };
        serialize_cmd(protocol::create_config_cmd(client_, {{"hostname", ""}}));
        return msg;
    }
    void add_offer_service_command(std::vector<std::vector<byte_t>>& _queue) {
        auto cmd =
                protocol::create_offer_service_cmd(client_, {.key_ = {.service_ = 2222, .instance_ = 1, .major_ = 1}, .minor_version_ = 0});
        std::vector<byte_t> msg(protocol::wire_size(cmd));
        protocol::serialize(cmd, msg.data());
        _queue.push_back(std::move(msg));
    }
    std::shared_ptr<policy_manager_impl> policy_manager_{std::make_shared<policy_manager_impl>()};
    std::shared_ptr<security> security_{std::make_shared<security>(policy_manager_)};

    std::shared_ptr<stub_factory> factory_;

    boost::asio::io_context io_;
    client_t client_{2222};
    client_t server_{3333};
    boost::asio::local::stream_protocol::endpoint server_endpoint_{"/tmp/vsomeip-3333"};
    std::shared_ptr<mock_routing_host> server_routing_host_{std::make_shared<mock_routing_host>()};
    std::shared_ptr<mock_routing_host> client_routing_host_{std::make_shared<mock_routing_host>()};
    std::shared_ptr<configuration> configuration_;
    std::vector<std::shared_ptr<local_endpoint>> server_eps_;
};

TEST_F(test_uds_local_endpoint, a_local_endpoint_can_connect_to_the_local_server) {

    auto server = create_server();
    auto client = create_client_ep();
    server->start([this](auto const& ep) {
        server_eps_.push_back(ep);
        ep->start();
    });
    client->start();

    io_.poll();
}

TEST_F(test_uds_local_endpoint, config_command_leads_to_information_forwarding_to_routing) {

    auto server = create_server();
    auto client = create_client_ep();
    server->start([this](auto const& ep) {
        server_eps_.push_back(ep);
        ep->start();
    });
    client->start();

    io_.poll();

    EXPECT_CALL(*server_routing_host_, on_message).Times(0); // the config command shall not be forwarded
    EXPECT_CALL(*server_routing_host_, lazy_load(::testing::_));
    auto config_msg = create_client_config_command();
    client->send(&config_msg[0], static_cast<uint32_t>(config_msg.size()));
    io_.poll();

    // check buffered message forwarding
    std::vector<std::vector<byte_t>> received_messages;
    ON_CALL(*server_routing_host_, on_message).WillByDefault([&](auto ptr, auto size, auto...) {
        std::vector<byte_t> msg;
        msg.insert(msg.end(), ptr, ptr + size);
        received_messages.push_back(std::move(msg));
    });
    EXPECT_CALL(*server_routing_host_, on_message).Times(3);
    std::vector<std::vector<byte_t>> send_messages;
    add_offer_service_command(send_messages);
    add_offer_service_command(send_messages);
    add_offer_service_command(send_messages);
    for (auto const& msg : send_messages) {
        client->send(&msg[0], static_cast<uint32_t>(msg.size()));
    }
    io_.poll();

    EXPECT_EQ(received_messages, send_messages);
}

// The identity reported for the peer (passed in when the consumer endpoint is created) is checked
// against the one actually read off the socket (SO_PEERCRED) once connected. On a mismatch -- e.g.
// the client id was re-used by another process -- the connection must not become usable.
TEST_F(test_uds_local_endpoint, connection_is_escalated_when_the_reported_peer_identity_does_not_match) {
    // The peer is the test process itself, so its real identity is the real uid/gid; report a
    // different one for it.
    vsomeip_sec_client_t const its_wrong_identity{.user = getuid() + 1, .group = getgid() + 1, .host = 0, .port = VSOMEIP_SEC_PORT_UNUSED};

    auto server = create_server();
    auto client = create_client_ep(its_wrong_identity);

    bool cleanup_called{false};
    bool cleanup_due_to_error{false};
    client->register_cleanup_handler([&](bool _due_to_error) {
        cleanup_called = true;
        cleanup_due_to_error = _due_to_error;
    });

    server->start([this](auto const& ep) {
        server_eps_.push_back(ep);
        ep->start();
    });
    client->start();
    io_.poll();

    EXPECT_TRUE(cleanup_called);
    EXPECT_TRUE(cleanup_due_to_error);
}

TEST_F(test_uds_local_endpoint, connection_stays_up_when_the_reported_peer_identity_matches) {
    vsomeip_sec_client_t const its_real_identity{.user = getuid(), .group = getgid(), .host = 0, .port = VSOMEIP_SEC_PORT_UNUSED};

    auto server = create_server();
    auto client = create_client_ep(its_real_identity);

    bool cleanup_called{false};
    client->register_cleanup_handler([&](bool) { cleanup_called = true; });

    server->start([this](auto const& ep) {
        server_eps_.push_back(ep);
        ep->start();
    });
    client->start();
    io_.poll();

    EXPECT_FALSE(cleanup_called);

    EXPECT_CALL(*server_routing_host_, lazy_load(::testing::_));
    auto config_msg = create_client_config_command();
    client->send(&config_msg[0], static_cast<uint32_t>(config_msg.size()));
    io_.poll();
}
}
