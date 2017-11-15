// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <boost/asio/ip/multicast.hpp>

#include "../include/endpoint_definition.hpp"
#include "../include/endpoint_host.hpp"
#include "../include/udp_server_endpoint_impl.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../logging/include/logger.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"
#include "../../service_discovery/include/defines.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip {

udp_server_endpoint_impl::udp_server_endpoint_impl(
        std::shared_ptr< endpoint_host > _host,
        endpoint_type _local,
        boost::asio::io_service &_io)
    : server_endpoint_impl<ip::udp_ext>(
            _host, _local, _io, VSOMEIP_MAX_UDP_MESSAGE_SIZE),
      secoc_endpoint_base(_local.port(), _host->get_configuration()),
      socket_(_io, _local.protocol()),
      recv_buffer_(VSOMEIP_MAX_UDP_MESSAGE_SIZE, 0),
      local_port_(_local.port()) {
    boost::system::error_code ec;

    boost::asio::socket_base::reuse_address optionReuseAddress(true);
    socket_.set_option(optionReuseAddress, ec);
    boost::asio::detail::throw_error(ec, "reuse address");

    socket_.bind(_local, ec);
    boost::asio::detail::throw_error(ec, "bind");

    if (_local.address().is_v4()) {
        boost::asio::ip::address_v4 its_unicast_address
            = _host->get_configuration()->get_unicast_address().to_v4();
        boost::asio::ip::multicast::outbound_interface option(its_unicast_address);
        socket_.set_option(option, ec);
        boost::asio::detail::throw_error(ec, "outbound interface option IPv4");
    } else if (_local.address().is_v6()) {
        boost::asio::ip::address_v6 its_unicast_address
            = _host->get_configuration()->get_unicast_address().to_v6();
        boost::asio::ip::multicast::outbound_interface option(
                static_cast<unsigned int>(its_unicast_address.scope_id()));
        socket_.set_option(option, ec);
        boost::asio::detail::throw_error(ec, "outbound interface option IPv6");
    }

    boost::asio::socket_base::broadcast option(true);
    socket_.set_option(option, ec);
    boost::asio::detail::throw_error(ec, "broadcast option");

#ifdef _WIN32
    const char* optval("0001");
    ::setsockopt(socket_.native(), IPPROTO_IP, IP_PKTINFO,
        optval, sizeof(optval));
#else
    int optval(1);
    ::setsockopt(socket_.native(), IPPROTO_IP, IP_PKTINFO,
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
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        if (socket_.is_open()) {
            boost::system::error_code its_error;
            socket_.shutdown(socket_type::shutdown_both, its_error);
            socket_.close(its_error);
        }
    }
}

void udp_server_endpoint_impl::receive() {
    std::lock_guard<std::mutex> its_lock(socket_mutex_);
    if(socket_.is_open()) {
        socket_.async_receive_from(
                boost::asio::buffer(&recv_buffer_[0], max_message_size_),
            remote_,
            std::bind(
                &udp_server_endpoint_impl::receive_cbk,
                std::dynamic_pointer_cast<
                    udp_server_endpoint_impl >(shared_from_this()),
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3
            )
        );
    }
}

void udp_server_endpoint_impl::restart() {
    receive();
}

bool udp_server_endpoint_impl::send(const uint8_t *_data, uint32_t _size, bool _flush) {
    if (VSOMEIP_SESSION_POS_MAX < _size) {
        message_buffer_t buffer;
        service_t its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
        method_t its_method = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);
        instance_t its_instance = get_instance(its_service);
        if (secoc_endpoint_base::is_secured(its_service, its_instance, its_method)) {
            secoc_endpoint_base::authenticate(_data, _size, its_service, its_instance, its_method, buffer);
            return udp_server_endpoint_base_impl::send(buffer.data(), static_cast<uint32_t>(buffer.size()), _flush);
        }
    }

    return udp_server_endpoint_base_impl::send(_data, _size, _flush);
}

bool udp_server_endpoint_impl::send_to(
    const std::shared_ptr<endpoint_definition> _target,
    const byte_t *_data, uint32_t _size, bool _flush) {

    message_buffer_t buffer;
    service_t its_service = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_SERVICE_POS_MIN], _data[VSOMEIP_SERVICE_POS_MAX]);
    method_t its_method = VSOMEIP_BYTES_TO_WORD(_data[VSOMEIP_METHOD_POS_MIN], _data[VSOMEIP_METHOD_POS_MAX]);
    instance_t its_instance = get_instance(its_service);

    std::lock_guard<std::mutex> its_lock(mutex_);
    endpoint_type its_target(_target->get_address(), _target->get_port());
    if (secoc_endpoint_base::is_secured(its_service, its_instance, its_method)) {
        secoc_endpoint_base::authenticate(_data, _size, its_service, its_instance, its_method, buffer);
        return send_intern(its_target, buffer.data(), static_cast<uint32_t>(buffer.size()), _flush);
    }

    return send_intern(its_target, _data, _size, _flush);
}

void udp_server_endpoint_impl::send_queued(
        queue_iterator_type _queue_iterator) {
    message_buffer_ptr_t its_buffer = _queue_iterator->second.front();
#if 0
        std::stringstream msg;
        msg << "usei::sq(" << _queue_iterator->first.address().to_string() << ":"
            << _queue_iterator->first.port() << "): ";
        for (std::size_t i = 0; i < its_buffer->size(); ++i)
            msg << std::hex << std::setw(2) << std::setfill('0')
                << (int)(*its_buffer)[i] << " ";
        VSOMEIP_INFO << msg.str();
#endif
    {
        std::lock_guard<std::mutex> its_lock(socket_mutex_);
        socket_.async_send_to(
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
}

bool udp_server_endpoint_impl::is_joined(const std::string &_address) const {
    std::lock_guard<std::mutex> its_lock(joined_mutex_);
    return (joined_.find(_address) != joined_.end());
}

void udp_server_endpoint_impl::join(const std::string &_address) {

    try {
        if (!is_joined(_address)) {
            bool is_v4(false);
            bool is_v6(false);
            {
                std::lock_guard<std::mutex> its_lock(local_mutex_);
                is_v4 = local_.address().is_v4();
                is_v6 = local_.address().is_v6();
            }
            if (is_v4) {
                std::lock_guard<std::mutex> its_lock(socket_mutex_);
                socket_.set_option(ip::udp_ext::socket::reuse_address(true));
                socket_.set_option(
                    boost::asio::ip::multicast::enable_loopback(false));
#ifdef _WIN32
                socket_.set_option(boost::asio::ip::multicast::join_group(
                    boost::asio::ip::address::from_string(_address).to_v4(),
                    local_.address().to_v4()));
#else
                socket_.set_option(boost::asio::ip::multicast::join_group(
                    boost::asio::ip::address::from_string(_address).to_v4()));
#endif
            } else if (is_v6) {
                std::lock_guard<std::mutex> its_lock(socket_mutex_);
                socket_.set_option(ip::udp_ext::socket::reuse_address(true));
                socket_.set_option(
                    boost::asio::ip::multicast::enable_loopback(false));
#ifdef _WIN32
                socket_.set_option(boost::asio::ip::multicast::join_group(
                    boost::asio::ip::address::from_string(_address).to_v6(),
                    local_.address().to_v6().scope_id()));
#else
                socket_.set_option(boost::asio::ip::multicast::join_group(
                    boost::asio::ip::address::from_string(_address).to_v6()));
#endif
            }
            {
                std::lock_guard<std::mutex> its_lock(joined_mutex_);
                joined_.insert(_address);
            }
        } else {
            VSOMEIP_INFO << "udp_server_endpoint_impl::join: "
                    "Trying to join already joined address: " << _address;
        }
    }
    catch (const std::exception &e) {
        VSOMEIP_ERROR << __func__ << ":" << e.what();
    }
}

void udp_server_endpoint_impl::leave(const std::string &_address) {
    try {
        if (is_joined(_address)) {
            bool is_v4(false);
            bool is_v6(false);
            {
                std::lock_guard<std::mutex> its_lock(local_mutex_);
                is_v4 = local_.address().is_v4();
                is_v6 = local_.address().is_v6();
            }
            if (is_v4) {
                std::lock_guard<std::mutex> its_lock(socket_mutex_);
                socket_.set_option(boost::asio::ip::multicast::leave_group(
                    boost::asio::ip::address::from_string(_address)));
            } else if (is_v6) {
                std::lock_guard<std::mutex> its_lock(socket_mutex_);
                socket_.set_option(boost::asio::ip::multicast::leave_group(
                    boost::asio::ip::address::from_string(_address)));
            }
            {
                std::lock_guard<std::mutex> its_lock(joined_mutex_);
                joined_.erase(_address);
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

unsigned short udp_server_endpoint_impl::get_local_port() const {
    return local_port_;
}

// TODO: find a better way to structure the receive functions
void udp_server_endpoint_impl::receive_cbk(
        boost::system::error_code const &_error, std::size_t _bytes,
        boost::asio::ip::address const &_destination) {
#if 0
    std::stringstream msg;
    msg << "usei::rcb(" << _error.message() << "): ";
    for (std::size_t i = 0; i < _bytes; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0')
            << (int) recv_buffer_[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif
    std::shared_ptr<endpoint_host> its_host = this->host_.lock();
    if (its_host) {
        if (!_error && 0 < _bytes) {
            std::size_t remaining_bytes = _bytes;
            std::size_t i = 0;
            const boost::asio::ip::address its_remote_address(remote_.address());
            const std::uint16_t its_remote_port(remote_.port());
            do {
                uint64_t read_message_size
                    = utility::get_message_size(&this->recv_buffer_[i],
                            remaining_bytes);
                if (read_message_size > MESSAGE_SIZE_UNLIMITED) {
                    VSOMEIP_ERROR << "Message size exceeds allowed maximum!";
                    return;
                }
                uint32_t current_message_size = static_cast<uint32_t>(read_message_size);
                uint32_t current_frame_size = current_message_size;
                if (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE &&
                        current_message_size <= remaining_bytes) {
                    if (remaining_bytes - current_message_size > remaining_bytes) {
                        VSOMEIP_ERROR << "buffer underflow in udp client endpoint ~> abort!";
                        return;
                    }
                    service_t its_service = VSOMEIP_BYTES_TO_WORD(recv_buffer_[i + VSOMEIP_SERVICE_POS_MIN],
                                                                  recv_buffer_[i + VSOMEIP_SERVICE_POS_MAX]);
                    method_t its_method = VSOMEIP_BYTES_TO_WORD(recv_buffer_[i + VSOMEIP_METHOD_POS_MIN],
                                                                recv_buffer_[i + VSOMEIP_METHOD_POS_MAX]);
                    instance_t its_instance = get_instance(its_service);

                    bool is_verified{true};
                    if (secoc_endpoint_base::is_secured(its_service, its_instance, its_method)) {
                        current_frame_size += trailer_size;
                        if (current_frame_size > remaining_bytes) {
                            VSOMEIP_ERROR << "Incomplete SecOC message!";
                            return;
                        }

                        message_buffer_t buffer;
                        is_verified = secoc_endpoint_base::verify(&recv_buffer_[i], current_frame_size, its_service,
                                                               its_instance, its_method, buffer);
                    }
                    remaining_bytes -= current_frame_size;
                    if (is_verified) {
                        if (utility::is_request(
                                recv_buffer_[i + VSOMEIP_MESSAGE_TYPE_POS])) {
                            client_t its_client;
                            std::memcpy(&its_client,
                                &recv_buffer_[i + VSOMEIP_CLIENT_POS_MIN],
                                sizeof(client_t));
                            session_t its_session;
                            std::memcpy(&its_session,
                                &recv_buffer_[i + VSOMEIP_SESSION_POS_MIN],
                                sizeof(session_t));
                            clients_mutex_.lock();
                            clients_[its_client][its_session] = remote_;
                            clients_mutex_.unlock();
                        }

                        if (its_service != VSOMEIP_SD_SERVICE ||
                            (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE &&
                                    current_message_size >= remaining_bytes)) {
                            its_host->on_message(&recv_buffer_[i],
                                    current_message_size, this, _destination,
                                    VSOMEIP_ROUTING_CLIENT, its_remote_address,
                                    its_remote_port);
                        } else {
                            //ignore messages for service discovery with shorter SomeIP length
                            VSOMEIP_ERROR << "Received an unreliable vSomeIP SD message with too short length field";
                        }
                    }
                    i += current_frame_size;
                } else {
                    VSOMEIP_ERROR << "Received an unreliable vSomeIP message with bad length field";
                    service_t its_service = VSOMEIP_BYTES_TO_WORD(recv_buffer_[VSOMEIP_SERVICE_POS_MIN],
                            recv_buffer_[VSOMEIP_SERVICE_POS_MAX]);
                    if (its_service != VSOMEIP_SD_SERVICE) {
                        its_host->on_error(&recv_buffer_[i],
                                (uint32_t)remaining_bytes, this,
                                its_remote_address, its_remote_port);
                    }
                    remaining_bytes = 0;
                }
            } while (remaining_bytes > 0);
            restart();
        } else {
            receive();
        }
    }
}

client_t udp_server_endpoint_impl::get_client(std::shared_ptr<endpoint_definition> _endpoint) {
    endpoint_type endpoint(_endpoint->get_address(), _endpoint->get_port());
    std::lock_guard<std::mutex> its_lock(clients_mutex_);
    for (auto its_client : clients_) {
        for (auto its_session : clients_[its_client.first]) {
            if (endpoint == its_session.second) {
                // TODO: Check system byte order before convert!
                client_t client = client_t(its_client.first << 8 | its_client.first >> 8);
                return client;
            }
        }
    }
    return 0;
}

instance_t udp_server_endpoint_impl::get_instance(service_t _service) const {
    auto host = host_.lock();
    if (_service == VSOMEIP_SD_SERVICE)  {
        return VSOMEIP_SD_INSTANCE;
    }

    return host->get_instance(_service, const_cast<udp_server_endpoint_impl*>(this));
}



} // namespace vsomeip
