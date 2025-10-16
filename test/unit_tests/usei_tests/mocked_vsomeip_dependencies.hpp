// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef MOCKED_VSOMEIP_DEPENDENCIES_HPP_
#define MOCKED_VSOMEIP_DEPENDENCIES_HPP_

#include <gmock/gmock.h>

#include <string>

#include <vsomeip/primitive_types.hpp>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif

#define private public
#define protected public
#include "../../../implementation/endpoints/include/udp_server_endpoint_impl.hpp"
#include "../../../implementation/endpoints/include/endpoint_host.hpp"
#include "../../../implementation/routing/include/routing_host.hpp"

template<typename Protocol>
vsomeip_v3::endpoint_impl<Protocol>::endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host,
                                                   const std::shared_ptr<routing_host>& _routing_host, boost::asio::io_context& _io,
                                                   const std::shared_ptr<configuration>& _configuration) :
    io_(_io), endpoint_host_(_endpoint_host), routing_host_(_routing_host), is_supporting_magic_cookies_(false),
    has_enabled_magic_cookies_(false), use_count_(0), sending_blocked_(false), configuration_(_configuration),
    is_supporting_someip_tp_(false) { }

template<typename Protocol>
void vsomeip_v3::endpoint_impl<Protocol>::enable_magic_cookies() {
    has_enabled_magic_cookies_ = is_supporting_magic_cookies_;
}

template<typename Protocol>
uint32_t vsomeip_v3::endpoint_impl<Protocol>::find_magic_cookie(byte_t* /*_buffer*/, size_t /*_size*/) {
    return 0xFFFFFFFF;
}

template<typename Protocol>
void vsomeip_v3::endpoint_impl<Protocol>::add_default_target(service_t, const std::string&, uint16_t) { }

template<typename Protocol>
void vsomeip_v3::endpoint_impl<Protocol>::remove_default_target(service_t) { }

template<typename Protocol>
void vsomeip_v3::endpoint_impl<Protocol>::remove_stop_handler(service_t) { }

template<typename Protocol>
void vsomeip_v3::endpoint_impl<Protocol>::register_error_handler(const error_handler_t& /*_error_handler*/) { }

template<typename Protocol>
vsomeip_v3::instance_t vsomeip_v3::endpoint_impl<Protocol>::get_instance(service_t /*_service*/) {
    return 0xFFFF;
}

template<typename Protocol>
vsomeip_v3::server_endpoint_impl<Protocol>::server_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host,
                                                                 const std::shared_ptr<routing_host>& _routing_host,
                                                                 boost::asio::io_context& _io,
                                                                 const std::shared_ptr<configuration>& _configuration) :
    endpoint_impl<Protocol>(_endpoint_host, _routing_host, _io, _configuration) { }

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::prepare_stop(const endpoint::prepare_stop_handler_t& /*_handler*/,
                                                              service_t /*_service*/) { }

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::stop() { }

template<typename Protocol>
bool vsomeip_v3::server_endpoint_impl<Protocol>::is_client() const {
    return false;
}

template<typename Protocol>
void vsomeip_v3::server_endpoint_impl<Protocol>::restart(bool /*_force*/) {
    boost::system::error_code its_error;
    this->stop();
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
void vsomeip_v3::server_endpoint_impl<Protocol>::remove_stop_handler(service_t /*_service*/) { }

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

template class vsomeip_v3::endpoint_impl<boost::asio::ip::udp>;
template class vsomeip_v3::server_endpoint_impl<boost::asio::ip::udp>;

struct mock_endpoint_host : public vsomeip_v3::endpoint_host {
    MOCK_METHOD1(on_connect, void(std::shared_ptr<vsomeip_v3::endpoint> _endpoint));
    MOCK_METHOD1(on_disconnect, void(std::shared_ptr<vsomeip_v3::endpoint> _endpoint));
    MOCK_METHOD4(on_bind_error,
                 bool(std::shared_ptr<vsomeip_v3::endpoint> _endpoint, const boost::asio::ip::address& _remote_address,
                      uint16_t _remote_port, uint16_t& _local_port));
    MOCK_METHOD5(on_error,
                 void(const vsomeip_v3::byte_t* _data, vsomeip_v3::length_t _length, vsomeip_v3::endpoint* const _receiver,
                      const boost::asio::ip::address& _remote_address, std::uint16_t _remote_port));
    MOCK_METHOD2(release_port, void(uint16_t _port, bool _reliable));
    MOCK_CONST_METHOD0(get_client, vsomeip_v3::client_t());
    MOCK_CONST_METHOD0(get_client_host, std::string());
    MOCK_CONST_METHOD2(find_instance, vsomeip_v3::instance_t(vsomeip_v3::service_t _service, vsomeip_v3::endpoint* const _endpoint));
    MOCK_METHOD1(add_multicast_option, void(const vsomeip_v3::multicast_option_t& _option));
};

struct mock_routing_host : public vsomeip_v3::routing_host {
    MOCK_METHOD8(on_message,
                 void(const vsomeip_v3::byte_t* _data, vsomeip_v3::length_t _length, vsomeip_v3::endpoint* _receiver, bool _is_multicast,
                      vsomeip_v3::client_t _bound_client, const vsomeip_sec_client_t* _sec_client,
                      const boost::asio::ip::address& _remote_address, std::uint16_t _remote_port));
    MOCK_CONST_METHOD0(get_client, vsomeip_v3::client_t());
    MOCK_METHOD3(add_guest, void(vsomeip_v3::client_t _client, const boost::asio::ip::address& _address, vsomeip_v3::port_t _port));
    MOCK_METHOD2(add_known_client, void(vsomeip_v3::client_t _client, const std::string& _client_host));
    MOCK_CONST_METHOD2(get_guest_by_address, vsomeip_v3::client_t(const boost::asio::ip::address& _address, vsomeip_v3::port_t _port));
    MOCK_METHOD2(remove_local, void(vsomeip_v3::client_t _client, bool _remove_sec_client));
    MOCK_CONST_METHOD1(get_env, std::string(vsomeip_v3::client_t _client));
    MOCK_METHOD3(remove_subscriptions,
                 void(vsomeip_v3::port_t _local_port, const boost::asio::ip::address& _remote_address, vsomeip_v3::port_t _remote_port));
    MOCK_METHOD0(get_routing_state, vsomeip_v3::routing_state_e());
};

#endif /* MOCKED_VSOMEIP_DEPENDENCIES_HPP_ */