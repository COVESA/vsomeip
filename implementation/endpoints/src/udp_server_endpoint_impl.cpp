// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>
#include <thread>

#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/asio/ip/network_v6.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/endpoint_definition.hpp"
#include "../include/endpoint_host.hpp"
#include "../include/tp.hpp"
#include "../include/udp_server_endpoint_impl.hpp"
#include "../include/udp_server_endpoint_impl_receive_op.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../routing/include/routing_host.hpp"
#include "../../service_discovery/include/defines.hpp"
#include "../../utility/include/bithelper.hpp"
#include "../../utility/include/utility.hpp"

namespace ip = boost::asio::ip;

namespace vsomeip_v3 {

udp_server_endpoint_impl::udp_server_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host,
                                                   const std::shared_ptr<routing_host>& _routing_host, boost::asio::io_context& _io,
                                                   const std::shared_ptr<configuration>& _configuration) :
    server_endpoint_impl<ip::udp>(_endpoint_host, _routing_host, _io, _configuration), lifecycle_idx_(0),
    netmask_(_configuration->get_netmask()), prefix_(_configuration->get_prefix()),
    tp_reassembler_(std::make_shared<tp::tp_reassembler>(_configuration->get_max_message_size_unreliable(), _io)), tp_cleanup_timer_(_io) {
    is_supporting_someip_tp_ = true;
    max_message_size_ = VSOMEIP_MAX_UDP_MESSAGE_SIZE;

    static std::atomic<unsigned> instance_count = 0;
    instance_name_ = "usei#" + std::to_string(++instance_count) + "::";

    VSOMEIP_INFO << instance_name_ << __func__;
}

udp_server_endpoint_impl::~udp_server_endpoint_impl() {
    VSOMEIP_INFO << instance_name_ << __func__ << ": lifecycle_idx=" << lifecycle_idx_.load();
}

bool udp_server_endpoint_impl::is_local() const {
    return false;
}

void udp_server_endpoint_impl::init(const endpoint_type& _local, boost::system::error_code& _error) {
    VSOMEIP_INFO << instance_name_ << __func__ << ": " << _local.address() << ":" << _local.port()
                 << ", lifecycle_idx=" << lifecycle_idx_.load();
    std::scoped_lock its_lock(sync_);
    init_unlocked(_local, _error);
    VSOMEIP_INFO << instance_name_ << __func__ << ": lifecycle_idx=" << lifecycle_idx_.load() << ", " << _error.message();
}

void udp_server_endpoint_impl::init_unlocked(const endpoint_type& _local, boost::system::error_code& _error) {
    // The caller must hold the lock

    if (unicast_socket_) {
        if (local_ == _local) {
            VSOMEIP_WARNING << instance_name_ << __func__ << ": already initialized, lifecycle_idx=" << lifecycle_idx_.load();
            return;
        }

        VSOMEIP_WARNING << instance_name_ << __func__ << ": reset unicast socket, lifecycle_idx=" << lifecycle_idx_.load();
        unicast_socket_.reset();
    }

    unicast_socket_ = std::make_shared<socket_type>(io_, _local.protocol());
    if (!unicast_socket_) {
        _error = boost::asio::error::make_error_code(boost::asio::error::no_memory);
        VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to create socket";
        return;
    }

    if (!unicast_socket_->is_open()) {
        std::ignore = unicast_socket_->open(_local.protocol(), _error);
        if (_error) {
            VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to open socket, " << _error.message();
            unicast_socket_.reset();
            return;
        }
    }

    boost::asio::socket_base::reuse_address opt_reuse_address(true);
    std::ignore = unicast_socket_->set_option(opt_reuse_address, _error);
    if (_error) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to reuse address, " << _error.message();
        unicast_socket_.reset();
        return;
    }

#if defined(__linux__) || defined(__QNX__)
    // If specified, bind to device
    std::string its_device(configuration_->get_device());
    if (!its_device.empty()) {
        if (setsockopt(unicast_socket_->native_handle(), SOL_SOCKET, SO_BINDTODEVICE, its_device.c_str(),
                       static_cast<socklen_t>(its_device.size()))
            == -1) {
            VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to bind to device, " << _error.message();
            // Non-fatal error
            _error.clear();
        }
    }
#endif

    std::ignore = unicast_socket_->bind(_local, _error);
    if (_error) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to bind, " << _error.message();
        unicast_socket_.reset();
        return;
    }

    if (_local.address().is_v4()) {
        is_v4_ = true;
        boost::asio::ip::multicast::outbound_interface option(_local.address().to_v4());
        std::ignore = unicast_socket_->set_option(option, _error);
        if (_error) {
            VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to configure IPv4 outbound interface, " << _error.message();
            unicast_socket_.reset();
            return;
        }
    } else {
        is_v4_ = false;
        // TODO(): an interface index is expected not a scope_id
        boost::asio::ip::multicast::outbound_interface option(static_cast<unsigned int>(_local.address().to_v6().scope_id()));
        std::ignore = unicast_socket_->set_option(option, _error);
        if (_error) {
            VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to configure IPv6 outbound interface, " << _error.message();
            unicast_socket_.reset();
            return;
        }
    }

    boost::asio::socket_base::broadcast option(true);
    std::ignore = unicast_socket_->set_option(option, _error);
    if (_error) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to configure broadcast option, " << _error.message();
        unicast_socket_.reset();
        return;
    }

    const int its_udp_recv_buffer_size = configuration_->get_udp_receive_buffer_size();
    std::ignore =
            unicast_socket_->set_option(boost::asio::socket_base::receive_buffer_size(static_cast<int>(its_udp_recv_buffer_size)), _error);

    if (_error) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to configure receive buffer size, " << _error.message();
        // Non-fatal error
        _error.clear();
    }

    boost::asio::socket_base::receive_buffer_size its_option;
    std::ignore = unicast_socket_->get_option(its_option, _error);

#ifdef __linux__
    // If regular setting of the buffer size did not work, try to force
    // (requires CAP_NET_ADMIN to be successful)
    if (its_option.value() < 0 || its_option.value() < its_udp_recv_buffer_size) {
        _error.assign(setsockopt(unicast_socket_->native_handle(), SOL_SOCKET, SO_RCVBUFFORCE, &its_udp_recv_buffer_size,
                                 sizeof(its_udp_recv_buffer_size)),
                      boost::system::generic_category());
        if (_error) {
            VSOMEIP_INFO << instance_name_ << __func__ << ": failed to force receive buffer size, " << _error.message();
            // Non-fatal error
            _error.clear();
        }
    }
#endif

    if (local_ != _local) {
        instance_name_ += _local.address().to_string();
        instance_name_ += ":";
        instance_name_ += std::to_string(_local.port());
        instance_name_ += "::";

        local_ = _local;

        queue_limit_ = configuration_->get_endpoint_queue_limit(configuration_->get_unicast_address().to_string(), local_.port());
    }
}

void udp_server_endpoint_impl::receive() { }

void udp_server_endpoint_impl::start() {
    VSOMEIP_INFO << instance_name_ << __func__ << ": lifecycle_idx=" << lifecycle_idx_.load();
    std::scoped_lock its_lock(sync_);
    start_unlocked();
    VSOMEIP_INFO << instance_name_ << __func__ << ": done, lifecycle_idx=" << lifecycle_idx_.load();
}

void udp_server_endpoint_impl::stop() {
    VSOMEIP_INFO << instance_name_ << __func__ << ": lifecycle_idx=" << lifecycle_idx_.load();
    std::scoped_lock its_lock(sync_);
    stop_unlocked();
    VSOMEIP_INFO << instance_name_ << __func__ << ": done, lifecycle_idx=" << lifecycle_idx_.load();
}

void udp_server_endpoint_impl::restart(bool _force) {
    std::ignore = _force;

    VSOMEIP_INFO << instance_name_ << __func__ << ": lifecycle_idx=" << lifecycle_idx_.load();
    std::scoped_lock its_lock(sync_);

    stop_unlocked();

    boost::system::error_code its_error;
    init_unlocked(local_, its_error);

    if (!its_error) {
        start_unlocked();
    } else {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": init failure " << its_error.message();
    }

    VSOMEIP_INFO << instance_name_ << __func__ << ": done, lifecycle_idx=" << lifecycle_idx_.load();
}

void udp_server_endpoint_impl::start_unlocked() {
    // The caller must hold the lock

    if (!is_stopped_) {
        VSOMEIP_INFO << instance_name_ << __func__ << ": already started";
        return;
    }

    lifecycle_idx_ += 1;

    if (!unicast_socket_ || !unicast_socket_->is_open()) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": init not called or not successful";
        return;
    }

    is_stopped_ = false;

    VSOMEIP_INFO << instance_name_ << __func__ << ": start unicast data handler, lifecycle_idx=" << lifecycle_idx_.load();
    receive_unicast_unlocked(nullptr);

    VSOMEIP_INFO << instance_name_ << __func__ << ": join " << joined_.size() << " groups";

    auto its_endpoint_host = endpoint_host_.lock();
    if (its_endpoint_host) {
        for (const auto& [its_address, its_joined] : joined_) {
            VSOMEIP_INFO << instance_name_ << __func__ << ": rejoin " << its_address << ", was joined " << its_joined;
            multicast_option_t its_join_option{shared_from_this(), true, boost::asio::ip::make_address(its_address)};
            its_endpoint_host->add_multicast_option(its_join_option);
        }
    }
}

void udp_server_endpoint_impl::stop_unlocked() {
    // The caller must hold the lock

    if (is_stopped_) {
        VSOMEIP_INFO << instance_name_ << __func__ << ": already stopped";
        return;
    }

    lifecycle_idx_ += 1;
    is_stopped_ = true;

    server_endpoint_impl::stop();
    unicast_socket_.reset();
    multicast_socket_.reset();
    tp_reassembler_->stop();
}

bool udp_server_endpoint_impl::is_closed() const {
    std::scoped_lock its_lock(sync_);
    return is_stopped_;
}

void udp_server_endpoint_impl::receive_unicast_unlocked(std::shared_ptr<message_buffer_t> _unicast_recv_buffer) {
    // The caller must hold the lock

    if (!_unicast_recv_buffer) {
        _unicast_recv_buffer = std::make_shared<message_buffer_t>(max_message_size_, 0);
    }

    if (unicast_socket_ && unicast_socket_->is_open()) {
        unicast_socket_->async_receive_from(
                boost::asio::buffer(_unicast_recv_buffer->data(), _unicast_recv_buffer->size()), unicast_remote_,
                [self = shared_ptr(), _unicast_recv_buffer, lifecycle_idx = lifecycle_idx_.load()](const boost::system::error_code& _error,
                                                                                                   std::size_t _bytes) {
                    bool repeat = false;

                    if (lifecycle_idx == self->lifecycle_idx_.load() && _error != boost::asio::error::eof
                        && _error != boost::asio::error::connection_reset && _error != boost::asio::error::operation_aborted) {
                        self->on_unicast_received(_error, _bytes, *_unicast_recv_buffer);

                        std::scoped_lock its_lock(self->sync_);
                        if (lifecycle_idx == self->lifecycle_idx_.load()) {
                            self->receive_unicast_unlocked(_unicast_recv_buffer);
                            repeat = true;
                        }
                    }

                    if (!repeat) {
                        VSOMEIP_WARNING << self->instance_name_ << __func__ << ": stop data handler, lifecycle_idx=" << lifecycle_idx
                                        << " vs " << self->lifecycle_idx_.load() << ", " << _error.message()
                                        << ", stopped=" << self->is_stopped_;
                    }
                });
    } else {
        VSOMEIP_WARNING << instance_name_ << __func__ << ": stop data handler, stopped=" << is_stopped_
                        << ", lifecycle_idx=" << lifecycle_idx_.load();
    }
}

//
// receive_multicast_unlocked is called with sync_ being hold
//
void udp_server_endpoint_impl::receive_multicast_unlocked(std::shared_ptr<message_buffer_t> _multicast_recv_buffer) {
    // The caller must hold the lock

    if (!_multicast_recv_buffer) {
        _multicast_recv_buffer = std::make_shared<message_buffer_t>(max_message_size_, 0);
    }

    if (multicast_socket_ && multicast_socket_->is_open()) {
        auto its_storage = std::make_shared<udp_endpoint_receive_op::storage>(
                multicast_socket_,
                std::bind(&udp_server_endpoint_impl::on_multicast_received,
                          std::dynamic_pointer_cast<udp_server_endpoint_impl>(shared_from_this()), std::placeholders::_1,
                          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5),
                _multicast_recv_buffer, is_v4_, boost::asio::ip::address(), std::numeric_limits<std::size_t>::min());
        multicast_socket_->async_wait(
                socket_type::wait_read,
                [self = shared_ptr(), its_storage, lifecycle_idx = lifecycle_idx_.load()](const boost::system::error_code& _error) {
                    bool repeat = false;

                    if (lifecycle_idx == self->lifecycle_idx_.load() && _error != boost::asio::error::eof
                        && _error != boost::asio::error::connection_reset && _error != boost::asio::error::operation_aborted) {
                        udp_endpoint_receive_op::storage::receive_cb(its_storage, _error);

                        std::scoped_lock its_lock(self->sync_);
                        if (lifecycle_idx == self->lifecycle_idx_.load()) {
                            self->receive_multicast_unlocked(its_storage->multicast_recv_buffer_);
                            repeat = true;
                        }
                    }

                    if (!repeat) {
                        VSOMEIP_WARNING << self->instance_name_ << __func__ << ": stop data handler, lifecycle_idx=" << lifecycle_idx
                                        << " vs " << self->lifecycle_idx_.load() << ", " << _error.message()
                                        << ", stopped=" << self->is_stopped_;
                    }
                });
    } else {
        VSOMEIP_WARNING << instance_name_ << __func__ << ": stop data handler, stopped=" << is_stopped_
                        << ", lifecycle_idx=" << lifecycle_idx_.load();
    }
}

bool udp_server_endpoint_impl::send_to(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size) {
    // The caller shall not hold the sync_ lock
    // But the mutex_ must be locked for the call to send_intern

    std::scoped_lock its_lock(mutex_);
    bool result = false;
    if (_target) {
        endpoint_type its_target(_target->get_address(), _target->get_port());
        result = send_intern(its_target, _data, _size);
    }
    return result;
}

bool udp_server_endpoint_impl::send_error(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size) {
    // The `mutex_` lock must be hold when modifying the `targets_` list or
    // any field inside this list (`_target` points to this list).
    std::scoped_lock its_lock(mutex_, sync_);

    const endpoint_type its_target(_target->get_address(), _target->get_port());
    const auto its_target_iterator(find_or_create_target_unlocked(its_target));
    auto& its_data = its_target_iterator->second;
    bool can_be_send = check_queue_limit(_data, _size, its_data) && check_message_size(_size);

    if (can_be_send) {
        its_data.queue_.emplace_back(std::make_shared<message_buffer_t>(_data, _data + _size), 0);
        its_data.queue_size_ += _size;

        if (!its_data.is_sending_ && unicast_socket_) { // no writing in progress
            std::ignore = send_queued_unlocked(its_target_iterator);
        }
    }

    return can_be_send;
}

bool udp_server_endpoint_impl::send_queued(const target_data_iterator_type _it) {
    // The caller hold the lock on `mutex_`

    std::scoped_lock its_lock(sync_);
    bool result = false;
    if (unicast_socket_) {
        result = send_queued_unlocked(_it);
    } else {
        VSOMEIP_WARNING << instance_name_ << __func__ << ": skipped, no socket!";
    }
    return result;
}

bool udp_server_endpoint_impl::send_queued_unlocked(const target_data_iterator_type _it) {
    // The caller hold two locks: `mutex_` and `sync_` in that order

    const auto its_entry = _it->second.queue_.front();

#if 0
    std::stringstream msg;
    msg << instance_name_ << "sq(" << _it->first.address().to_string() << ":" << _it->first.port()
        << "): ";
    for (std::size_t i = 0; i < its_entry.first->size(); ++i)
        msg << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>((*its_entry.first)[i]) << " ";
    VSOMEIP_INFO << msg.str();
#endif

    // Check whether we need to wait (SOME/IP-TP separation time)
    if (its_entry.second > 0) {
        if (last_sent_ != std::chrono::steady_clock::time_point()) {
            const auto its_elapsed =
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - last_sent_).count();
            if (its_entry.second > its_elapsed) {
                std::this_thread::sleep_for(std::chrono::microseconds(its_entry.second - its_elapsed));
            }
        }
        last_sent_ = std::chrono::steady_clock::now();
    } else {
        last_sent_ = std::chrono::steady_clock::time_point();
    }

    if (auto its_me{std::dynamic_pointer_cast<udp_server_endpoint_impl>(shared_from_this())}) {
        _it->second.is_sending_ = true;
        unicast_socket_->async_send_to(boost::asio::buffer(*its_entry.first), _it->first,
                                       [its_me, _it, its_entry](const boost::system::error_code& _error, std::size_t _bytes) {
                                           if (!_error && its_me->on_unicast_sent_ && !_it->first.address().is_multicast()) {
                                               its_me->on_unicast_sent_(&(its_entry.first)->at(0), static_cast<uint32_t>(_bytes),
                                                                        _it->first.address());
                                           }
                                           its_me->send_cbk(_it->first, _error, _bytes);
                                       });
    }

    return false;
}

void udp_server_endpoint_impl::get_configured_times_from_endpoint(service_t _service, method_t _method,
                                                                  std::chrono::nanoseconds* _debouncing,
                                                                  std::chrono::nanoseconds* _maximum_retention) const {

    // Shall not use the lock

    configuration_->get_configured_timing_responses(_service, udp_server_endpoint_base_impl::local_.address().to_string(),
                                                    udp_server_endpoint_base_impl::local_.port(), _method, _debouncing, _maximum_retention);
}

bool udp_server_endpoint_impl::is_joined(const std::string& _address) const {
    std::scoped_lock its_lock(sync_);
    auto result = is_joined_unlocked(_address);
    return result;
}

bool udp_server_endpoint_impl::is_joined(const std::string& _address, bool& _received) const {
    std::scoped_lock its_lock(sync_);
    auto result = is_joined_unlocked(_address, _received);
    return result;
}

//
// Both is_joined_unlocked - methods must be called with sync_ being hold!
//
bool udp_server_endpoint_impl::is_joined_unlocked(const std::string& _address) const {

    return (joined_.find(_address) != joined_.end());
}

bool udp_server_endpoint_impl::is_joined_unlocked(const std::string& _address, bool& _received) const {
    const auto found_address = joined_.find(_address);
    if (found_address != joined_.end()) {
        _received = found_address->second;
    } else {
        _received = false;
    }

    return (found_address != joined_.end());
}

void udp_server_endpoint_impl::join(const std::string& _address) {
    VSOMEIP_INFO << instance_name_ << __func__ << ": " << _address;
    std::scoped_lock its_lock(sync_);
    join_unlocked(_address);
}

void udp_server_endpoint_impl::join_unlocked(const std::string& _address) {
    // The caller must hold the lock

    try {
        if (!is_joined_unlocked(_address)) {
            joined_[_address] = false;

            auto its_endpoint_host = endpoint_host_.lock();
            if (its_endpoint_host) {
                multicast_option_t its_join_option{shared_from_this(), true, boost::asio::ip::make_address(_address)};
                its_endpoint_host->add_multicast_option(its_join_option);
            }
        }
    } catch (const std::exception& e) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": exception " << e.what();
    }
}

void udp_server_endpoint_impl::leave(const std::string& _address) {
    VSOMEIP_INFO << instance_name_ << __func__ << ": " << _address;
    std::scoped_lock its_lock(sync_);
    leave_unlocked(_address);
}

void udp_server_endpoint_impl::disconnect_from(const client_t _client) {
    std::ignore = _client;
}

void udp_server_endpoint_impl::leave_unlocked(const std::string& _address) {
    // The caller must hold the lock

    try {
        if (is_joined_unlocked(_address)) {
            joined_.erase(_address);

            auto its_endpoint_host = endpoint_host_.lock();
            if (its_endpoint_host) {
                multicast_option_t its_leave_option{shared_from_this(), false, boost::asio::ip::make_address(_address)};
                its_endpoint_host->add_multicast_option(its_leave_option);
            }
        }
    } catch (const std::exception& e) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": exception " << e.what();
    }
}

void udp_server_endpoint_impl::add_default_target(service_t _service, const std::string& _address, uint16_t _port) {
    std::scoped_lock its_lock(sync_);
    endpoint_type its_endpoint(boost::asio::ip::make_address(_address), _port);
    default_targets_[_service] = its_endpoint;
}

void udp_server_endpoint_impl::remove_default_target(service_t _service) {
    std::scoped_lock its_lock(sync_);
    default_targets_.erase(_service);
}

bool udp_server_endpoint_impl::get_default_target(service_t _service, udp_server_endpoint_impl::endpoint_type& _target) const {
    std::scoped_lock its_lock(sync_);
    bool is_valid(false);
    auto find_service = default_targets_.find(_service);
    if (find_service != default_targets_.end()) {
        _target = find_service->second;
        is_valid = true;
    }
    return is_valid;
}

uint16_t udp_server_endpoint_impl::get_local_port() const {
    std::scoped_lock its_lock(sync_);
    return local_.port();
}

void udp_server_endpoint_impl::on_unicast_received(const boost::system::error_code& _error, std::size_t _bytes,
                                                   const message_buffer_t& _unicast_recv_buffer) {
    // The caller shall not hold the lock

    if (_error) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": " << _error.message();
    } else {
        on_message_received_unlocked(_error, _bytes, false, unicast_remote_, _unicast_recv_buffer);
    }
}

void udp_server_endpoint_impl::on_multicast_received(const boost::system::error_code& _error, std::size_t _bytes,
                                                     const boost::asio::ip::udp::endpoint& _sender,
                                                     const boost::asio::ip::address& /*_destination*/,
                                                     const message_buffer_t& _multicast_recv_buffer) {
    // The caller shall not hold the lock

    if (_error) {
        VSOMEIP_ERROR << instance_name_ << __func__ << ": " << _error.message();
    } else {
        bool own_message = false;
        bool own_subnet = false;
        on_sent_multicast_received_cbk_t own_callback = nullptr;

        {
            std::scoped_lock its_lock(sync_);
            own_message = _sender.address() == local_.address();
            own_subnet = is_same_subnet_unlocked(_sender.address());
            own_callback = receive_own_multicast_messages_ ? on_sent_multicast_received_ : nullptr;
        }

        if (!own_message) {
            if (own_subnet) {
                on_message_received_unlocked(_error, _bytes, true, _sender, _multicast_recv_buffer);
            }
        } else if (own_callback) {
            own_callback(_multicast_recv_buffer.data(), static_cast<uint32_t>(_bytes), boost::asio::ip::address());
        } else {
            // Nothing to do, else clang-tidy complains
        }
    }
}

void udp_server_endpoint_impl::on_message_received_unlocked(const boost::system::error_code& _error, std::size_t _bytes, bool _is_multicast,
                                                            const endpoint_type& _remote, const message_buffer_t& _buffer) {
    // The caller shall not hold the lock

#if 0
    std::stringstream msg;
    msg << instance_name_ << "rcb(" << _remote.address() << ":" << _remote.port() << "): ";
    for (std::size_t i = 0; i < _bytes; ++i)
        msg << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(_buffer[i]) << " ";
    VSOMEIP_INFO << msg.str();
#endif

    std::shared_ptr<routing_host> its_host = routing_host_.lock();

    if (its_host) {
        if (!_error && 0 < _bytes) {
            std::size_t remaining_bytes = _bytes;
            std::size_t i = 0;
            const boost::asio::ip::address its_remote_address(_remote.address());
            const uint16_t its_remote_port(_remote.port());
            do {
                uint64_t read_message_size = utility::get_message_size(&_buffer[i], remaining_bytes);
                if (read_message_size > MESSAGE_SIZE_UNLIMITED) {
                    VSOMEIP_ERROR << instance_name_ << __func__ << ": message size exceeds allowed maximum!";
                    return;
                }
                auto current_message_size = static_cast<uint32_t>(read_message_size);
                if (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE && current_message_size <= remaining_bytes) {
                    if (remaining_bytes - current_message_size > remaining_bytes) {
                        VSOMEIP_ERROR << instance_name_ << __func__ << ": buffer underflow!";
                        return;
                    }

                    if (current_message_size > VSOMEIP_RETURN_CODE_POS
                        && (_buffer[i + VSOMEIP_PROTOCOL_VERSION_POS] != VSOMEIP_PROTOCOL_VERSION
                            || !utility::is_valid_message_type(tp::tp::tp_flag_unset(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS]))
                            || !utility::is_valid_return_code(static_cast<return_code_e>(_buffer[i + VSOMEIP_RETURN_CODE_POS]))
                            || (tp::tp::tp_flag_is_set(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS])
                                && get_local_port() == configuration_->get_sd_port()))) {
                        if (_buffer[i + VSOMEIP_PROTOCOL_VERSION_POS] != VSOMEIP_PROTOCOL_VERSION) {
                            VSOMEIP_ERROR << instance_name_ << __func__ << ": wrong protocol version: 0x" << std::hex << std::setfill('0')
                                          << std::setw(2) << static_cast<uint32_t>(_buffer[i + VSOMEIP_PROTOCOL_VERSION_POS])
                                          << " local: " << get_address_port_local_unlocked() << " remote: " << its_remote_address << ":"
                                          << std::dec << its_remote_port;
                            // ensure to send back a message w/ wrong protocol version
                            its_host->on_message(&_buffer[i], VSOMEIP_SOMEIP_HEADER_SIZE + 8, this, _is_multicast, VSOMEIP_ROUTING_CLIENT,
                                                 nullptr, its_remote_address, its_remote_port);
                        } else if (!utility::is_valid_message_type(tp::tp::tp_flag_unset(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS]))) {
                            VSOMEIP_ERROR << instance_name_ << __func__ << ": invalid message type: 0x" << std::hex << std::setfill('0')
                                          << std::setw(2) << static_cast<uint32_t>(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS])
                                          << " local: " << get_address_port_local_unlocked() << " remote: " << its_remote_address << ":"
                                          << std::dec << its_remote_port;
                        } else if (!utility::is_valid_return_code(static_cast<return_code_e>(_buffer[i + VSOMEIP_RETURN_CODE_POS]))) {
                            VSOMEIP_ERROR << instance_name_ << __func__ << ": invalid return code: 0x" << std::hex << std::setfill('0')
                                          << std::setw(2) << static_cast<uint32_t>(_buffer[i + VSOMEIP_RETURN_CODE_POS])
                                          << " local: " << get_address_port_local_unlocked() << " remote: " << its_remote_address << ":"
                                          << std::dec << its_remote_port;
                        } else if (tp::tp::tp_flag_is_set(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS])
                                   && get_local_port() == configuration_->get_sd_port()) {
                            VSOMEIP_WARNING << instance_name_ << __func__ << ": not a SD message,"
                                            << " local: " << get_address_port_local_unlocked() << " remote: " << its_remote_address << ":"
                                            << std::dec << its_remote_port;
                        } else {
                            // Nothing to do, else clang-tidy complains
                        }
                        return;
                    }

                    remaining_bytes -= current_message_size;
                    const service_t its_service = bithelper::read_uint16_be(&_buffer[i + VSOMEIP_SERVICE_POS_MIN]);

                    if (utility::is_request(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS])) {
                        const client_t its_client = bithelper::read_uint16_be(&_buffer[i + VSOMEIP_CLIENT_POS_MIN]);
                        if (its_client != MAGIC_COOKIE_CLIENT) {
                            const method_t its_method = bithelper::read_uint16_be(&_buffer[i + VSOMEIP_METHOD_POS_MIN]);
                            const session_t its_session = bithelper::read_uint16_be(&_buffer[i + VSOMEIP_SESSION_POS_MIN]);
                            std::scoped_lock its_clients_lock(clients_mutex_);
                            clients_[to_clients_key(its_service, its_method, its_client)][its_session] = _remote;
                        }
                    }
                    if (tp::tp::tp_flag_is_set(_buffer[i + VSOMEIP_MESSAGE_TYPE_POS])) {
                        const method_t its_method = bithelper::read_uint16_be(&_buffer[i + VSOMEIP_METHOD_POS_MIN]);
                        instance_t its_instance = this->get_instance(its_service);

                        if (its_instance != ANY_INSTANCE) {
                            if (!tp_segmentation_enabled(its_service, its_instance, its_method)) {
                                VSOMEIP_WARNING << instance_name_ << __func__ << ": SomeIP/TP message for service: 0x" << std::hex
                                                << its_service << " method: 0x" << its_method << " which is not configured for TP:"
                                                << " local: " << get_address_port_local_unlocked() << " remote: " << its_remote_address
                                                << ":" << std::dec << its_remote_port;
                                return;
                            }
                        }
                        const auto res =
                                tp_reassembler_->process_tp_message(&_buffer[i], current_message_size, its_remote_address, its_remote_port);
                        if (res.first) {
                            if (utility::is_request(res.second[VSOMEIP_MESSAGE_TYPE_POS])) {
                                const client_t its_client = bithelper::read_uint16_be(&res.second[VSOMEIP_CLIENT_POS_MIN]);
                                if (its_client != MAGIC_COOKIE_CLIENT) {
                                    const session_t its_session = bithelper::read_uint16_be(&res.second[VSOMEIP_SESSION_POS_MIN]);
                                    std::scoped_lock its_clients_lock(clients_mutex_);
                                    clients_[to_clients_key(its_service, its_method, its_client)][its_session] = _remote;
                                }
                            }
                            its_host->on_message(&res.second[0], static_cast<uint32_t>(res.second.size()), this, _is_multicast,
                                                 VSOMEIP_ROUTING_CLIENT, nullptr, its_remote_address, its_remote_port);
                        }
                    } else {
                        if (its_service != VSOMEIP_SD_SERVICE
                            || (current_message_size > VSOMEIP_SOMEIP_HEADER_SIZE && current_message_size >= remaining_bytes)) {
                            its_host->on_message(&_buffer[i], current_message_size, this, _is_multicast, VSOMEIP_ROUTING_CLIENT, nullptr,
                                                 its_remote_address, its_remote_port);
                        } else {
                            // ignore messages for service discovery with shorter SomeIP length
                            VSOMEIP_ERROR << instance_name_ << __func__ << ": unreliable vSomeIP SD message with too short length field"
                                          << " local: " << get_address_port_local_unlocked() << " remote: " << its_remote_address << ":"
                                          << std::dec << its_remote_port;
                        }
                    }
                    i += current_message_size;
                } else {
                    VSOMEIP_ERROR << instance_name_ << __func__
                                  << ": unreliable vSomeIP message with bad length field local: " << get_address_port_local_unlocked()
                                  << " remote: " << its_remote_address << ":" << std::dec << its_remote_port;
                    if (remaining_bytes > VSOMEIP_SERVICE_POS_MAX) {
                        service_t its_service = bithelper::read_uint16_be(&_buffer[VSOMEIP_SERVICE_POS_MIN]);
                        if (its_service != VSOMEIP_SD_SERVICE) {
                            if (read_message_size == 0) {
                                VSOMEIP_ERROR << instance_name_ << __func__ << ": unreliable vSomeIP message with SomeIP message length 0!";

                            } else {
                                auto its_endpoint_host = endpoint_host_.lock();
                                if (its_endpoint_host) {
                                    its_endpoint_host->on_error(&_buffer[i], static_cast<uint32_t>(remaining_bytes), this,
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

bool udp_server_endpoint_impl::is_same_subnet_unlocked(const boost::asio::ip::address& _address) const {
    bool is_same(true);

    if (_address.is_v4()) {
        boost::asio::ip::network_v4 its_network(local_.address().to_v4(), netmask_.to_v4());
        boost::asio::ip::address_v4_range its_hosts = its_network.hosts();
        is_same = (its_hosts.find(_address.to_v4()) != its_hosts.end());
    } else {
        boost::asio::ip::network_v6 its_network(local_.address().to_v6(), prefix_);
        boost::asio::ip::address_v6_range its_hosts = its_network.hosts();
        is_same = (its_hosts.find(_address.to_v6()) != its_hosts.end());
    }

    return is_same;
}

void udp_server_endpoint_impl::print_status() {
    std::scoped_lock its_lock(mutex_, sync_);

    VSOMEIP_ERROR << instance_name_ << __func__ << ": " << std::dec << local_.port() << " number targets: " << std::dec << targets_.size();

    for (const auto& c : targets_) {
        std::size_t its_data_size(0);
        std::size_t its_queue_size(0);
        its_queue_size = c.second.queue_.size();
        its_data_size = c.second.queue_size_;

        VSOMEIP_INFO << instance_name_ << __func__ << ": client: " << c.first.address().to_string() << ":" << std::dec << c.first.port()
                     << " queue: " << std::dec << its_queue_size << " data: " << std::dec << its_data_size;
    }
}

std::string udp_server_endpoint_impl::get_remote_information(const target_data_iterator_type _it) const {
    return _it->first.address().to_string() + ":" + std::to_string(_it->first.port());
}

std::string udp_server_endpoint_impl::get_remote_information(const endpoint_type& _remote) const {
    return _remote.address().to_string() + ":" + std::to_string(_remote.port());
}

bool udp_server_endpoint_impl::is_reliable() const {
    return false;
}

std::string udp_server_endpoint_impl::get_address_port_local_unlocked() const {
    // The caller shall not hold the lock

    std::shared_ptr<socket_type> unicast_socket;

    {
        std::scoped_lock its_lock(sync_);
        unicast_socket = unicast_socket_;
    }

    std::string its_address_port;
    its_address_port.reserve(21);
    its_address_port = "ERR!";

    boost::system::error_code ec;

    if (unicast_socket && unicast_socket->is_open()) {
        endpoint_type its_local_endpoint = unicast_socket->local_endpoint(ec);
        if (!ec) {
            its_address_port = its_local_endpoint.address().to_string();
            its_address_port += ":";
            its_address_port += std::to_string(its_local_endpoint.port());
        }
    }

    return its_address_port;
}

bool udp_server_endpoint_impl::tp_segmentation_enabled(service_t _service, instance_t _instance, method_t _method) const {

    return configuration_->is_tp_service(_service, _instance, _method);
}

void udp_server_endpoint_impl::set_multicast_option(const boost::asio::ip::address& _address, bool _is_join,
                                                    boost::system::error_code& _error) {
    VSOMEIP_INFO << instance_name_ << __func__ << ": " << (_is_join ? "join " : "leave ") << _address
                 << ", lifecycle_idx=" << lifecycle_idx_.load() << ", stopped=" << is_stopped_;

    std::unique_lock its_lock(sync_);

    if (is_stopped_) {
        VSOMEIP_INFO << instance_name_ << __func__ << ": ignored because server is stopping";
        return;
    }

    bool has_joined = multicast_socket_ && join_status_.find(_address.to_string()) != join_status_.end();

    if (_is_join && has_joined) {
        // We can skip the join operation, but we don't skip the leave operation
        // because if the network interface is down when this operation is executed,
        // it returns an error and we do not know in which state is the join.
        VSOMEIP_INFO << instance_name_ << __func__ << ": operation already done, skipped";
        return;
    }

    if (_is_join) {
        // If the multicast socket does not yet exist, create it.
        if (!multicast_socket_) {
            // All previous successful join operations shall be ignored
            // because we will recreate the socket.
            join_status_.clear();

            multicast_socket_ = std::make_unique<socket_type>(io_, local_.protocol());
            if (!multicast_socket_) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to create socket";
                _error = boost::asio::error::make_error_code(boost::asio::error::no_memory);
                return;
            }

            if (!multicast_socket_->is_open()) {
                std::ignore = multicast_socket_->open(local_.protocol(), _error);
                if (_error) {
                    VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to open socket, " << _error.message();
                    multicast_socket_.reset();
                    return;
                }
            }

            std::ignore = multicast_socket_->set_option(ip::udp::socket::reuse_address(true), _error);
            if (_error) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to configure reuse address, " << _error.message();
                multicast_socket_.reset();
                return;
            }

#ifdef _WIN32
            const char* its_pktinfo_option("0001");
            ::setsockopt(multicast_socket_->native_handle(), (is_v4_ ? IPPROTO_IP : IPPROTO_IPV6), (is_v4_ ? IP_PKTINFO : IPV6_PKTINFO),
                         its_pktinfo_option, sizeof(its_pktinfo_option));
#else
            int its_pktinfo_option(1);
            ::setsockopt(multicast_socket_->native_handle(), (is_v4_ ? IPPROTO_IP : IPPROTO_IPV6), (is_v4_ ? IP_PKTINFO : IPV6_RECVPKTINFO),
                         &its_pktinfo_option, sizeof(its_pktinfo_option));
#endif

            if (!multicast_local_) {
                if (is_v4_) {
                    multicast_local_ = std::make_unique<endpoint_type>(boost::asio::ip::address_v4::any(), local_.port());
                } else { // is_v6
                    multicast_local_ = std::make_unique<endpoint_type>(boost::asio::ip::address_v6::any(), local_.port());
                }
            }

            std::ignore = multicast_socket_->bind(*multicast_local_, _error);
            if (_error) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to bind, " << _error.message();
                multicast_socket_.reset();
                return;
            }

            // TODO(): Check why changing the receive buffer size? By default it is 256KiB.
            //         In addition, it prevents Linux from doing some optimizations.
            const int its_udp_recv_buffer_size = configuration_->get_udp_receive_buffer_size();

            std::ignore = multicast_socket_->set_option(boost::asio::socket_base::receive_buffer_size(its_udp_recv_buffer_size), _error);
            if (_error) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to configure received buffer size, " << _error.message();
                // Non-fatal error
                _error.clear();
            }
#ifndef _WIN32
            struct timeval timeout { };

            timeout.tv_sec = VSOMEIP_SETSOCKOPT_TIMEOUT_US / 1'000'000;
            timeout.tv_usec = VSOMEIP_SETSOCKOPT_TIMEOUT_US % 1'000'000;

            if (setsockopt(multicast_socket_->native_handle(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to configure SO_RCVTIMEO, " << _error.message();
                // Non-fatal error
                _error.clear();
            }

            if (setsockopt(multicast_socket_->native_handle(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to configure SO_SNDTIMEO, " << _error.message();
                // Non-fatal error
                _error.clear();
            }
#endif
            boost::asio::socket_base::receive_buffer_size its_option;
            std::ignore = multicast_socket_->get_option(its_option, _error);
            if (_error) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": failed to get received buffer size, " << _error.message();
                multicast_socket_.reset();
                return;
            }
#ifdef __linux__
            // If regular setting of the buffer size did not work, try to force
            // (requires CAP_NET_ADMIN to be successful)
            if (its_option.value() < 0 || its_option.value() < its_udp_recv_buffer_size) {
                _error.assign(setsockopt(multicast_socket_->native_handle(), SOL_SOCKET, SO_RCVBUFFORCE, &its_udp_recv_buffer_size,
                                         sizeof(its_udp_recv_buffer_size)),
                              boost::system::generic_category());
                if (_error) {
                    VSOMEIP_INFO << instance_name_ << __func__ << ": failed to force received buffer size, " << _error.message();
                    // Non-fatal error
                    _error.clear();
                }
            }
#endif

            VSOMEIP_INFO << instance_name_ << __func__ << ": start multicast data handler, lifecycle_idx=" << lifecycle_idx_.load();
            receive_multicast_unlocked(nullptr);
        }

        boost::asio::ip::multicast::join_group its_join_option;

        if (is_v4_) {
            its_join_option = boost::asio::ip::multicast::join_group(_address.to_v4(), local_.address().to_v4());
        } else {
            // TODO(): an interface index is expected not a scope_id
            its_join_option = boost::asio::ip::multicast::join_group(_address.to_v6(),
                                                                     static_cast<unsigned int>(local_.address().to_v6().scope_id()));
        }

        // "Both ADD_MEMBERSHIP and DROP_MEMBERSHIP are nonblocking operations. They
        // should return immediately indicating either success or failure."
        // https://tldp.org/HOWTO/Multicast-HOWTO-6.html
        std::ignore = multicast_socket_->set_option(its_join_option, _error);

        if (!_error) {
            joined_[_address.to_string()] = true;
            join_status_[_address.to_string()] = true;
            VSOMEIP_INFO << instance_name_ << __func__ << ": join successful";
        } else {
            VSOMEIP_ERROR << instance_name_ << __func__ << ": join failure, " << _error.message()
                          << ", could happen during restart operation";
            // Non-fatal error, that must be reported to endpoint_manager_impl
            // so it can repeat the operation. It could occur after a restart,
            // and Linux didn't have time to clean the previous memberships.
            // See https://stackoverflow.com/questions/45442416
        }
    } else {
        join_status_.erase(_address.to_string());

        if (multicast_socket_ && multicast_socket_->is_open()) {
            boost::asio::ip::multicast::leave_group its_leave_option(_address);
            std::ignore = multicast_socket_->set_option(its_leave_option, _error);

            if (_error) {
                VSOMEIP_ERROR << instance_name_ << __func__ << ": leave failure, " << _error.message();
            } else {
                VSOMEIP_INFO << instance_name_ << __func__ << ": leave successful";
            }

            if (joined_.empty()) {
                VSOMEIP_INFO << instance_name_ << __func__ << ": stop multicast";
                multicast_socket_.reset();
            }
        }
    }
}

void udp_server_endpoint_impl::set_unicast_sent_callback(const on_unicast_sent_cbk_t& _cbk) {
    std::scoped_lock its_lock(sync_);
    on_unicast_sent_ = _cbk;
}

void udp_server_endpoint_impl::set_sent_multicast_received_callback(const on_sent_multicast_received_cbk_t& _cbk) {
    std::scoped_lock its_lock(sync_);
    on_sent_multicast_received_ = _cbk;
}

void udp_server_endpoint_impl::set_receive_own_multicast_messages(bool value) {
    std::scoped_lock its_lock(sync_);
    receive_own_multicast_messages_ = value;
}

bool udp_server_endpoint_impl::is_joining() const {
    std::scoped_lock its_lock(sync_);
    auto result = !joined_.empty();
    return result;
}

} // namespace vsomeip_v3
