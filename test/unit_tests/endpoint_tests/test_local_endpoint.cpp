// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "base_endpoint_fixture.hpp"
#include "mock_routing_host.hpp"
#include "mock_endpoint_host.hpp"

#include "../../../implementation/endpoints/include/asio_timer.hpp"
#include "../../../implementation/endpoints/include/asio_tcp_socket.hpp"
#include "../../../implementation/endpoints/include/local_endpoint.hpp"
#include "../../../implementation/endpoints/include/local_server.hpp"
#include "../../../implementation/endpoints/include/local_acceptor_uds_impl.hpp"
#include "../../../implementation/endpoints/include/local_socket_uds_impl.hpp"
#include "../../../implementation/configuration/include/configuration_impl.hpp"
#include "../../../implementation/protocol/include/protocol.hpp"
#include "../../../implementation/protocol/include/config_command.hpp"
#include "../../../implementation/protocol/include/offer_service_command.hpp"
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/system/error_code.hpp>

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
    virtual std::unique_ptr<abstract_timer> create_timer(boost::asio::io_context& _io) override {
        return std::make_unique<asio_timer>(_io);
    }
};

struct test_uds_local_endpoint : base_endpoint_fixture {
    test_uds_local_endpoint() : factory_(std::make_shared<stub_factory>()) {
        delegate_->impl_ = factory_;

        static constexpr char const* path = "uds_local_endpoint_config.json";
        configuration_ = std::make_shared<vsomeip_v3::cfg::configuration_impl>(path);
        configuration_->set_configuration_path(path);
        configuration_->load("stub");
    }

    auto create_server() {
        auto acceptor = std::make_shared<local_acceptor_uds_impl>(io_, server_endpoint_, configuration_);
        boost::system::error_code ec;
        acceptor->init(ec, std::nullopt);
        if (ec) {
            throw std::logic_error("server could not be set-up");
        }
        return std::make_shared<local_server>(io_, std::move(acceptor), configuration_, server_routing_host_, server_endpoint_host_, false);
    }
    auto create_client_ep() {
        return local_endpoint::create_client_ep(
                local_endpoint_context{io_, configuration_, client_routing_host_, client_endpoint_host_},
                local_endpoint_params{false, server_,
                                      std::make_unique<local_socket_uds_impl>(io_, boost::asio::local::stream_protocol::endpoint{},
                                                                              server_endpoint_, socket_role_e::SENDER)});
    }
    auto create_client_config_command() {
        std::vector<byte_t> msg;
        protocol::config_command command;
        command.set_client(client_);
        command.insert("hostname", "");

        protocol::error_e its_error;
        command.serialize(msg, its_error);
        if (its_error != protocol::error_e::ERROR_OK) {
            throw std::logic_error("config command could not be created");
        }

        return msg;
    }
    void add_offer_service_command(std::vector<std::vector<byte_t>>& _queue) {
        std::vector<byte_t> msg;
        protocol::offer_service_command command;
        command.set_service(2222);
        command.set_instance(1);
        command.set_major(1);
        command.set_minor(0);
        protocol::error_e its_error;
        command.serialize(msg, its_error);
        if (its_error != protocol::error_e::ERROR_OK) {
            throw std::logic_error("service offer could not be serialized");
        }
        _queue.push_back(std::move(msg));
    }
    std::shared_ptr<stub_factory> factory_;

    boost::asio::io_context io_;
    client_t client_{2222};
    client_t server_{3333};
    boost::asio::local::stream_protocol::endpoint server_endpoint_{"/tmp/vsomeip-3333"};
    std::shared_ptr<mock_routing_host> server_routing_host_{std::make_shared<mock_routing_host>()};
    std::shared_ptr<mock_routing_host> client_routing_host_{std::make_shared<mock_routing_host>()};
    std::shared_ptr<mock_endpoint_host> server_endpoint_host_{std::make_shared<mock_endpoint_host>()};
    std::shared_ptr<mock_endpoint_host> client_endpoint_host_{std::make_shared<mock_endpoint_host>()};
    std::shared_ptr<configuration> configuration_;
};

TEST_F(test_uds_local_endpoint, a_local_endpoint_can_connect_to_the_local_server) {
    EXPECT_CALL(*client_endpoint_host_, on_connect);

    auto server = create_server();
    auto client = create_client_ep();
    server->start();
    client->start();

    io_.poll();
}

TEST_F(test_uds_local_endpoint, config_command_leads_to_information_forwarding_to_routing) {
    EXPECT_CALL(*client_endpoint_host_, on_connect);

    auto server = create_server();
    auto client = create_client_ep();
    server->start();
    client->start();

    io_.poll();

    EXPECT_CALL(*server_routing_host_, on_message).Times(0); // the config command shall not be forwarded
    EXPECT_CALL(*server_routing_host_, add_known_client(client_, ::testing::_));
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
}
