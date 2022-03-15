// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/ip/multicast.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/endpoint_definition.hpp"
#include "../include/endpoint_host.hpp"
#include "../include/tp.hpp"
#include "../../routing/include/routing_host.hpp"
#include "../include/udp_server_endpoint_impl.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"
#include "../../service_discovery/include/defines.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip_v3 {

udp_server_endpoint_impl::udp_server_endpoint_impl(
        const std::shared_ptr<endpoint_host>& _endpoint_host,
        const std::shared_ptr<routing_host>& _routing_host,
        const endpoint_type& _local,
        boost::asio::io_service &_io,
        const std::shared_ptr<configuration>& _configuration) :
      server_endpoint_impl<ip::udp_ext>(_endpoint_host, _routing_host, _local,
                _io, VSOMEIP_MAX_UDP_MESSAGE_SIZE,
                _configuration->get_endpoint_queue_limit(_configuration->get_unicast_address().to_string(), _local.port()),
                _configuration),
      unicast_socket_(_io, _local.protocol()),
      unicast_recv_buffer_(VSOMEIP_MAX_UDP_MESSAGE_SIZE, 0),
      multicast_id_(0),
      joined_group_(false),
      local_port_(_local.port()),
      tp_reassembler_(std::make_shared<tp::tp_reassembler>(_configuration->get_max_message_size_unreliable(), _io)),
      tp_cleanup_timer_(_io) {
    is_supporting_someip_tp_ = true;

    boost::system::error_code ec;

    boost::asio::socket_base::reuse_address optionReuseAddress(true);
    unicast_socket_.set_option(optionReuseAddress, ec);
    boost::asio::detail::throw_error(ec, "reuse address");

#ifndef _WIN32
    // If specified, bind to device
    std::string its_device(configuration_->get_device());
    if (its_device != "") {
        if (setsockopt(unicast_socket_.native_handle(),
                SOL_SOCKET, SO_BINDTODEVICE, its_device.c_str(), (int)its_device.size()) == -1) {
            VSOMEIP_WARNING << "UDP Server: Could not bind to device \"" << its_device << "\"";
        }
    }
#endif

    unicast_socket_.bind(_local, ec);
    boost::asio::detail::throw_error(ec, "bind");

    if (local_.address().is_v4()) {
        boost::asio::ip::multicast::outbound_interface option(_local.address().to_v4());
        unicast_socket_.set_option(option, ec);
        boost::asio::detail::throw_error(ec, "outbound interface option IPv4");
    } else if (local_.address().is_v6()) {
        boost::asio::ip::multicast::outbound_interface option(
                static_cast<unsigned int>(local_.address().to_v6().scope_id()));
        unicast_socket_.set_option(option, ec);
        boost::asio::detail::throw_error(ec, "outbound interface option IPv6");
    }

    boost::asio::socket_base::broadcast option(true);
    unicast_socket_.set_option(option, ec);
    boost::asio::detail::throw_error(ec, "broadcast option");

    const std::uint32_t its_udp_recv_buffer_size =
            configuration_->get_udp_receive_buffer_size();
    unicast_socket_.set_option(boost::asio::socket_base::receive_buffer_size(
            its_udp_recv_buffer_size), ec);

    if (ec) {
        VSOMEIP_WARNING << "udp_server_endpoint_impl:: couldn't set "
                << "SO_RCVBUF: " << ec.message() << " to: " << std::dec
                << its_udp_recv_buffer_size << " local port: " << std::dec
                << local_port_;
    } else {
        boost::asio::socket_base::receive_buffer_size its_option;
        unicast_socket_.get_option(its_option, ec);
        if (ec) {
            VSOMEIP_WARNING << "udp_server_endpoint_impl: couldn't get "
                    << "SO_RCVBUF: " << ec.message() << " local port:"
                    << std::dec << local_port_;
        } else {
            VSOMEIP_INFO << "udp_server_endpoint_impl: SO_RCVBUF is: "
                    << std::dec << its_option.value();
        }
    }

#ifdef _WIN32
    const char* optval("0001");
    ::setsockopt(unicast_socket_.native_handle(), IPPROTO_IP, IP_PKTINFO,
        optval, sizeof(optval));
#else
    int optval(1);
    ::setsockopt(unicast_socket_.native_handle(), IPPROTO_IP, IP_PKTINFO,
        &optval, sizeof(optval));
#endif
}

udp_server_endpoint_impl::~udp_server_endpoint_impl() {
}

bool udp_server_endpoint_impl::is_local() const {
    return false;
}

void udp_server_endpoint_impl::start() {
    receive();
}

void udp_server_endpoint_impl::stop() {
    server_endpoint_impl::stop();
    {
        std::lock_guard<std::mutex> its_lock(unicast_mutex_);

        if (unicast_socket_.is_open()) {
            boost::system::error_code its_error;
            unicast_socket_.shutdown(socket_type::shutdown_both, its_error);
            unicast_socket_.close(its_error);
        }
    }

    {
        std::lock_guard<std::mutex> its_lock(multicast_mutex_);

        if (multicast_socket_ && multicast_socket_->is_open()) {
            boost::system::error_code its_error;
            multicast_socket_->shutdown(socket_type::shutdown_both, its_error);
            multicast_socket_->close(its_error);
        }
    }

    tp_reassembler_->stop();
}

void udp_server_endpoint_impl::receive() {
    receive_unicast();
}

void udp_server_endpoint_impl::receive_unicast() {

    std::lock_guard<std::mutex> its_lock(unicast_mutex_);

    if(unicast_socket_.is_open()) {
        unicast_socket_.async_receive_from(
                boost::asio::buffer(&unicast_recv_buffer_[0], max_message_size_),
            unicast_remote_,
            std::bind(
                &udp_server_endpoint_impl::on_unicast_received,
                std::dynamic_pointer_cast<
                    udp_server_endpoint_impl >(shared_from_this()),
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3
            )
        );
    }
}

//
// receive_multicast is called with multicast_mutex_ being hold
//
void udp_server_endpoint_impl::receive_multicast(uint8_t _multicast_id) {

    if (_multicast_id == multicast_id_ && multicast_socket_ && multicast_socket_->is_open()) {
        multicast_socket_->async_receive_from(
                boost::asio::buffer(&multicast_recv_buffer_[0], max_message_size_),
            multicast_remote_,
            std::bind(
                &udp_server_endpoint_impl::on_multicast_received,
                std::dynamic_pointer_cast<
                    udp_server_endpoint_impl >(shared_from_this()),
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3,
                _multicast_id
            )
        );
    }
}

bool udp_server_endpoint_impl::send_to(
    const std::shared_ptr<endpoint_definition> _target,
    const byte_t *_data, uint32_t _size) {

    std::lock_guard<std::mutex> its_lock(mutex_);
    endpoint_type its_target(_target->get_address(), _target->get_port());
    return send_intern(its_target, _data, _size);
}

bool udp_server_endpoint_impl::send_error(
    const std::shared_ptr<endpoint_definition> _target,
    const byte_t *_data, uint32_t _size) {

    bool ret(false);
    std::lock_guard<std::mutex> its_lock(mutex_);
    const endpoint_type its_target(_target->get_address(), _target->get_port());
    const queue_iterator_type target_queue_iterator(find_or_create_queue_unlocked(its_target));
    auto& its_qpair = target_queue_iterator->second;
    const bool queue_size_zero_on_entry(its_qpair.second.empty());

    if (check_message_size(nullptr, _size, its_target) == endpoint_impl::cms_ret_e::MSG_OK &&
        check_queue_limit(_data, _size, its_qpair.first)) {
        its_qpair.second.emplace_back(
                std::make_shared<message_buffer_t>(_data, _data + _size));
        its_qpair.first += _size;

        if (queue_size_zero_on_entry) { // no writing in progress
            send_queued(target_queue_iterator);
        }
        ret = true;
    }
    return ret;
}

void udp_server_endpoint_impl::send_queued(
        const queue_iterator_type _queue_iterator) {

    message_buffer_ptr_t its_buffer = _queue_iterator->second.second.front();
#if 0
        std::stringstream msg;
        msg << "usei::sq(" << _queue_iterator->first.address().to_string() << ":"
            << _queue_iterator->first.port() << "): ";
        for (std::size_t i = 0; i < its_buffer->size(); ++i)
            msg << std::hex << std::setw(2) << std::setfill('0')
                << (int)(*its_buffer)[i] << " ";
        VSOMEIP_INFO << msg.str();
#endif
    std::lock_guard<std::mutex> its_lock(unicast_mutex_);

    unicast_socket_.async_send_to(
        boost::asio::buffer(*its_buffer),
        _queue_iterator->first,
        std::bind(
            &udp_server_endpoint_base_impl::send_cbk,
            shared_from_this(),
            _queue_iterator,
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void udp_server_endpoint_impl::get_configured_times_from_endpoint(
        service_t _service, method_t _method,
        std::chrono::nanoseconds *_debouncing,
        std::chrono::nanoseconds *_maximum_retention) const {

    configuration_->get_configured_timing_responses(_service,
            udp_server_endpoint_base_impl::local_.address().to_string(),
            udp_server_endpoint_base_impl::local_.port(), _method,
            _debouncing, _maximum_retention);
}

//
// Both is_joined - methods must be called with multicast_mutex_ being hold!
//
bool udp_server_endpoint_impl::is_joined(const std::string &_address) const {

    return (joined_.find(_address) != joined_.end());
}

bool udp_server_endpoint_impl::is_joined(
        const std::string &_address, bool* _received) const {

    const auto found_address = joined_.find(_address);
    if (found_address != joined_.end()) {
        *_received = found_address->second;
    } else {
        *_received = false;
    }

    return (found_address != joined_.end());
}

void udp_server_endpoint_impl::join(const std::string &_address) {

    std::lock_guard<std::mutex> its_lock(multicast_mutex_);
    join_unlocked(_address);
}

void udp_server_endpoint_impl::join_unlocked(const std::string &_address) {

    bool has_received(false);

    //
    // join_func must be called with multicast_mutex_ being hold!
    //
    auto join_func = [this](const std::string &_address) {
        try {
            VSOMEIP_DEBUG << "Joining to multicast group " << _address
                    << " from " << local_.address().to_string();

            boost::system::error_code ec;

            bool is_v4(false);
            bool is_v6(false);
            {
                std::lock_guard<std::mutex> its_lock(local_mutex_);
                is_v4 = local_.address().is_v4();
                is_v6 = local_.address().is_v6();
            }

            if (multicast_recv_buffer_.empty())
                multicast_recv_buffer_.resize(VSOMEIP_MAX_UDP_MESSAGE_SIZE, 0);

            if (!multicast_local_) {
                if (is_v4) {
                    multicast_local_ = std::unique_ptr<endpoint_type>(
                            new endpoint_type(boost::asio::ip::address_v4::any(), local_port_));
                }
                if (is_v6) {
                    multicast_local_ = std::unique_ptr<endpoint_type>(
                            new endpoint_type(boost::asio::ip::address_v6::any(), local_port_));
                }
            }

            if (!multicast_socket_) {
                multicast_socket_ = std::unique_ptr<socket_type>(
                        new socket_type(service_, local_.protocol()));

                boost::asio::socket_base::reuse_address optionReuseAddress(true);
                multicast_socket_->set_option(optionReuseAddress, ec);
                boost::asio::detail::throw_error(ec, "reuse address in multicast");
                boost::asio::socket_base::broadcast optionBroadcast(true);
                multicast_socket_->set_option(optionBroadcast, ec);
                boost::asio::detail::throw_error(ec, "set broadcast option");

                multicast_socket_->bind(*multicast_local_, ec);
                boost::asio::detail::throw_error(ec, "bind multicast");

                const std::uint32_t its_udp_recv_buffer_size =
                        configuration_->get_udp_receive_buffer_size();

                multicast_socket_->set_option(boost::asio::socket_base::receive_buffer_size(
                        its_udp_recv_buffer_size), ec);

                if (ec) {
                    VSOMEIP_WARNING << "udp_server_endpoint_impl:: couldn't set "
                            << "SO_RCVBUF: " << ec.message() << " to: " << std::dec
                            << its_udp_recv_buffer_size << " local port: " << std::dec
                            << local_port_;
                } else {
                    boost::asio::socket_base::receive_buffer_size its_option;
                    multicast_socket_->get_option(its_option, ec);

                    if (ec) {
                        VSOMEIP_WARNING << "udp_server_endpoint_impl: couldn't get "
                                << "SO_RCVBUF: " << ec.message() << " local port:"
                                << std::dec << local_port_;
                    } else {
                        VSOMEIP_INFO << "udp_server_endpoint_impl: SO_RCVBUF (Multicast) is: "
                                << std::dec << its_option.value();
                    }
                }

#ifdef _WIN32
                const char* optval("0001");
                if (is_v4) {
                    ::setsockopt(multicast_socket_->native_handle(), IPPROTO_IP, IP_PKTINFO,
                        optval, sizeof(optval));
                } else if (is_v6) {
                    ::setsockopt(multicast_socket_->native_handle(), IPPROTO_IPV6, IPV6_PKTINFO,
                        optval, sizeof(optval));
                }
#else
                int optval(1);
                if (is_v4) {
                    ::setsockopt(multicast_socket_->native_handle(), IPPROTO_IP, IP_PKTINFO,
                        &optval, sizeof(optval));
                } else {
                    ::setsockopt(multicast_socket_->native_handle(), IPPROTO_IPV6, IPV6_RECVPKTINFO,
                        &optval, sizeof(optval));
                }
#endif
                multicast_id_++;
                receive_multicast(multicast_id_);
            }

            if (is_v4) {
                multicast_socket_->set_option(ip::udp_ext::socket::reuse_address(true));
                multicast_socket_->set_option(
                    boost::asio::ip::multicast::enable_loopback(false));
                multicast_socket_->set_option(boost::asio::ip::multicast::join_group(
                    boost::asio::ip::address::from_string(_address).to_v4(),
                    local_.address().to_v4()));
            } else if (is_v6) {
                multicast_socket_->set_option(ip::udp_ext::socket::reuse_address(true));
                multicast_socket_->set_option(
                    boost::asio::ip::multicast::enable_loopback(false));
                multicast_socket_->set_option(boost::asio::ip::multicast::join_group(
                    boost::asio::ip::address::from_string(_address).to_v6(),
                    local_.address().to_v6().scope_id()));
            }

            joined_[_address] = false;
            joined_group_ = true;

        } catch (const std::exception &e) {
            VSOMEIP_ERROR << "udp_server_endpoint_impl::join" << ":" << e.what();
        }
    };

    if (!is_joined(_address, &has_received)) {
        join_func(_address);
    } else if (!has_received) {
        // joined the multicast group but didn't receive a event yet -> rejoin
        leave_unlocked(_address);
        join_func(_address);
    }
}

void udp_server_endpoint_impl::leave(const std::string &_address) {

    std::lock_guard<std::mutex> its_lock(multicast_mutex_);
    leave_unlocked(_address);
}

void udp_server_endpoint_impl::leave_unlocked(const std::string &_address) {

    try {
        if (is_joined(_address)) {
            VSOMEIP_DEBUG << "Leaving the multicast group " << _address
                    << " from " << local_.address().to_string();

            bool is_v4(false);
            bool is_v6(false);
            {
                std::lock_guard<std::mutex> its_lock(local_mutex_);
                is_v4 = local_.address().is_v4();
                is_v6 = local_.address().is_v6();
            }
            if (is_v4) {
                multicast_socket_->set_option(boost::asio::ip::multicast::leave_group(
                    boost::asio::ip::address::from_string(_address)));
            } else if (is_v6) {
                multicast_socket_->set_option(boost::asio::ip::multicast::leave_group(
                    boost::asio::ip::address::from_string(_address)));
            }

            joined_.erase(_address);
            if (0 == joined_.size()) {
                joined_group_ = false;

                boost::system::error_code ec;
                multicast_socket_->cancel(ec);

                multicast_socket_.reset(nullptr);
                multicast_local_.reset(nullptr);
            }
        }
    }
    catch (const std::exception &e) {
        VSOMEIP_ERROR << __func__ << ":" << e.what();
    }
}

void udp_server_endpoint_impl::add_default_target(
        service_t _service, const std::string &_address, uint16_t _port) {
    std::lock_guard<std::mutex> its_lock(default_targets_mutex_);
    endpoint_type its_endpoint(
            boost::asio::ip::address::from_string(_address), _port);
    default_targets_[_service] = its_endpoint;
}

void udp_server_endpoint_impl::remove_default_target(service_t _service) {
    std::lock_guard<std::mutex> its_lock(default_targets_mutex_);
    default_targets_.erase(_service);
}

bool udp_server_endpoint_impl::get_default_target(service_t _service,
        udp_server_endpoint_impl::endpoint_type &_target) const {
    std::lock_guard<std::mutex> its_lock(default_targets_mutex_);
    bool is_valid(false);
    auto find_service = default_targets_.find(_service);
    if (find_service != default_targets_.end()) {
        _target = find_service->second;
        is_valid = true;
    }
    return is_valid;
}

std::uint16_t udp_server_endpoint_impl::get_local_port() const {
    return local_port_;
}

void udp_server_endpoint_impl::on_unicast_received(
        boost::system::error_code const &_error,
        std::size_t _bytes,
        boost::asio::ip::address const &_destination) {

    if (_error != boost::asio::error::operation_aborted) {
        {
            // By locking the multicast mutex here it is ensured that unicast
            // & multicast messages are not processed in parallel. This aligns
            // the behavior of endpoints with one and two active sockets.
            std::lock_guard<std::mutex> its_lock(multicast_mutex_);
            on_message_received(_error, _bytes, _destination,
                    unicast_remote_, unicast_recv_buffer_);
        }
        receive_unicast();
    }
}

void udp_server_endpoint_impl::on_multicast_received(
        boost::system::error_code const &_error,
        std::size_t _bytes,
        boost::asio::ip::address const &_destination,
        uint8_t _multicast_id) {

    std::lock_guard<std::mutex> its_lock(multicast_mutex_);
    if (_error != boost::asio::error::operation_aborted) {
        // Filter messages sent from the same source address
        if (multicast_remote_.address() != local_.address()) {
            on_message_received(_error, _bytes, _destination,
                    multicast_remote_, multicast_recv_buffer_);
        }

        receive_multicast(_multicast_id);
    }
}

void udp_server_endpoint_impl::on_message_received(
        boost::system::error_code const &_error, std::size_t _bytes,
        boost::asio::ip::address const &_destination,
        endpoint_type const &_remote,
        message_buffer_t const &_buffer) {
#if 0
    std::stringstream msg;
    msg << "usei::rcb(" << _error.message() << "): ";
    for (std::size_t i = 0; i < _bytes; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0')
            << (int) recv_buffer_[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    std::shared_ptr<routing_host> its_host = routing_host_.lock();

    if (its_host) {
        if (!_error && 0 < _bytes) {
            std::size_t remaining_bytes = _bytes;
            std::size_t i = 0;
            const boost::asio::ip::address its_remote_address(_remote.address());
            const std::uint16_t its_remote_port(_remote.port());
            do {
                uint64_t read_message_size
                    = utility::get_message_size(&_buffer[i],
                            remaining_bytes);
                if (read_message_size > MESSAGE_SIZE_UNLIMITED) {
                    VSOMEIP_ERROR << "Message size exceeds allowed maximum!";
                    return;
                }
                uint32_t current_message_size = static_cast<uint32_t>(read_message_size);
                if (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE &&
                        current_message_size <= remaining_bytes) {
                    if (remaining_bytes - current_message_size > remaining_bytes) {
                        VSOMEIP_ERROR << "buffer underflow in udp client endpoint ~> abort!";
                        return;
                    } else if (current_message_size > VSOMEIP_RETURN_CODE_POS &&
                        (_buffer[i + VSOMEIP_PROTOCOL_VERSION_POS] != VSOMEIP_PROTOCOL_VERSION ||
                         !utility::is_valid_message_type(tp::tp::tp_flag_unset(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS])) ||
                         /*!utility::is_valid_return_code(static_cast<return_code_e>(_buffer[i + VSOMEIP_RETURN_CODE_POS])) ||*/
                         (tp::tp::tp_flag_is_set(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS]) && get_local_port() == configuration_->get_sd_port())
                        )) {
                        if (_buffer[i + VSOMEIP_PROTOCOL_VERSION_POS] != VSOMEIP_PROTOCOL_VERSION) {
                            VSOMEIP_ERROR << "use: Wrong protocol version: 0x"
                                    << std::hex << std::setw(2) << std::setfill('0')
                                    << std::uint32_t(_buffer[i + VSOMEIP_PROTOCOL_VERSION_POS])
                                    << " local: " << get_address_port_local()
                                    << " remote: " << its_remote_address << ":" << std::dec << its_remote_port;
                            // ensure to send back a message w/ wrong protocol version
                            its_host->on_message(&_buffer[i],
                                                 VSOMEIP_SOMEIP_HEADER_SIZE + 8, this,
                                                 _destination,
                                                 VSOMEIP_ROUTING_CLIENT,
                                                 std::make_pair(ANY_UID, ANY_GID),
                                                 its_remote_address, its_remote_port);
                        } else if (!utility::is_valid_message_type(tp::tp::tp_flag_unset(
                                _buffer[i + VSOMEIP_MESSAGE_TYPE_POS]))) {
                            VSOMEIP_ERROR << "use: Invalid message type: 0x"
                                    << std::hex << std::setw(2) << std::setfill('0')
                                    << std::uint32_t(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS])
                                    << " local: " << get_address_port_local()
                                    << " remote: " << its_remote_address << ":" << std::dec << its_remote_port;
                        } else if (!utility::is_valid_return_code(static_cast<return_code_e>(
                                _buffer[i + VSOMEIP_RETURN_CODE_POS]))) {
                            VSOMEIP_ERROR << "use: Invalid return code: 0x"
                                    << std::hex << std::setw(2) << std::setfill('0')
                                    << std::uint32_t(_buffer[i + VSOMEIP_RETURN_CODE_POS])
                                    << " local: " << get_address_port_local()
                                    << " remote: " << its_remote_address << ":" << std::dec << its_remote_port;
                        } else if (tp::tp::tp_flag_is_set(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS])
                            && get_local_port() == configuration_->get_sd_port()) {
                            VSOMEIP_WARNING << "use: Received a SomeIP/TP message on SD port:"
                                    << " local: " << get_address_port_local()
                                    << " remote: " << its_remote_address << ":" << std::dec << its_remote_port;
                        }
                        return;
                    }
                    remaining_bytes -= current_message_size;
                    const service_t its_service = VSOMEIP_BYTES_TO_WORD(_buffer[i + VSOMEIP_SERVICE_POS_MIN],
                                                                        _buffer[i + VSOMEIP_SERVICE_POS_MAX]);
                    if (utility::is_request(
                            _buffer[i + VSOMEIP_MESSAGE_TYPE_POS])) {
                        const client_t its_client = VSOMEIP_BYTES_TO_WORD(
                                _buffer[i + VSOMEIP_CLIENT_POS_MIN],
                                _buffer[i + VSOMEIP_CLIENT_POS_MAX]);
                        if (its_client != MAGIC_COOKIE_CLIENT) {
                            const session_t its_session = VSOMEIP_BYTES_TO_WORD(
                                    _buffer[i + VSOMEIP_SESSION_POS_MIN],
                                    _buffer[i + VSOMEIP_SESSION_POS_MAX]);
                            clients_mutex_.lock();
                            clients_[its_client][its_session] = _remote;
                            clients_mutex_.unlock();
                        }
                    } else if (its_service != VSOMEIP_SD_SERVICE
                            && utility::is_notification(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS])
                            && joined_group_) {
                        boost::system::error_code ec;
                        const auto found_address = joined_.find(_destination.to_string(ec));
                        if (found_address != joined_.end()) {
                            found_address->second = true;
                        }
                    }
                    if (tp::tp::tp_flag_is_set(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS])) {
                        const method_t its_method = VSOMEIP_BYTES_TO_WORD(_buffer[i + VSOMEIP_METHOD_POS_MIN],
                                                                          _buffer[i + VSOMEIP_METHOD_POS_MAX]);
                        if (!tp_segmentation_enabled(its_service, its_method)) {
                            VSOMEIP_WARNING << "use: Received a SomeIP/TP message for service: 0x" << std::hex << its_service
                                    << " method: 0x" << its_method << " which is not configured for TP:"
                                    << " local: " << get_address_port_local()
                                    << " remote: " << its_remote_address << ":" << std::dec << its_remote_port;
                            return;
                        }
                        const auto res = tp_reassembler_->process_tp_message(
                                &_buffer[i], current_message_size,
                                its_remote_address, its_remote_port);
                        if (res.first) {
                            if (utility::is_request(res.second[VSOMEIP_MESSAGE_TYPE_POS])) {
                                const client_t its_client = VSOMEIP_BYTES_TO_WORD(
                                        res.second[VSOMEIP_CLIENT_POS_MIN],
                                        res.second[VSOMEIP_CLIENT_POS_MAX]);
                                if (its_client != MAGIC_COOKIE_CLIENT) {
                                    const session_t its_session = VSOMEIP_BYTES_TO_WORD(
                                            res.second[VSOMEIP_SESSION_POS_MIN],
                                            res.second[VSOMEIP_SESSION_POS_MAX]);
                                    std::lock_guard<std::mutex> its_client_lock(clients_mutex_);
                                    clients_[its_client][its_session] = _remote;
                                }
                            } else if (its_service != VSOMEIP_SD_SERVICE
                                    && utility::is_notification(res.second[VSOMEIP_MESSAGE_TYPE_POS])
                                    && joined_group_) {
                                boost::system::error_code ec;
                                const auto found_address = joined_.find(_destination.to_string(ec));
                                if (found_address != joined_.end()) {
                                    found_address->second = true;
                                }
                            }
                            its_host->on_message(&res.second[0],
                                    static_cast<std::uint32_t>(res.second.size()),
                                    this, _destination, VSOMEIP_ROUTING_CLIENT,
                                    std::make_pair(ANY_UID, ANY_GID),
                                    its_remote_address, its_remote_port);
                        }
                    } else {
                        if (its_service != VSOMEIP_SD_SERVICE ||
                            (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE &&
                                    current_message_size >= remaining_bytes)) {
                            its_host->on_message(&_buffer[i],
                                    current_message_size, this, _destination,
                                    VSOMEIP_ROUTING_CLIENT,
                                    std::make_pair(ANY_UID, ANY_GID),
                                    its_remote_address, its_remote_port);
                        } else {
                            //ignore messages for service discovery with shorter SomeIP length
                            VSOMEIP_ERROR << "Received an unreliable vSomeIP SD message with too short length field"
                                    << " local: " << get_address_port_local()
                                    << " remote: " << its_remote_address << ":" << std::dec << its_remote_port;
                        }
                    }
                    i += current_message_size;
                } else {
                    VSOMEIP_ERROR << "Received an unreliable vSomeIP message with bad length field"
                            << " local: " << get_address_port_local()
                            << " remote: " << its_remote_address << ":" << std::dec << its_remote_port;
                    if (remaining_bytes > VSOMEIP_SERVICE_POS_MAX) {
                        service_t its_service = VSOMEIP_BYTES_TO_WORD(_buffer[VSOMEIP_SERVICE_POS_MIN],
                                _buffer[VSOMEIP_SERVICE_POS_MAX]);
                        if (its_service != VSOMEIP_SD_SERVICE) {
                            if (read_message_size == 0) {
                                VSOMEIP_ERROR << "Ignoring unreliable vSomeIP message with SomeIP message length 0!";
                            } else {
                                auto its_endpoint_host = endpoint_host_.lock();
                                if (its_endpoint_host) {
                                    its_endpoint_host->on_error(&_buffer[i],
                                            (uint32_t)remaining_bytes, this,
                                            its_remote_address, its_remote_port);
                                }
                            }
                        }
                    }
                    remaining_bytes = 0;
                }
            } while (remaining_bytes > 0);
        }
    }
}

void udp_server_endpoint_impl::print_status() {
    std::lock_guard<std::mutex> its_lock(mutex_);

    VSOMEIP_INFO << "status use: " << std::dec << local_port_
            << " number queues: " << std::dec << queues_.size()
            << " recv_buffer: "
            << std::dec << unicast_recv_buffer_.capacity()
            << " multicast_recv_buffer: "
            << std::dec << multicast_recv_buffer_.capacity();

    for (const auto &c : queues_) {
        std::size_t its_data_size(0);
        std::size_t its_queue_size(0);
        its_queue_size = c.second.second.size();
        its_data_size = c.second.first;

        boost::system::error_code ec;
        VSOMEIP_INFO << "status use: client: "
                << c.first.address().to_string(ec) << ":"
                << std::dec << c.first.port()
                << " queue: " << std::dec << its_queue_size
                << " data: " << std::dec << its_data_size;
    }
}

std::string udp_server_endpoint_impl::get_remote_information(
        const queue_iterator_type _queue_iterator) const {
    boost::system::error_code ec;
    return _queue_iterator->first.address().to_string(ec) + ":"
            + std::to_string(_queue_iterator->first.port());
}

std::string udp_server_endpoint_impl::get_remote_information(
        const endpoint_type& _remote) const {
    boost::system::error_code ec;
    return _remote.address().to_string(ec) + ":"
            + std::to_string(_remote.port());
}

bool udp_server_endpoint_impl::is_reliable() const {
    return false;
}

const std::string udp_server_endpoint_impl::get_address_port_local() const {

    std::lock_guard<std::mutex> its_lock(unicast_mutex_);
    std::string its_address_port;
    its_address_port.reserve(21);
    boost::system::error_code ec;
    if (unicast_socket_.is_open()) {
        endpoint_type its_local_endpoint = unicast_socket_.local_endpoint(ec);
        if (!ec) {
            its_address_port += its_local_endpoint.address().to_string(ec);
            its_address_port += ":";
            its_address_port += std::to_string(its_local_endpoint.port());
        }
    }
    return its_address_port;
}

bool udp_server_endpoint_impl::tp_segmentation_enabled(
        service_t _service, method_t _method) const {

    return configuration_->tp_segment_messages_service_to_client(_service,
            local_.address().to_string(),
            local_.port(), _method);
}

} // namespace vsomeip_v3
