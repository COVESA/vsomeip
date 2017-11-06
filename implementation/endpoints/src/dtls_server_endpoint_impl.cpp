// Copyright (c) 2017 by Vector Informatik GmbH
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/ip/multicast.hpp>

#include "../../configuration/include/configuration.hpp"
#include "../../logging/include/logger.hpp"
#include "../../service_discovery/include/defines.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"
#include "../include/dtls_server_endpoint_impl.hpp"
#include "../include/endpoint_definition.hpp"
#include "../include/endpoint_host.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip {

dtls_server_endpoint_impl::dtls_server_endpoint_impl(
        std::shared_ptr<endpoint_host> _host, endpoint_type _local,
        boost::asio::io_service &_io, bool _confidential, const std::vector<std::uint8_t> &_psk,
        const std::string &_pskid)
        : secure_endpoint_base(_confidential, _psk, _pskid),
          server_endpoint_impl(_host, _local, _io, get_max_dtls_payload_size()),
          socket_(_io, _local.protocol()),
          recv_buffer_(std::make_shared<message_buffer_t>(get_max_dtls_payload_size(), 0)),
          local_port_(_local.port()),
          tls_socket_(&socket_, socket_mutex_, psk_, pskid_, cipher_suite_),
          mcast_endpoint_(std::make_shared<udp_server_endpoint_impl>(_host, _local, _io)) {
    boost::system::error_code ec;

    boost::asio::socket_base::reuse_address optionReuseAddress(true);
    socket_.set_option(optionReuseAddress, ec);
    boost::asio::detail::throw_error(ec, "reuse address");

    socket_.bind(_local, ec);
    boost::asio::detail::throw_error(ec, "bind");

    if (_local.address().is_v4()) {
        boost::asio::ip::address_v4 its_unicast_address =
        _host->get_configuration()->get_unicast_address().to_v4();
        boost::asio::ip::multicast::outbound_interface option(its_unicast_address);
        socket_.set_option(option, ec);
        boost::asio::detail::throw_error(ec, "outbound interface option IPv4");
    } else if (_local.address().is_v6()) {
        boost::asio::ip::address_v6 its_unicast_address =
        _host->get_configuration()->get_unicast_address().to_v6();
        boost::asio::ip::multicast::outbound_interface option(
                static_cast<unsigned int>(its_unicast_address.scope_id()));
        socket_.set_option(option, ec);
        boost::asio::detail::throw_error(ec, "outbound interface option IPv6");
    }

#ifdef _WIN32
    const char *optval("0001");
    ::setsockopt(socket_.native(), IPPROTO_IP, IP_PKTINFO, optval,
            sizeof(optval));
#else
    int optval(1);
    ::setsockopt(socket_.native(), IPPROTO_IP, IP_PKTINFO, &optval,
            sizeof(optval));
#endif
}

dtls_server_endpoint_impl::~dtls_server_endpoint_impl() {
}

bool dtls_server_endpoint_impl::is_local() const {
    return false;
}

void dtls_server_endpoint_impl::start() {
    if (socket_.is_open()) {
        tls_socket_.async_handshake(
                std::bind(&dtls_server_endpoint_impl::handshake_cbk,
                          std::dynamic_pointer_cast<dtls_server_endpoint_impl>(shared_from_this()),
                          std::placeholders::_1));
    }
}

void dtls_server_endpoint_impl::stop() {
    server_endpoint_impl::stop();
    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        if (socket_.is_open()) {
            boost::system::error_code its_error;
            socket_.shutdown(socket_type::shutdown_both, its_error);
            socket_.close(its_error);
        }
    }
    tls_socket_.reset();
}

void dtls_server_endpoint_impl::receive() {
    if (socket_.is_open()) {
        tls_socket_.async_receive_from(
                recv_buffer_,
                remote_,
                std::bind(
                        &dtls_server_endpoint_impl::receive_cbk,
                        std::dynamic_pointer_cast<dtls_server_endpoint_impl>(
                                shared_from_this()),
                        std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3));
    }
}

void dtls_server_endpoint_impl::restart() {
    start();
}

void dtls_server_endpoint_impl::handshake_cbk(boost::system::error_code const &_error) {
    if (!_error) {
        receive();
    } else {
        restart();
    }
}

bool dtls_server_endpoint_impl::send_to(
        const std::shared_ptr<endpoint_definition> _target, const byte_t *_data,
        uint32_t _size, bool _flush) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    endpoint_type its_target(_target->get_address(), _target->get_port());

    if (its_target.address().is_multicast()) {
        return mcast_endpoint_->send_to(_target, _data, _size, _flush);
    }

    if (!tls_socket_.is_handshake_done()) {
        return false;
    }

    return send_intern(its_target, _data, _size, _flush);
}

void dtls_server_endpoint_impl::send_queued(
        queue_iterator_type _queue_iterator) {
    message_buffer_ptr_t its_buffer = _queue_iterator->second.front();
    tls_socket_.async_send_to(
            its_buffer,
            _queue_iterator->first,
            std::bind(&udp_server_endpoint_base_impl::send_cbk,
                      shared_from_this(), _queue_iterator,
                      std::placeholders::_1, std::placeholders::_2));

}

void dtls_server_endpoint_impl::join(const std::string &/*_address*/) {
    VSOMEIP_ERROR << "dtls_server_endpoint_impl::join: Invalid operation for a unicast endpoint!";
}

void dtls_server_endpoint_impl::leave(const std::string &/*_address*/) {
    VSOMEIP_ERROR << "dtls_server_endpoint_impl::leave: Invalid operation for a unicast endpoint!";
}

void dtls_server_endpoint_impl::add_default_target(service_t _service,
                                                   const std::string &_address,
                                                   uint16_t _port) {
    std::lock_guard<std::mutex> its_lock(default_targets_mutex_);
    endpoint_type its_endpoint(
            boost::asio::ip::address::from_string(_address), _port);
    default_targets_[_service] = its_endpoint;
}

void dtls_server_endpoint_impl::remove_default_target(service_t _service) {
    std::lock_guard<std::mutex> its_lock(default_targets_mutex_);
    default_targets_.erase(_service);
}

bool dtls_server_endpoint_impl::get_default_target(
        service_t _service,
        dtls_server_endpoint_impl::endpoint_type &_target) const {
    std::lock_guard<std::mutex> its_lock(default_targets_mutex_);
    bool is_valid(false);
    auto find_service = default_targets_.find(_service);
    if (find_service != default_targets_.end()) {
        _target = find_service->second;
        is_valid = true;
    }
    return is_valid;
}

unsigned short dtls_server_endpoint_impl::get_local_port() const {
    return local_port_;
}

// TODO: find a better way to structure the receive functions
void dtls_server_endpoint_impl::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes,
        boost::asio::ip::address const &_destination) {
    std::shared_ptr<endpoint_host> its_host = this->host_.lock();
    if (its_host) {
        if (!_error && 0 < _bytes) {
            std::size_t remaining_bytes = _bytes;
            std::size_t i = 0;

            const boost::asio::ip::address its_remote_address(remote_.address());
            const std::uint16_t its_remote_port(remote_.port());
            do {
                uint64_t read_message_size = utility::get_message_size(
                        &(*recv_buffer_)[i], remaining_bytes);
                if (read_message_size > max_message_size_) {
                    VSOMEIP_ERROR<< "Message size exceeds allowed maximum!";
                    return;
                }
                uint32_t current_message_size =
                        static_cast<uint32_t>(read_message_size);
                if (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE
                        && current_message_size <= remaining_bytes) {
                    if (remaining_bytes - current_message_size
                            > remaining_bytes) {
                        VSOMEIP_ERROR<< "buffer underflow in udp client endpoint ~> abort!";
                        return;
                    }
                    remaining_bytes -= current_message_size;
                    if (utility::is_request(
                            (*recv_buffer_)[i + VSOMEIP_MESSAGE_TYPE_POS])) {
                        client_t its_client;
                        std::memcpy(&its_client,
                                    &(*recv_buffer_)[i + VSOMEIP_CLIENT_POS_MIN],
                                    sizeof(client_t));
                        session_t its_session;
                        std::memcpy(&its_session,
                                    &(*recv_buffer_)[i + VSOMEIP_SESSION_POS_MIN],
                                    sizeof(session_t));
                        clients_mutex_.lock();
                        clients_[its_client][its_session] = remote_;
                        clients_mutex_.unlock();
                    }
                    service_t its_service =
                    VSOMEIP_BYTES_TO_WORD((*recv_buffer_)[i + VSOMEIP_SERVICE_POS_MIN],
                            recv_buffer_->data()[i + VSOMEIP_SERVICE_POS_MAX]);
                    if (its_service != VSOMEIP_SD_SERVICE ||
                            (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE &&
                                    current_message_size >= remaining_bytes)) {

                        its_host->on_message(&(*recv_buffer_)[i], current_message_size, this,
                                _destination, VSOMEIP_ROUTING_CLIENT,
                                its_remote_address, its_remote_port);
                    } else {
                        // ignore messages for service discovery with shorter SomeIP length
                        VSOMEIP_ERROR << "Received an unreliable vSomeIP SD message with "
                        "too short length field";
                    }
                    i += current_message_size;
                } else {
                    VSOMEIP_ERROR
                    << "Received an unreliable vSomeIP message with bad length field";
                    service_t its_service =
                    VSOMEIP_BYTES_TO_WORD((*recv_buffer_)[VSOMEIP_SERVICE_POS_MIN],
                            (*recv_buffer_)[VSOMEIP_SERVICE_POS_MAX]);
                    if (its_service != VSOMEIP_SD_SERVICE) {
                        its_host->on_error(&(*recv_buffer_)[i], (uint32_t)remaining_bytes,
                                this, its_remote_address, its_remote_port);
                    }
                    remaining_bytes = 0;
                }
            } while (remaining_bytes > 0);
            receive();
        } else {
            receive();
        }
    }
}

client_t dtls_server_endpoint_impl::get_client(
        std::shared_ptr<endpoint_definition> _endpoint) {
    endpoint_type endpoint(_endpoint->get_address(), _endpoint->get_port());
    std::lock_guard<std::mutex> its_lock(clients_mutex_);
    for (auto its_client : clients_) {
        for (auto its_session : clients_[its_client.first]) {
            if (endpoint == its_session.second) {
                // TODO: Check system byte order before convert!
                client_t client = client_t(
                        its_client.first << 8 | its_client.first >> 8);
                return client;
            }
        }
    }
    return 0;
}

}  // namespace vsomeip
