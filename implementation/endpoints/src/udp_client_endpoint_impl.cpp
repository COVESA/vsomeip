// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>
#include <thread>

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/multicast.hpp>

#include <vsomeip/internal/logger.hpp>

#include "../include/boardnet_endpoint_host.hpp"
#include "../include/tp.hpp"
#include "../../routing/include/routing_host.hpp"
#include "../include/udp_client_endpoint_impl.hpp"
#include "../include/udp_socket.hpp"
#include "../../utility/include/utility.hpp"
#include "../../utility/include/bithelper.hpp"

#define VSOMEIP_LOG_PREFIX "ucei"

namespace vsomeip_v3 {

udp_client_endpoint_impl::udp_client_endpoint_impl(const std::shared_ptr<boardnet_endpoint_host>& _boardnet_endpoint_host,
                                                   const std::shared_ptr<routing_host>& _routing_host, const endpoint_type& _local,
                                                   const endpoint_type& _remote, boost::asio::io_context& _io,
                                                   const std::shared_ptr<configuration>& _configuration) :
    udp_client_endpoint_base_impl(_boardnet_endpoint_host, _routing_host, _local, _remote, _io, _configuration),
    remote_address_(_remote.address()), remote_port_(_remote.port()),
    udp_receive_buffer_size_(_configuration->get_udp_receive_buffer_size()),
    tp_reassembler_(std::make_shared<tp::tp_reassembler>(_configuration->get_max_message_size_unreliable(), _io)) {
    is_supporting_someip_tp_ = true;

    this->max_message_size_ = VSOMEIP_MAX_UDP_MESSAGE_SIZE;
    this->queue_limit_ = _configuration->get_endpoint_queue_limit(_remote.address().to_string(), _remote.port());
}

udp_client_endpoint_impl::~udp_client_endpoint_impl() {
    tp_reassembler_->stop();
}

bool udp_client_endpoint_impl::is_local() const {
    return false;
}

void udp_client_endpoint_impl::connect() {
    std::unique_lock<std::mutex> its_lock(socket_mutex_);
    boost::system::error_code its_error;
    socket_->open(remote_.protocol(), its_error);
    if (!its_error || its_error == boost::asio::error::already_open) {
        // Enable SO_REUSEADDR to avoid bind problems with services going offline
        // and coming online again and the user has specified only a small number
        // of ports in the clients section for one service instance
        socket_->set_option(boost::asio::socket_base::reuse_address(true), its_error);
        if (its_error) {
            VSOMEIP_WARNING_P << "Couldn't enable SO_REUSEADDR: " << its_error.message() << " remote:" << get_address_port_remote();
        }
        socket_->set_option(boost::asio::socket_base::receive_buffer_size(static_cast<int>(udp_receive_buffer_size_)), its_error);
        if (its_error) {
            VSOMEIP_WARNING_P << "Couldn't set SO_RCVBUF: " << its_error.message() << " to: " << udp_receive_buffer_size_
                              << " local port:" << local_.port() << " remote:" << get_address_port_remote();
        }

        boost::asio::socket_base::receive_buffer_size its_option;
        socket_->get_option(its_option, its_error);
#ifdef __linux__
        // If regular setting of the buffer size did not work, try to force
        // (requires CAP_NET_ADMIN to be successful)
        if (its_option.value() < 0 || its_option.value() < udp_receive_buffer_size_) {
            socket_->set_option(udp_receive_buffer_force{udp_receive_buffer_size_}, its_error);
            if (!its_error) {
                VSOMEIP_INFO_P << "SO_RCVBUFFORCE successful!";
            } else {
                VSOMEIP_INFO_P << "SO_RCVBUFFORCE failed, " << its_error.message();
                its_error = {};
            }
            socket_->get_option(its_option, its_error);
        }
#endif
        if (its_error) {
            VSOMEIP_WARNING_P << "Couldn't get SO_RCVBUF: " << its_error.message() << " local port:" << local_.port()
                              << " remote:" << get_address_port_remote();
        } else {
            VSOMEIP_INFO_P << "SO_RCVBUF is: " << its_option.value() << " (" << udp_receive_buffer_size_ << ")"
                           << " local port:" << local_.port() << " remote:" << get_address_port_remote();
        }

        if (local_.port() == ILLEGAL_PORT) {
            // Let the OS assign the port
            local_.port(0);
        }

#if defined(__linux__) || defined(__QNX__)
        // If specified, bind to device
        std::string its_device(configuration_->get_device());
        if (its_device != "") {
            boost::system::error_code its_ec{};
            socket_->set_option(udp_bind_to_device{its_device}, its_ec);
            if (its_ec) {
                VSOMEIP_WARNING_P << "Could not bind to device \"" << its_device << "\", " << its_ec.message();
            }
        }
#endif

        // In case a client endpoint port was configured,
        // bind to it before connecting
        if (local_.port() != ILLEGAL_PORT) {
            boost::system::error_code its_bind_error;
            socket_->bind(local_, its_bind_error);
            if (its_bind_error) {
                VSOMEIP_WARNING_P << "Error binding socket: " << its_bind_error.message() << " local: " << local_.address().to_string()
                                  << ":" << local_.port() << " remote:" << get_address_port_remote();

                its_lock.unlock();

                if (std::shared_ptr<boardnet_endpoint_host> its_host = endpoint_host_.lock(); its_host) {
                    uint16_t local_port = ILLEGAL_PORT;
                    // set new client port depending on service / instance / remote port
                    if (!its_host->on_bind_error(shared_from_this(), remote_address_, remote_port_, local_port)) {
                        VSOMEIP_WARNING_P << "Failed to set new local port for uce: local: " << local_.address().to_string() << ":"
                                          << local_.port() << " remote:" << get_address_port_remote();
                    } else {
                        local_.port(local_port);
                        VSOMEIP_INFO_P << "Using new new local port for uce: local: " << local_.address().to_string() << ":"
                                       << local_.port() << " remote:" << get_address_port_remote();
                    }
                }

                try {
                    // don't connect on bind error to avoid using a random port
                    boost::asio::post(strand_, std::bind(&client_endpoint_impl::connect_cbk, shared_from_this(), its_bind_error));
                } catch (const std::exception& e) {
                    VSOMEIP_ERROR_P << e.what() << " remote:" << get_address_port_remote();
                }
                return;
            }
        }

        // update port, in case of random port allocation
        {
            boost::system::error_code err;
            endpoint_type local = socket_->local_endpoint(err);
            if (!err) {
                local_.port(local.port());
            } else {
                VSOMEIP_WARNING_P << "Could not get local port due to err (" << its_error.value() << "): " << its_error.message();
            }
        }

        state_ = cei_state_e::CONNECTING;
        auto self = std::dynamic_pointer_cast<udp_client_endpoint_impl>(shared_from_this());
        socket_->async_connect(remote_, boost::asio::bind_executor(strand_, [self](const auto& _error) { self->connect_cbk(_error); }));
    } else {
        VSOMEIP_WARNING_P << "Error opening socket: " << its_error.message() << " remote:" << get_address_port_remote();
        boost::asio::post(strand_, std::bind(&udp_client_endpoint_base_impl::connect_cbk, shared_from_this(), its_error));
    }
}

void udp_client_endpoint_impl::start() {
    connect();
}

void udp_client_endpoint_impl::restart(bool _force) {
    if (!_force && state_ == cei_state_e::CONNECTING) {
        return;
    }
    {
        std::lock_guard<std::recursive_mutex> its_lock(mutex_);
        queue_.clear();
    }
    std::string local;
    {
        std::scoped_lock its_lock(socket_mutex_);
        local = get_address_port_local();
    }
    was_not_connected_ = true;
    is_sending_ = false;
    reconnect_counter_ = 0;
    VSOMEIP_WARNING_P << "local: " << local << " remote: " << get_address_port_remote();
    close_socket(false, false);
    state_ = cei_state_e::CONNECTING;
    start_connect_timer();
}

void udp_client_endpoint_impl::send_queued(std::pair<message_buffer_ptr_t, uint32_t>& _entry) {
    std::scoped_lock its_socket_lock(socket_mutex_);

    // Check whether we need to wait (SOME/IP-TP separation time)
    if (_entry.second > 0) {
        if (last_sent_ != std::chrono::steady_clock::time_point()) {
            const auto its_elapsed =
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - last_sent_).count();
            if (_entry.second > its_elapsed)
                std::this_thread::sleep_for(std::chrono::microseconds(_entry.second - its_elapsed));
        }
        last_sent_ = std::chrono::steady_clock::now();
    } else {
        last_sent_ = std::chrono::steady_clock::time_point();
    }
    // Send
    socket_->async_send(boost::asio::buffer(*_entry.first),
                        std::bind(&udp_client_endpoint_base_impl::send_cbk, shared_from_this(), std::placeholders::_1,
                                  std::placeholders::_2, _entry.first));
}

void udp_client_endpoint_impl::get_configured_times_from_endpoint(service_t _service, method_t _method,
                                                                  std::chrono::nanoseconds* _debouncing,
                                                                  std::chrono::nanoseconds* _maximum_retention) const {
    configuration_->get_configured_timing_requests(_service, remote_address_.to_string(), remote_port_, _method, _debouncing,
                                                   _maximum_retention);
}

void udp_client_endpoint_impl::receive() {
    std::scoped_lock its_lock(socket_mutex_);
    if (!socket_->is_open()) {
        return;
    }
    auto its_buffer = std::make_shared<message_buffer_t>(VSOMEIP_UDP_BUFFER_SIZE, 0);
    auto its_sender_endpoint = std::make_shared<endpoint_type>();
    auto self = std::dynamic_pointer_cast<udp_client_endpoint_impl>(shared_from_this());
    socket_->async_receive_from(
            boost::asio::buffer(*its_buffer), *its_sender_endpoint,
            boost::asio::bind_executor(strand_, [self, its_buffer, its_sender_endpoint](const auto& _error, auto _bytes) {
                self->receive_cbk(_error, _bytes, its_buffer);
            }));
}

bool udp_client_endpoint_impl::get_remote_address(boost::asio::ip::address& _address) const {
    if (remote_address_.is_unspecified()) {
        return false;
    }
    _address = remote_address_;
    return true;
}

std::uint16_t udp_client_endpoint_impl::get_local_port() const {
    std::scoped_lock its_lock(socket_mutex_);

    return local_.port();
}

std::uint16_t udp_client_endpoint_impl::get_remote_port() const {
    return remote_port_;
}

void udp_client_endpoint_impl::receive_cbk(boost::system::error_code const& _error, std::size_t _bytes,
                                           const message_buffer_ptr_t& _recv_buffer) {
    if (_error == boost::asio::error::operation_aborted) {
        // endpoint was stopped
        return;
    }

    // reject UDP packets larger than 1416 (16 bytes full header + 1400 payload); see Section 4.1.2.9 "Payload" in AUTOSAR FO R22-11
    // "With UDP the SOME/IP payload shall be between 0 and 1400 Bytes. The limitation to 1400
    // Bytes is needed in order to allow for future changes to protocol stack (e.g. changing to
    // IPv6 or adding security means)"
    if (_bytes > VSOMEIP_MAX_UDP_MESSAGE_SIZE) {
        VSOMEIP_ERROR_P << "Received a packet that is bigger than VSOMEIP_MAX_UDP_MESSAGE_SIZE (" << VSOMEIP_MAX_UDP_MESSAGE_SIZE
                        << ") bytes with " << _bytes << " bytes in " << local_.address() << ":" << get_local_port() << " from "
                        << remote_.address() << ":" << remote_.port() << ". Message will be dropped";
        receive();
        return;
    } else if (_bytes < VSOMEIP_FULL_HEADER_SIZE) {
        VSOMEIP_ERROR << "ucei::" << __func__ << ": Dropping packet that is smaller than VSOMEIP_FULL_HEADER_SIZE (16). size=" << _bytes
                      << " remote=" << remote_;

        receive();
        return;
    }

    std::shared_ptr<routing_host> its_host = routing_host_.lock();
    if (!_error && 0 < _bytes && its_host) {
        std::size_t remaining_bytes = _bytes;
        std::size_t i = 0;

        do {
            uint64_t read_message_size = utility::get_message_size(&(*_recv_buffer)[i], remaining_bytes);
            if (read_message_size > MESSAGE_SIZE_UNLIMITED) {
                VSOMEIP_ERROR_P << "Message size exceeds allowed maximum!";
                receive();
                return;
            }

            uint32_t current_message_size = static_cast<uint32_t>(read_message_size);
            if (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE && current_message_size <= remaining_bytes) {
                if (remaining_bytes - current_message_size > remaining_bytes) {
                    VSOMEIP_ERROR_P << "Buffer underflow in udp client endpoint ~> abort!";
                    receive();
                    return;
                } else if (current_message_size > VSOMEIP_RETURN_CODE_POS
                           && ((*_recv_buffer)[i + VSOMEIP_PROTOCOL_VERSION_POS] != VSOMEIP_PROTOCOL_VERSION
                               || !utility::is_valid_message_type(tp::tp::tp_flag_unset((*_recv_buffer)[i + VSOMEIP_MESSAGE_TYPE_POS]))
                               || !utility::is_valid_return_code(
                                       static_cast<return_code_e>((*_recv_buffer)[i + VSOMEIP_RETURN_CODE_POS])))) {
                    if ((*_recv_buffer)[i + VSOMEIP_PROTOCOL_VERSION_POS] != VSOMEIP_PROTOCOL_VERSION) {
                        VSOMEIP_ERROR_P << "Wrong protocol version: 0x" << hex2((*_recv_buffer)[i + VSOMEIP_PROTOCOL_VERSION_POS])
                                        << " local: " << get_address_port_local() << " remote: " << get_address_port_remote();
                        // ensure to send back a message w/ wrong protocol version
                        its_host->on_message(&(*_recv_buffer)[i], VSOMEIP_SOMEIP_HEADER_SIZE + 8, this, false, VSOMEIP_ROUTING_CLIENT,
                                             nullptr, remote_address_, remote_port_);
                    } else if (!utility::is_valid_message_type(tp::tp::tp_flag_unset((*_recv_buffer)[i + VSOMEIP_MESSAGE_TYPE_POS]))) {
                        VSOMEIP_ERROR_P << "Invalid message type: 0x" << hex2((*_recv_buffer)[i + VSOMEIP_MESSAGE_TYPE_POS])
                                        << " local: " << get_address_port_local() << " remote: " << get_address_port_remote();
                    } else if (!utility::is_valid_return_code(static_cast<return_code_e>((*_recv_buffer)[i + VSOMEIP_RETURN_CODE_POS]))) {
                        VSOMEIP_ERROR_P << "Invalid return code: 0x" << hex2((*_recv_buffer)[i + VSOMEIP_RETURN_CODE_POS])
                                        << " local: " << get_address_port_local() << " remote: " << get_address_port_remote();
                    }

                    receive();
                    return;
                } else if (tp::tp::tp_flag_is_set((*_recv_buffer)[i + VSOMEIP_MESSAGE_TYPE_POS])) {
                    const auto res =
                            tp_reassembler_->process_tp_message(&(*_recv_buffer)[i], current_message_size, remote_address_, remote_port_);
                    if (res.first) {
                        its_host->on_message(&res.second[0], static_cast<std::uint32_t>(res.second.size()), this, false,
                                             VSOMEIP_ROUTING_CLIENT, nullptr, remote_address_, remote_port_);
                    }
                } else {
                    its_host->on_message(&(*_recv_buffer)[i], current_message_size, this, false, VSOMEIP_ROUTING_CLIENT, nullptr,
                                         remote_address_, remote_port_);
                }
                remaining_bytes -= current_message_size;
            } else {
                VSOMEIP_ERROR_P << "Received a unreliable SomeIP message with bad length field. Message size: " << current_message_size
                                << " Bytes. From: " << remote_.address() << ":" << remote_.port() << ". Dropping message.";
                remaining_bytes = 0;
            }
            i += current_message_size;
        } while (remaining_bytes > 0);
    }

    if (!_error) {
        receive();
    } else {
        if (_error == boost::asio::error::connection_refused) {
            VSOMEIP_WARNING_P << "local: " << get_address_port_local() << " remote: " << get_address_port_remote()
                              << " error: " << _error.message();
            std::shared_ptr<boardnet_endpoint_host> its_ep_host = endpoint_host_.lock();
            its_ep_host->on_disconnect(shared_from_this());
            restart(false);
        } else {
            receive();
        }
    }
}

std::string udp_client_endpoint_impl::get_address_port_remote() const {
    std::string its_address_port;
    its_address_port.reserve(21);
    boost::asio::ip::address its_address;
    if (get_remote_address(its_address)) {
        its_address_port += its_address.to_string();
    }
    its_address_port += ":";
    its_address_port += std::to_string(remote_port_);
    return its_address_port;
}

std::string udp_client_endpoint_impl::get_address_port_local() const {
    std::string its_address_port;
    its_address_port.reserve(21);
    boost::system::error_code ec;
    if (socket_->is_open()) {
        endpoint_type its_local_endpoint = socket_->local_endpoint(ec);
        if (!ec) {
            its_address_port += its_local_endpoint.address().to_string();
            its_address_port += ":";
            its_address_port.append(std::to_string(its_local_endpoint.port()));
        }
    }
    return its_address_port;
}

void udp_client_endpoint_impl::print_status() {
    std::size_t its_data_size(0);
    std::size_t its_queue_size(0);
    {
        std::lock_guard<std::recursive_mutex> its_lock(mutex_);
        its_queue_size = queue_.size();
        its_data_size = queue_size_;
    }
    std::string local;
    {
        std::scoped_lock its_lock(socket_mutex_);
        local = get_address_port_local();
    }

    VSOMEIP_INFO << "status uce: " << local << " -> " << get_address_port_remote() << " queue: " << its_queue_size
                 << " data: " << its_data_size;
}

std::string udp_client_endpoint_impl::get_remote_information() const {
    return remote_.address().to_string() + ":" + std::to_string(remote_.port());
}

void udp_client_endpoint_impl::send_cbk(boost::system::error_code const& _error, std::size_t _bytes,
                                        const message_buffer_ptr_t& _sent_msg) {
    (void)_bytes;
    if (!_error) {
        std::lock_guard<std::recursive_mutex> its_lock(mutex_);
        if (queue_.size() > 0) {
            queue_size_ -= queue_.front().first->size();
            queue_.pop_front();

            update_last_departure();

            if (queue_.empty())
                is_sending_ = false;
            else {
                auto its_entry = get_front();
                if (its_entry.first) {
                    send_queued(its_entry);
                }
            }
        }
        return;
    } else if (_error == boost::asio::error::broken_pipe) {
        bool stopping(false);
        {
            std::lock_guard<std::recursive_mutex> its_lock(mutex_);
            stopping = sending_blocked_;
            if (stopping) {
                queue_.clear();
                queue_size_ = 0;
            } else {
                service_t its_service(0);
                method_t its_method(0);
                client_t its_client(0);
                session_t its_session(0);
                if (_sent_msg && _sent_msg->size() > VSOMEIP_SESSION_POS_MAX) {
                    its_service = bithelper::read_uint16_be(&(*_sent_msg)[VSOMEIP_SERVICE_POS_MIN]);
                    its_method = bithelper::read_uint16_be(&(*_sent_msg)[VSOMEIP_METHOD_POS_MIN]);
                    its_client = bithelper::read_uint16_be(&(*_sent_msg)[VSOMEIP_CLIENT_POS_MIN]);
                    its_session = bithelper::read_uint16_be(&(*_sent_msg)[VSOMEIP_SESSION_POS_MIN]);
                }
                VSOMEIP_WARNING_P << "Received error: " << _error.message() << " (" << _error.value() << ") " << get_remote_information()
                                  << " " << queue_.size() << " " << queue_size_ << " (" << hex4(its_client) << "): [" << hex4(its_service)
                                  << "." << hex4(its_method) << "." << hex4(its_session) << "]";
            }
        }
        if (!stopping) {
            print_status();
        }
        was_not_connected_ = true;
        close_socket(true, true);
        boost::asio::dispatch(strand_, std::bind(&client_endpoint_impl::connect, this->shared_from_this()));
    } else if (_error == boost::asio::error::not_connected || _error == boost::asio::error::bad_descriptor
               || _error == boost::asio::error::no_permission) {
        if (_error == boost::asio::error::no_permission) {
            VSOMEIP_WARNING_P << "Received error: " << _error.message() << " (" << _error.value() << ") " << get_remote_information();
            std::lock_guard<std::recursive_mutex> its_lock(mutex_);
            queue_.clear();
            queue_size_ = 0;
        }
        was_not_connected_ = true;
        close_socket(true, true);
        boost::asio::dispatch(strand_, std::bind(&client_endpoint_impl::connect, this->shared_from_this()));
    } else if (_error == boost::asio::error::operation_aborted) {
        VSOMEIP_WARNING_P << "Received error: " << _error.message();
        // endpoint was stopped
        sending_blocked_ = true;
        close_socket(false, false);
    } else {
        if (state_ == cei_state_e::CONNECTING) {
            VSOMEIP_WARNING_P << "Endpoint is already restarting:" << get_remote_information();
        } else {
            close_socket(false, true);
            if (std::shared_ptr<boardnet_endpoint_host> its_host = endpoint_host_.lock(); its_host) {
                its_host->on_disconnect(shared_from_this());
            }
            state_ = cei_state_e::CONNECTING;
            restart(true);
        }
        service_t its_service(0);
        method_t its_method(0);
        client_t its_client(0);
        session_t its_session(0);
        if (_sent_msg && _sent_msg->size() > VSOMEIP_SESSION_POS_MAX) {
            its_service = bithelper::read_uint16_be(&(*_sent_msg)[VSOMEIP_SERVICE_POS_MIN]);
            its_method = bithelper::read_uint16_be(&(*_sent_msg)[VSOMEIP_METHOD_POS_MIN]);
            its_client = bithelper::read_uint16_be(&(*_sent_msg)[VSOMEIP_CLIENT_POS_MIN]);
            its_session = bithelper::read_uint16_be(&(*_sent_msg)[VSOMEIP_SESSION_POS_MIN]);
        }
        VSOMEIP_WARNING_P << "Received error: " << _error.message() << " (" << _error.value() << ") " << get_remote_information() << " "
                          << queue_.size() << " " << queue_size_ << " (" << hex4(its_client) << "): [" << hex4(its_service) << "."
                          << hex4(its_method) << "." << hex4(its_session) << "]";
        print_status();
    }

    std::lock_guard<std::recursive_mutex> its_lock(mutex_);
    is_sending_ = false;
}

bool udp_client_endpoint_impl::tp_segmentation_enabled(service_t _service, instance_t _instance, method_t _method) const {

    return configuration_->is_tp_client(_service, _instance, _method);
}

bool udp_client_endpoint_impl::is_reliable() const {
    return false;
}

std::uint32_t udp_client_endpoint_impl::get_max_allowed_reconnects() const {
    return MAX_RECONNECTS_UNLIMITED;
}

void udp_client_endpoint_impl::max_allowed_reconnects_reached() {
    return;
}

} // namespace vsomeip_v3
