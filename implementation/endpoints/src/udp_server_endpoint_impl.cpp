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
        boost::asio::io_service &_io,
        configuration::endpoint_queue_limit_t _queue_limit)
    : server_endpoint_impl<ip::udp_ext>(
            _host, _local, _io, VSOMEIP_MAX_UDP_MESSAGE_SIZE, _queue_limit),
      socket_(_io, _local.protocol()),
      joined_group_(false),
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

bool udp_server_endpoint_impl::send_to(
    const std::shared_ptr<endpoint_definition> _target,
    const byte_t *_data, uint32_t _size, bool _flush) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    endpoint_type its_target(_target->get_address(), _target->get_port());
    return send_intern(its_target, _data, _size, _flush);
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

bool udp_server_endpoint_impl::is_joined(
        const std::string &_address, bool* _received) const {
    *_received = false;
    std::lock_guard<std::mutex> its_lock(joined_mutex_);
    const auto found_address = joined_.find(_address);
    if (found_address != joined_.end()) {
        *_received = found_address->second;
    }
    return (found_address != joined_.end());
}

void udp_server_endpoint_impl::join(const std::string &_address) {
    bool has_received(false);

    std::function<void(const std::string &)> join_func =
            [this](const std::string &_address) {
        try {
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
                joined_[_address] = false;
            }
            joined_group_ = true;
        } catch (const std::exception &e) {
            VSOMEIP_ERROR << "udp_server_endpoint_impl::join" << ":" << e.what();
        }
    };

    if (!is_joined(_address, &has_received)) {
        join_func(_address);
    } else if (!has_received) {
        // joined the multicast group but didn't receive a event yet -> rejoin
        leave(_address);
        join_func(_address);
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
                if (!joined_.size()) {
                    joined_group_ = false;
                }
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
                if (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE &&
                        current_message_size <= remaining_bytes) {
                    if (remaining_bytes - current_message_size > remaining_bytes) {
                        VSOMEIP_ERROR << "buffer underflow in udp client endpoint ~> abort!";
                        return;
                    }
                    remaining_bytes -= current_message_size;
                    service_t its_service = VSOMEIP_BYTES_TO_WORD(recv_buffer_[i + VSOMEIP_SERVICE_POS_MIN],
                            recv_buffer_[i + VSOMEIP_SERVICE_POS_MAX]);
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
                        endpoint_to_client_[remote_] = its_client;
                        clients_mutex_.unlock();
                    } else if (its_service != VSOMEIP_SD_SERVICE
                            && utility::is_notification(recv_buffer_[i + VSOMEIP_MESSAGE_TYPE_POS])
                            && joined_group_) {
                        std::lock_guard<std::mutex> its_lock(joined_mutex_);
                        boost::system::error_code ec;
                        const auto found_address = joined_.find(_destination.to_string(ec));
                        if (found_address != joined_.end()) {
                            found_address->second = true;
                        }
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
                    i += current_message_size;
                } else {
                    VSOMEIP_ERROR << "Received an unreliable vSomeIP message with bad length field";
                    if (remaining_bytes > VSOMEIP_SERVICE_POS_MAX) {
                        service_t its_service = VSOMEIP_BYTES_TO_WORD(recv_buffer_[VSOMEIP_SERVICE_POS_MIN],
                                recv_buffer_[VSOMEIP_SERVICE_POS_MAX]);
                        if (its_service != VSOMEIP_SD_SERVICE) {
                            if (read_message_size == 0) {
                                VSOMEIP_ERROR << "Ignoring unreliable vSomeIP message with SomeIP message length 0!";
                            } else {
                                its_host->on_error(&recv_buffer_[i],
                                        (uint32_t)remaining_bytes, this,
                                        its_remote_address, its_remote_port);
                            }
                        }
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

client_t udp_server_endpoint_impl::get_client(std::shared_ptr<endpoint_definition> _endpoint) {
    const endpoint_type endpoint(_endpoint->get_address(), _endpoint->get_port());
    std::lock_guard<std::mutex> its_lock(clients_mutex_);
    auto found_endpoint = endpoint_to_client_.find(endpoint);
    if (found_endpoint != endpoint_to_client_.end()) {
        // TODO: Check system byte order before convert!
        const client_t client = client_t(found_endpoint->second << 8 | found_endpoint->second >> 8);
        return client;
    }
    return 0;
}

void udp_server_endpoint_impl::print_status() {
    std::lock_guard<std::mutex> its_lock(mutex_);

    VSOMEIP_INFO << "status use: " << std::dec << local_port_
            << " number queues: " << std::dec << queues_.size()
            << " recv_buffer: " << std::dec << recv_buffer_.capacity();

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

} // namespace vsomeip
