// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "fake_udp_socket_handle.hpp"
#include "fake_udp_socket.hpp"
#include "../socket_manager.hpp"
#include "../command_message.hpp"
#include "../test_logging.hpp"
#include "../someip_message.hpp"

#include <mutex>
#include <span>
#include <thread>
#include <numeric>

#define LOCAL_LOG TEST_LOG << "[fake-udp-socket] "

namespace vsomeip_v3::testing {

fake_udp_socket_handle::fake_udp_socket_handle(boost::asio::io_context& _io) : io_(_io) { }

fake_udp_socket_handle::~fake_udp_socket_handle() {
    LOCAL_LOG << " Deleting: " << socket_id_.fd_;
    auto sm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        return socket_manager_.lock();
    }();
    if (!sm) {
        return;
    }
    sm->remove(socket_id_.fd_);
}

void fake_udp_socket_handle::init(fd_t _fd, socket_type _type, std::weak_ptr<socket_manager> _socket_manager) {
    auto const lock = std::scoped_lock(mtx_);
    socket_id_.fd_ = _fd;
    socket_id_.type_ = _type;
    socket_manager_ = _socket_manager;
}

void fake_udp_socket_handle::cancel() {
    LOCAL_LOG << __func__ << ": " << socket_id_.fd_;
    auto const lock = std::scoped_lock(mtx_);
    if (receptor_) {
        boost::asio::post(io_, [handler = std::move(receptor_->rw_handler_)] {
            handler(boost::asio::error::make_error_code(boost::asio::error::operation_aborted), 0);
        });
        receptor_ = std::nullopt;
    }

    protocol_type_ = std::nullopt;
    connected_ep_ = std::nullopt;
    local_ep_ = std::nullopt;
}

void fake_udp_socket_handle::clear_handler() {
    LOCAL_LOG << __func__ << ", fd: " << socket_id_;
    cancel();
}

void fake_udp_socket_handle::set_app_name(std::string const& _name) {
    auto const lock = std::scoped_lock(mtx_);
    socket_id_.app_name_ = _name;
}

std::string fake_udp_socket_handle::get_app_name() const {
    auto const lock = std::scoped_lock(mtx_);
    return socket_id_.app_name_;
}

[[nodiscard]] bool fake_udp_socket_handle::is_open() {
    auto const lock = std::scoped_lock(mtx_);
    return static_cast<bool>(protocol_type_);
}

void fake_udp_socket_handle::open(boost::asio::ip::udp::endpoint::protocol_type _type) {
    auto const lock = std::scoped_lock(mtx_);
    protocol_type_ = _type;
}

void fake_udp_socket_handle::close() {
    LOCAL_LOG << __func__ << ", fd: " << socket_id_.fd_;
    cancel();
}

[[nodiscard]] bool fake_udp_socket_handle::bind(boost::asio::ip::udp::endpoint const& _ep) {
    LOCAL_LOG << "calling bind on: " << socket_id_ << " with: " << _ep;
    auto bsm = [&]() -> std::shared_ptr<socket_manager> {
        auto const lock = std::scoped_lock(mtx_);
        if (!local_ep_) {
            return socket_manager_.lock();
        }
        return nullptr;
    }();

    if (bsm && bsm->bind_socket(std::dynamic_pointer_cast<fake_udp_socket_handle>(shared_from_this()), _ep, socket_id_.fd_)) {
        auto const lock = std::scoped_lock(mtx_);
        local_ep_ = _ep;
        return true;
    }

    return false;
}

boost::asio::ip::udp::endpoint fake_udp_socket_handle::local_endpoint() {
    auto const lock = std::scoped_lock(mtx_);
    return *local_ep_;
}

void fake_udp_socket_handle::set_option(boost::asio::ip::multicast::join_group _join, boost::system::error_code& _ec) {
    auto type_sm = [&]() -> std::pair<boost::asio::ip::udp, std::shared_ptr<socket_manager>> {
        auto const lock = std::scoped_lock(mtx_);
        return std::make_pair(local_ep_->protocol(), socket_manager_.lock());
    }();

    if (type_sm.first == boost::asio::ip::udp::v6()) {
        _ec = boost::asio::error::address_family_not_supported;
        throw std::runtime_error("currently not supporting ipv6");
        return;
    }

    auto const& multicast_addresses = _join.data(type_sm.first);
    auto const inet4_cast = reinterpret_cast<const boost::asio::detail::in4_mreq_type*>(multicast_addresses);
    boost::asio::ip::address_v4 multicast_address{ntohl(inet4_cast->imr_multiaddr.s_addr)};
    boost::asio::ip::address_v4 interface_address{ntohl(inet4_cast->imr_interface.s_addr)};

    if (type_sm.second) {
        type_sm.second->join_multicast_group(multicast_address, socket_id_.fd_, socket_id_.app_name_);
        _ec = boost::system::error_code();
    } else {
        _ec = boost::asio::error::network_unreachable;
    }
}

void fake_udp_socket_handle::set_option(boost::asio::ip::multicast::leave_group _leave, boost::system::error_code& _ec) {
    auto type_sm = [&]() -> std::pair<boost::asio::ip::udp, std::shared_ptr<socket_manager>> {
        auto const lock = std::scoped_lock(mtx_);
        return std::make_pair(local_ep_->protocol(), socket_manager_.lock());
    }();

    if (type_sm.first == boost::asio::ip::udp::v6()) {
        _ec = boost::asio::error::address_family_not_supported;
        throw std::runtime_error("currently not supporting ipv6");
        return;
    }

    auto const& multicast_addresses = _leave.data(type_sm.first);
    auto const inet4_cast = reinterpret_cast<const boost::asio::detail::in4_mreq_type*>(multicast_addresses);
    boost::asio::ip::address_v4 multicast_address{ntohl(inet4_cast->imr_multiaddr.s_addr)};
    if (type_sm.second) {
        type_sm.second->leave_multicast_group(multicast_address, socket_id_.fd_);
    }

    _ec = boost::system::error_code();
}

void fake_udp_socket_handle::async_connect(boost::asio::ip::udp::endpoint const& _remote_ep, connect_handler _handler) {
    auto const lock = std::scoped_lock(mtx_);
    LOCAL_LOG << "binding remote endpoint";
    if (local_ep_ && !connected_ep_) {
        connected_ep_ = _remote_ep;
        boost::asio::post(io_, [handler = std::move(_handler)] { handler(boost::system::error_code()); });
    } else {
        boost::asio::post(io_, [handler = std::move(_handler)] {
            if (!handler) {
                return;
            }
            handler(boost::asio::error::make_error_code(boost::asio::error::connection_refused));
            return;
        });
    }
}

void fake_udp_socket_handle::async_send(boost::asio::const_buffer const& _buffer, rw_handler _handler) {
    auto lock = std::unique_lock(mtx_);
    if (auto bsm = socket_manager_.lock(); bsm && local_ep_ && connected_ep_) {
        auto src = *local_ep_;
        auto dst = *connected_ep_;
        auto pipe = sender_pipe_;
        lock.unlock();

        boost::asio::post(io_, [handler = std::move(_handler), size = _buffer.size()] {
            if (!handler) {
                return;
            }
            handler(boost::system::error_code(), size);
            return;
        });

        auto data = prepare_control_data(_buffer, src, dst);
        if (pipe) {
            pipe->add_data(data);
            update_sending();
        }
    } else {
        boost::asio::post(io_, [handler = std::move(_handler)] {
            if (!handler) {
                return;
            }
            handler(boost::asio::error::make_error_code(boost::asio::error::no_protocol_option), 0);
            return;
        });
    }
}

void fake_udp_socket_handle::async_send_to(boost::asio::const_buffer const& _buffer, boost::asio::ip::udp::endpoint _dst,
                                           rw_handler _handler) {
    // Check if we should delay this message
    if (delay_messages_.load(std::memory_order_acquire)) {
        std::vector<unsigned char> data;
        data.reserve(_buffer.size());
        auto first = static_cast<const unsigned char*>(_buffer.data());
        auto const last = first + _buffer.size();
        data.insert(data.end(), first, last);

        {
            std::scoped_lock lock(mtx_);
            delayed_messages_.push_back({std::move(data), std::optional<addresses>{{*local_ep_, _dst}}});
            LOCAL_LOG << "delaying message from " << *local_ep_ << " to " << _dst << " (total delayed: " << delayed_messages_.size() << ")";
        }

        boost::asio::post(io_, [handler = std::move(_handler), size = _buffer.size()] {
            if (!handler) {
                return;
            }
            handler(boost::system::error_code(), size);
            return;
        });
        return;
    }

    boost::asio::post(io_, [handler = std::move(_handler), size = _buffer.size()] {
        if (!handler) {
            return;
        }
        handler(boost::system::error_code(), size);
        return;
    });

    {
        auto const lock = std::scoped_lock(mtx_);
        auto data = prepare_control_data(_buffer, *local_ep_, _dst);
        sender_pipe_->add_data(data);
    }
    update_sending();
}

void fake_udp_socket_handle::consume(std::vector<unsigned char> _buffer, boost::asio::ip::udp::endpoint _src,
                                     boost::asio::ip::udp::endpoint _dst) {
    auto const lock = std::scoped_lock(mtx_);
    if (someip_message message; parse(_buffer, message) > 0) {
        LOCAL_LOG << socket_id_ << " @ " << *local_ep_ << " received message from " << _src.address() << " data: " << message;
        if (message.sd_) {
            for (auto const& entry : message.sd_->get_entries()) {
                someip_sd_record_message received_entry{entry->get_type(), entry->get_ttl()};
                received_sd_record_.record(received_entry);
            }
        }
        control_data_t data = {.buffer_ = _buffer, .addresses_ = std::optional<addresses>{{_src, _dst}}};
        receiver_pipe_->add_data(data);
        update_reception_unlocked();
    } else {
        LOCAL_LOG << "Failure parsing data";
    }
}

void fake_udp_socket_handle::async_receive_from(boost::asio::mutable_buffer _buffer, boost::asio::ip::udp::endpoint& _from,
                                                rw_handler _handler) {
    auto const lock = std::scoped_lock(mtx_);
    if (receptor_) {
        boost::asio::post(
                io_, [handler = std::move(_handler)] { handler(boost::asio::error::make_error_code(boost::asio::error::in_progress), 0); });
        return;
    }

    receptor_.emplace(std::move(_handler), _buffer, _from);
    update_reception_unlocked();
}

void fake_udp_socket_handle::update_reception_unlocked() {
    // Check if a receptor is configured.
    if (!receptor_) {
        LOCAL_LOG << "No receptor" << " ec=" << stashed_ec_.value_or(boost::system::error_code());
        return;
    }

    // Check if there is an error to deliver.
    if (stashed_ec_) {
        // Post the handler to the event loop.
        boost::asio::post(io_, [handler = std::move(receptor_->rw_handler_), ec = *stashed_ec_] { handler(ec, 0); });

        // Clean-up data.
        stashed_ec_.reset();
        receptor_.reset();
        return;
    }

    // Check if there is data to process.
    if (receiver_pipe_->size() != 0) {
        control_data_t input;
        if (!receiver_pipe_->fetch_data(input)) {
            return;
        }

        // Copy buffer.
        auto writable_length = std::min(receptor_->buffer_.size(), input.buffer_.size());
        std::memcpy(receptor_->buffer_.data(), input.buffer_.data(), writable_length);

        // Update the source endpoint.
        receptor_->endpoint_ = input.addresses_->src_;

        // Post the handler to the event loop.
        boost::asio::post(io_, [handler = std::move(receptor_->rw_handler_), written = writable_length] {
            handler(boost::system::error_code(), written);
        });

        // Clean-up data.
        receptor_.reset();
    }
}

void fake_udp_socket_handle::update_sending() {
    auto sm_pipe = [&]() -> std::pair<std::shared_ptr<socket_manager>, std::shared_ptr<data_pipe>> {
        auto const lock = std::scoped_lock(mtx_);
        return std::make_pair(socket_manager_.lock(), sender_pipe_);
    }();

    if (sm_pipe.first && sm_pipe.second->size() != 0) {
        control_data_t input;
        if (!sm_pipe.second->fetch_data(input)) {
            return;
        }
        sm_pipe.first->send_someip(input.buffer_, input.addresses_->src_, input.addresses_->dst_);
    }
}

bool fake_udp_socket_handle::delay_message_processing(bool _delay) {
    {
        std::scoped_lock its_lock(mtx_);
        LOCAL_LOG << "setting delay_message_processing: " << (_delay ? "true" : "false") << " on fd: " << socket_id_.fd_;
    }
    delay_messages_.store(_delay, std::memory_order_release);

    if (!_delay) {
        process_delayed_messages();
    }

    return true;
}

void fake_udp_socket_handle::process_delayed_messages() {
    std::vector<control_data_t> messages_to_send;

    {
        std::scoped_lock lock(mtx_);
        messages_to_send = std::move(delayed_messages_);
        delayed_messages_.clear();
    }

    if (!messages_to_send.empty()) {
        std::shared_ptr<socket_manager> bsm;
        std::shared_ptr<data_pipe> sender;
        {
            std::scoped_lock its_lock(mtx_);
            LOCAL_LOG << "processing " << messages_to_send.size() << " delayed messages on fd: " << socket_id_.fd_;
            bsm = socket_manager_.lock();
            sender = sender_pipe_;
        }

        if (bsm && sender) {
            for (auto& data : messages_to_send) {
                sender->add_data(data);
                update_sending();
            }
        }
    }
}

void fake_udp_socket_handle::stash_ec(boost::system::error_code _ec) {
    std::scoped_lock lock{mtx_};
    stashed_ec_ = {_ec};
    update_reception_unlocked();
}

void fake_udp_socket_handle::replace_pipe(std::shared_ptr<data_pipe> _pipe, socket_role _applied_on) {
    auto lock = std::unique_lock(mtx_);
    if (_applied_on == socket_role::client) {
        // receiving pipe
        _pipe->init([weak_self = weak_from_this(), this] {
            if (auto self = weak_self.lock(); self) {
                auto const lock = std::scoped_lock(mtx_);
                update_reception_unlocked();
            }
        });
        receiver_pipe_->exchange_queues(*_pipe);
        receiver_pipe_ = _pipe;
        if (local_ep_) {
            update_reception_unlocked();
        }
    } else {
        // sending pipe
        _pipe->init([weak_self = weak_from_this(), this] {
            if (auto self = weak_self.lock(); self) {
                update_sending();
            }
        });
        sender_pipe_->exchange_queues(*_pipe);
        sender_pipe_ = _pipe;
        bool binded = local_ep_.has_value();
        lock.unlock();
        if (binded) {
            update_sending();
        }
    }
}

control_data_t fake_udp_socket_handle::prepare_control_data(boost::asio::const_buffer const& _buffer,
                                                            boost::asio::ip::udp::endpoint const& _src,
                                                            boost::asio::ip::udp::endpoint const& _dst) {
    std::vector<unsigned char> input;
    input.reserve(_buffer.size());
    auto first = static_cast<const char*>(_buffer.data());
    auto const last = first + _buffer.size();
    for (; first != last; ++first) {
        input.push_back(static_cast<unsigned char>(*first));
    }

    return {.buffer_ = input, .addresses_ = std::optional<addresses>{{.src_ = _src, .dst_ = _dst}}};
}
}
