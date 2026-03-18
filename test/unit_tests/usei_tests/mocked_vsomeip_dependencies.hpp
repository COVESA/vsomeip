// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <gmock/gmock.h>

#include <string>

#include <vsomeip/primitive_types.hpp>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif

#define private public
#define protected public
#include "../../../implementation/endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../../implementation/endpoints/include/abstract_socket_factory.hpp"
#include "../../../implementation/endpoints/include/asio_udp_socket.hpp"
#include "../../../implementation/endpoints/include/boardnet_endpoint_host.hpp"
#include "../../../implementation/routing/include/routing_host.hpp"

template<typename Protocol>
vsomeip_v3::endpoint_impl<Protocol>::endpoint_impl(const std::shared_ptr<boardnet_endpoint_host>& _endpoint_host,
                                                   const std::shared_ptr<boardnet_routing_host>& _routing_host,
                                                   boost::asio::io_context& _io, const std::shared_ptr<configuration>& _configuration) :
    io_(_io), endpoint_host_(_endpoint_host), routing_host_(_routing_host), sending_blocked_(false), configuration_(_configuration),
    is_supporting_someip_tp_(false) { }

template<typename Protocol>
uint32_t vsomeip_v3::endpoint_impl<Protocol>::find_magic_cookie(byte_t* /*_buffer*/, size_t /*_size*/) {
    return 0xFFFFFFFF;
}

template<typename Protocol>
void vsomeip_v3::endpoint_impl<Protocol>::add_default_target(service_t, const std::string&, uint16_t) { }

template<typename Protocol>
void vsomeip_v3::endpoint_impl<Protocol>::remove_default_target(service_t) { }

template<typename Protocol>
vsomeip_v3::instance_t vsomeip_v3::endpoint_impl<Protocol>::get_instance(service_t /*_service*/) {
    return 0xFFFF;
}

template<typename Protocol>
vsomeip_v3::server_endpoint_impl<Protocol>::server_endpoint_impl(const std::shared_ptr<boardnet_endpoint_host>& _endpoint_host,
                                                                 const std::shared_ptr<boardnet_routing_host>& _routing_host,
                                                                 boost::asio::io_context& _io,
                                                                 const std::shared_ptr<configuration>& _configuration) :
    endpoint_impl<Protocol>(_endpoint_host, _routing_host, _io, _configuration) { }

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::stop(bool /*_due_to_error*/) { }

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::is_client() const {
    return false;
}

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::restart(bool /*_force*/) {
    boost::system::error_code its_error;
    this->stop(false);
    this->init(server_endpoint_impl<Protocol>::local_, its_error);
    this->start();
}

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::is_established() const {
    return true;
}

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::is_established_or_connected() const {
    return true;
}

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::set_established(bool /*_established*/) { }

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::set_connected(bool /*_connected*/) { }

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::send(const uint8_t* /*_data*/, uint32_t /*_size*/) {
    return true;
}
template<typename Protocol>
typename vsomeip_v3::server_endpoint_impl<Protocol>::clients_key_t
vsomeip_v3::server_endpoint_impl<Protocol>::to_clients_key(service_t its_service, method_t its_method, client_t its_client) {
    return (static_cast<clients_key_t>(its_service) << 48) | (static_cast<clients_key_t>(its_method) << 32)
            | (static_cast<clients_key_t>(its_client) << 16);
}

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::send(const std::vector<byte_t>& /*_cmd_header*/, const byte_t* /*_data*/,
                                                      uint32_t /*_size*/) {
    return false;
}

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::send_intern(endpoint_type /*_target*/, const byte_t* /*_data*/, uint32_t /*_size*/) {
    return true;
}

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::set_client_target(const clients_key_t /*_client*/, const endpoint_type& /*target*/) { }

template<typename Protocol>
std::optional<typename Protocol::endpoint> vsomeip_v3::server_endpoint_impl<Protocol>::get_client_target(const clients_key_t /*_client*/) {
    return std::nullopt;
}

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::tp_segmentation_enabled(service_t /*_service*/, instance_t /*_instance*/,
                                                                         method_t /*_method*/) const {
    return false;
}

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::send_segments(const tp::tp_split_messages_t& /*_segments*/,
                                                               std::uint32_t /*_separation_time*/, const endpoint_type& /*_target*/) { }

template<typename Protocol>
typename vsomeip_v3::server_endpoint_impl<Protocol>::target_data_iterator_type
vsomeip_v3::server_endpoint_impl<Protocol>::find_or_create_target_unlocked(endpoint_type /*_target*/) {
    return targets_.end();
}

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::schedule_train(endpoint_data_type& /*_data*/) { }

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::check_message_size(std::uint32_t /*_size*/) const {
    return true;
}

template<typename Protocol>
typename vsomeip_v3::endpoint_impl<Protocol>::cms_ret_e
vsomeip_v3::server_endpoint_impl<Protocol>::segment_message(const std::uint8_t* const /*_data*/, std::uint32_t /*_size*/,
                                                            const endpoint_type& /*_target*/) {
    return endpoint_impl<Protocol>::cms_ret_e::MSG_WAS_SPLIT;
}

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::recalculate_queue_size(endpoint_data_type& /*_data*/) const { }

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::check_queue_limit(const uint8_t* /*_data*/, std::uint32_t /*_size*/,
                                                                   endpoint_data_type& /*_endpoint_data*/) const {
    return true;
}

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::queue_train(target_data_iterator_type /*_it*/, const std::shared_ptr<train>& /*_train*/) {
    return false;
}

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::flush(endpoint_type /*_key*/) {
    return true;
}

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::connect_cbk(boost::system::error_code const& /*_error*/) { }

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::send_cbk(const endpoint_type /*_key*/, boost::system::error_code const& /*_error*/,
                                                          std::size_t /*_bytes*/) { }

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::flush_cbk(endpoint_type /*_key*/, const boost::system::error_code& /*_error_code*/) { }

template<typename Protocol>
size_t vsomeip_v3::server_endpoint_impl<Protocol>::get_queue_size() const {
    return 0;
}

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::start_dispatch_timer(target_data_iterator_type /*_it*/,
                                                                      const std::chrono::steady_clock::time_point& /*_now*/) { }

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::cancel_dispatch_timer(target_data_iterator_type /*_it*/) { }

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::update_last_departure(endpoint_data_type& /*_data*/) { }

struct mocked_socket_factory : public vsomeip_v3::abstract_socket_factory {
#if defined(__linux__)
    std::shared_ptr<vsomeip_v3::abstract_netlink_connector>
    create_netlink_connector(boost::asio::io_context&, const boost::asio::ip::address&, const boost::asio::ip::address&, bool) override {
        return nullptr;
    };
#endif

    std::unique_ptr<vsomeip_v3::tcp_socket> create_tcp_socket(boost::asio::io_context&) override { return nullptr; };
    std::unique_ptr<vsomeip_v3::tcp_acceptor> create_tcp_acceptor(boost::asio::io_context&) override { return nullptr; }

    std::unique_ptr<vsomeip_v3::udp_socket> create_udp_socket(boost::asio::io_context& _io) override {
        return std::make_unique<vsomeip_v3::asio_udp_socket>(_io);
    }

#if defined(__linux__) || defined(__QNX__)
    std::unique_ptr<vsomeip_v3::uds_socket> create_uds_socket(boost::asio::io_context&) { return nullptr; }
    std::unique_ptr<vsomeip_v3::uds_acceptor> create_uds_acceptor(boost::asio::io_context&) override { return nullptr; }
#endif

    std::unique_ptr<vsomeip_v3::abstract_timer> create_timer(boost::asio::io_context&) { return nullptr; }
};

vsomeip_v3::abstract_socket_factory* vsomeip_v3::abstract_socket_factory::get() {
    static std::shared_ptr<vsomeip_v3::abstract_socket_factory> const factory = std::make_shared<mocked_socket_factory>();
    return factory.get();
}

template class vsomeip_v3::endpoint_impl<boost::asio::ip::udp>;
template class vsomeip_v3::server_endpoint_impl<boost::asio::ip::udp>;

struct mock_endpoint_host : public vsomeip_v3::boardnet_endpoint_host {
    MOCK_METHOD1(on_connect, void(std::shared_ptr<vsomeip_v3::boardnet_endpoint> _endpoint));
    MOCK_METHOD1(on_disconnect, void(std::shared_ptr<vsomeip_v3::boardnet_endpoint> _endpoint));
    MOCK_METHOD4(on_bind_error,
                 bool(std::shared_ptr<vsomeip_v3::boardnet_endpoint> _endpoint, const boost::asio::ip::address& _remote_address,
                      uint16_t _remote_port, uint16_t& _local_port));
    MOCK_METHOD5(on_error,
                 void(const vsomeip_v3::byte_t* _data, vsomeip_v3::length_t _length, vsomeip_v3::boardnet_endpoint* const _receiver,
                      const boost::asio::ip::address& _remote_address, std::uint16_t _remote_port));
    MOCK_METHOD2(release_port, void(uint16_t _port, bool _reliable));
    MOCK_CONST_METHOD0(get_client, vsomeip_v3::client_t());
    MOCK_CONST_METHOD0(get_client_host, std::string());
    MOCK_CONST_METHOD2(find_instance,
                       vsomeip_v3::instance_t(vsomeip_v3::service_t _service, vsomeip_v3::boardnet_endpoint* const _endpoint));
    MOCK_METHOD1(add_multicast_option, void(const vsomeip_v3::multicast_option_t& _option));
};

struct mock_boardnet_routing_host : public vsomeip_v3::boardnet_routing_host {
    MOCK_METHOD6(on_message,
                 void(const vsomeip_v3::byte_t* _data, vsomeip_v3::length_t _length, vsomeip_v3::boardnet_endpoint* _receiver,
                      const boost::asio::ip::address& _remote_address, vsomeip_v3::port_t _remote_port, bool _is_multicast));
    MOCK_METHOD3(remove_subscriptions,
                 void(vsomeip_v3::port_t _local_port, const boost::asio::ip::address& _remote_address, vsomeip_v3::port_t _remote_port));
};
