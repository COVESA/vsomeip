// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/ip/multicast.hpp>

#include <vsomeip/logger.hpp>

#include "../include/endpoint_definition.hpp"
#include "../include/endpoint_host.hpp"
#include "../include/udp_server_endpoint_impl.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip {

udp_server_endpoint_impl::udp_server_endpoint_impl(
        std::shared_ptr< endpoint_host > _host,
        endpoint_type _local,
        boost::asio::io_service &_io)
    : server_endpoint_impl<
          ip::udp, VSOMEIP_MAX_UDP_MESSAGE_SIZE
      >(_host, _local, _io),
      socket_(_io, _local.protocol()) {
    boost::system::error_code ec;

    boost::asio::socket_base::reuse_address optionReuseAddress(true);
    socket_.set_option(optionReuseAddress);

    socket_.bind(_local, ec);
    boost::asio::detail::throw_error(ec, "bind");

    boost::asio::socket_base::broadcast option(true);
    socket_.set_option(option);
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
    if (socket_.is_open())
        socket_.close();
}

void udp_server_endpoint_impl::receive() {
    packet_buffer_ptr_t its_buffer
        = std::make_shared< packet_buffer_t >();
    socket_.async_receive_from(
        boost::asio::buffer(*its_buffer),
        remote_,
        std::bind(
            &udp_server_endpoint_impl::receive_cbk,
            std::dynamic_pointer_cast<
                udp_server_endpoint_impl >(shared_from_this()),
            its_buffer,
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

void udp_server_endpoint_impl::restart() {
    receive();
}

bool udp_server_endpoint_impl::send_to(
    const std::shared_ptr<endpoint_definition> _target,
    const byte_t *_data, uint32_t _size, bool _flush) {
  endpoint_type its_target(_target->get_address(), _target->get_port());
  return send_intern(its_target, _data, _size, _flush);
}

void udp_server_endpoint_impl::send_queued(
        endpoint_type _target, message_buffer_ptr_t _buffer) {
#if 0
        std::stringstream msg;
        msg << "usei::sq(" << _target.address().to_string() << ":"
            << _target.port() << "): ";
        for (std::size_t i = 0; i < _buffer->size(); ++i)
            msg << std::hex << std::setw(2) << std::setfill('0')
                << (int)(*_buffer)[i] << " ";
        VSOMEIP_DEBUG << msg.str();
#endif
     socket_.async_send_to(
        boost::asio::buffer(*_buffer),
        _target,
        std::bind(
            &udp_server_endpoint_base_impl::send_cbk,
            shared_from_this(),
            _buffer,
            std::placeholders::_1,
            std::placeholders::_2
        )
    );
}

udp_server_endpoint_impl::endpoint_type
udp_server_endpoint_impl::get_remote() const {
    return remote_;
}

bool udp_server_endpoint_impl::get_multicast(service_t _service, event_t _event,
        udp_server_endpoint_impl::endpoint_type &_target) const {
    bool is_valid(false);
    auto find_service = multicasts_.find(_service);
    if (find_service != multicasts_.end()) {
        auto find_event = find_service->second.find(_event);
        if (find_event != find_service->second.end()) {
            _target = find_event->second;
            is_valid = true;
        }
    }
    return is_valid;
}

void udp_server_endpoint_impl::join(const std::string &_address) {
    if (local_.address().is_v4()) {
        try {
            socket_.set_option(
                boost::asio::ip::udp::socket::reuse_address(true));
            socket_.set_option(
                boost::asio::ip::multicast::enable_loopback(false));
            socket_.set_option(boost::asio::ip::multicast::join_group(
                boost::asio::ip::address::from_string(_address).to_v4()));
        }
        catch (const std::exception &e) {
            VSOMEIP_ERROR << e.what();
        }
    } else {
        // TODO: support multicast for IPv6
    }
}

void udp_server_endpoint_impl::leave(const std::string &_address) {
    if (local_.address().is_v4()) {
        try {
            socket_.set_option(boost::asio::ip::multicast::leave_group(
                boost::asio::ip::address::from_string(_address)));
        }
        catch (...) {

        }
    } else {
        // TODO: support multicast for IPv6
    }
}

void udp_server_endpoint_impl::add_multicast(
        service_t _service, instance_t _instance,
        const std::string &_address, uint16_t _port) {
    endpoint_type its_endpoint(
        boost::asio::ip::address::from_string(_address), _port);
    multicasts_[_service][_instance] = its_endpoint;
}

void udp_server_endpoint_impl::remove_multicast(
        service_t _service, instance_t _instance) {
    auto found_service = multicasts_.find(_service);
    if (found_service != multicasts_.end()) {
        auto found_instance = found_service->second.find(_instance);
        if (found_instance != found_service->second.end()) {
            found_service->second.erase(_instance);
        }
    }
}

unsigned short udp_server_endpoint_impl::get_local_port() const {
    return socket_.local_endpoint().port();
}

// TODO: find a better way to structure the receive functions
void udp_server_endpoint_impl::receive_cbk(
        packet_buffer_ptr_t _buffer,
        boost::system::error_code const &_error, std::size_t _bytes) {
#if 0
    std::stringstream msg;
    msg << "usei::rcb(" << _error.message() << "): ";
    for (std::size_t i = 0; i < _bytes; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0')
            << (int)(*_buffer)[i] << " ";
    VSOMEIP_DEBUG << msg.str();
#endif
    std::shared_ptr<endpoint_host> its_host = this->host_.lock();
    if (its_host) {
        if (!_error && 0 < _bytes) {
            message_.insert(message_.end(), _buffer->begin(),
                            _buffer->begin() + _bytes);

            bool has_full_message;
            do {
                uint32_t current_message_size
                    = utility::get_message_size(message_);
                has_full_message = (current_message_size > 0
                    && current_message_size <= message_.size());
                if (has_full_message) {
                    if (utility::is_request(
                            message_[VSOMEIP_MESSAGE_TYPE_POS])) {
                        client_t its_client;
                        std::memcpy(&its_client,
                            &message_[VSOMEIP_CLIENT_POS_MIN],
                            sizeof(client_t));
                        session_t its_session;
                        std::memcpy(&its_session,
                            &message_[VSOMEIP_SESSION_POS_MIN],
                            sizeof(session_t));
                        clients_[its_client][its_session] = remote_;
                    }

                    its_host->on_message(&message_[0],
                        current_message_size, this);
                    message_.erase(message_.begin(),
                        message_.begin() + current_message_size);
                }
            } while (has_full_message);

            restart();
        } else {
            receive();
        }
    }
}

client_t udp_server_endpoint_impl::get_client(std::shared_ptr<endpoint_definition> _endpoint) {
    endpoint_type endpoint(_endpoint->get_address(), _endpoint->get_port());
    for (auto its_client : clients_) {
        for (auto its_session : clients_[its_client.first]) {
            if (endpoint == its_session.second) {
                // TODO: Check system byte order before convert!
                client_t client = its_client.first << 8 | its_client.first >> 8;
                return client;
            }
        }
    }
    return 0;
}

} // namespace vsomeip
