// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/local_server.hpp"
#include "../include/local_acceptor.hpp"
#include "../include/local_endpoint.hpp"
#include "../include/local_socket.hpp"

#include "../../configuration/include/configuration.hpp"

#include "../../protocol/include/protocol.hpp"
#include "../../protocol/include/assign_client_command.hpp"
#include "../../protocol/include/config_command.hpp"
#include "../../protocol/include/assign_client_ack_command.hpp"

#include "../../utility/include/utility.hpp"
#include "../../utility/include/is_value.hpp"
#include "../../routing/include/routing_host.hpp"
#include "../../security/include/policy_manager_impl.hpp"
#include "../../logger/include/logger_ext.hpp"

#include <cstdint>
#include <boost/asio/error.hpp>

#include <iomanip>

#define VSOMEIP_LOG_PREFIX "ls"
namespace vsomeip_v3 {

local_server::local_server(boost::asio::io_context& _io, std::shared_ptr<local_acceptor> _acceptor,
                           std::shared_ptr<configuration> _configuration, std::weak_ptr<routing_host> _routing_host,
                           connection_handler _connection_handler, bool _is_router, std::string _server_host) :
    is_router_(_is_router), io_(_io), acceptor_(std::move(_acceptor)), configuration_(std::move(_configuration)),
    routing_host_(std::move(_routing_host)), connection_handler_(std::move(_connection_handler)), server_host_(std::move(_server_host)) { }

local_server::~local_server() = default;

void local_server::start() {
    std::scoped_lock const lock{mtx_};
    if (state_ == state_e::STARTED) {
        VSOMEIP_WARNING_P << "Rejecting the attempt to start the server when it is already started";
        return;
    }
    state_ = state_e::STARTED;
    ++lc_count_;
    start_unlock(lc_count_);
}

void local_server::stop() {
    std::scoped_lock const lock{mtx_};
    if (state_ == state_e::STOPPED) {
        VSOMEIP_WARNING_P << "Rejecting the attempt to stop the server when it is already stopped";
        return;
    }
    state_ = state_e::STOPPED;
    ++lc_count_;
    boost::system::error_code ec;
    acceptor_->close(ec);
    if (ec) {
        VSOMEIP_ERROR_P << "Error encountered: " << ec.message() << ", self: " << this;
    }
    if (debounce_) {
        debounce_->stop();
    }
}

void local_server::halt() {
    std::scoped_lock const lock{mtx_};
    if (is_value(state_).any_of(state_e::HALTED, state_e::STOPPED)) {
        VSOMEIP_WARNING_P << "Rejecting the attempt to stop the server when it is already stopped";
        return;
    }
    state_ = state_e::HALTED;
    ++lc_count_;
    boost::system::error_code ec;
    acceptor_->cancel(ec);
    if (ec) {
        VSOMEIP_ERROR_P << "Error encountered: " << ec.message() << ", self: " << this;
    }
}

void local_server::block_from(const boost::asio::ip::address& _address) {
    std::scoped_lock const lock{mtx_};
    if (auto itr = std::find(blocked_addresses_.begin(), blocked_addresses_.end(), _address); itr != blocked_addresses_.end()) {
        VSOMEIP_WARNING_P << "address " << _address.to_string() << " already in blocked list";
    } else {
        blocked_addresses_.push_back(_address);
    }
}

void local_server::allow_from(const boost::asio::ip::address& _address) {
    std::scoped_lock const lock{mtx_};
    if (auto itr = std::find(blocked_addresses_.begin(), blocked_addresses_.end(), _address); itr != blocked_addresses_.end()) {
        blocked_addresses_.erase(itr);
    } else {
        VSOMEIP_WARNING_P << "address " << _address.to_string() << " not in blocked list";
    }
}

port_t local_server::get_local_port() const {
    return acceptor_->get_local_port();
}

void local_server::set_id(client_t _id) {
    std::scoped_lock const lock{mtx_};
    own_client_id_ = _id;
}

void local_server::accept_cbk(boost::system::error_code const& _ec, std::shared_ptr<local_socket> _socket, uint32_t _lc_count) {
    if (!_ec) {
        std::scoped_lock const lock{mtx_};
        if (_lc_count != lc_count_) {
            VSOMEIP_WARNING_P << "Dropping connection from former lifecycle: " << _lc_count << ", current lc: " << lc_count_;
            _socket->stop(true);
            return;
        }

        boost::asio::ip::tcp::endpoint const peer_endpoint = _socket->peer_endpoint();
        if (std::find(blocked_addresses_.begin(), blocked_addresses_.end(), peer_endpoint.address()) != blocked_addresses_.end()) {
            VSOMEIP_WARNING_P << "connection from client @ " << peer_endpoint.address().to_string() << ":" << peer_endpoint.port()
                              << " rejected, dropping connection, self: " << this;

            _socket->stop(true);
            start_unlock(lc_count_);
            return;
        }

        auto connection = std::make_shared<tmp_connection>(std::move(_socket), is_router_, _lc_count, weak_from_this(), configuration_);
        connection->async_receive();
        start_unlock(lc_count_);
        return;
    }
    if (_ec != boost::asio::error::operation_aborted) {
        VSOMEIP_ERROR_P << "Received error: " << _ec.message() << ", self: " << this;
        if (_ec == boost::asio::error::bad_descriptor) {
            auto log_problem = [mem = this] {
                VSOMEIP_FATAL_P << "async_accept: Bad descriptors can not be dealt with. Lingering in a very bad state, self: " << mem;
                return true;
            };
            log_problem();
            std::scoped_lock const lock{mtx_};
            debounce_ = timer::create(io_, std::chrono::milliseconds(200), log_problem);
            debounce_->start();
            return;
        }
        if (_ec == boost::asio::error::no_descriptors) {
            std::scoped_lock const lock{mtx_};
            if (!debounce_) {
                debounce_ = timer::create(io_, std::chrono::milliseconds(1000), [weak_self = weak_from_this(), _lc = lc_count_] {
                    if (auto self = weak_self.lock(); self) {
                        std::scoped_lock const inner_lock{self->mtx_};
                        if (_lc == self->lc_count_) {
                            VSOMEIP_INFO_P << "Retrying to accept connections, after debounce, self: " << self.get();
                            self->start_unlock(_lc);
                        } else {
                            VSOMEIP_WARNING_P << "Stopping current retry from the lifecycle: " << _lc << ", current lc: " << self->lc_count_
                                              << ", self: " << self.get();
                        }
                    }
                    return false;
                });
            }
            VSOMEIP_WARNING_P << "Will try to accept again in 1000ms, self: " << this;
            debounce_->start();
            return;
        }
        std::scoped_lock const lock{mtx_};
        if (_lc_count == lc_count_) {
            VSOMEIP_INFO_P << "Retrying to accept connections, self: " << this;
            start_unlock(lc_count_);
        } else {
            VSOMEIP_WARNING_P << "Stopping current retry from the lifecycle: " << _lc_count << ", current lc: " << lc_count_
                              << ", self: " << this;
        }
    }
}
void local_server::add_connection(client_t _client, [[maybe_unused]] client_t _expected_id, std::shared_ptr<local_socket> _socket,
                                  std::shared_ptr<local_receive_buffer> _buffer, uint32_t _lc_count, std::string _environment) {
    std::unique_lock lock{mtx_};
    if (_lc_count == lc_count_) {
        if (_expected_id != own_client_id_ && _expected_id != VSOMEIP_CLIENT_UNSET) {
            VSOMEIP_WARNING_P << "Connection refused due to wrong client id, expected: " << hex4(_expected_id)
                              << ", actual: " << hex4(own_client_id_) << ", from client: " << hex4(_client);
            // This should not happen for the router, as the router for tcp needs to be configured, an no other
            // application should be able to claim this ip+port
            _socket->stop(true);
            return;
        }
        if (auto rh = routing_host_.lock(); rh) {
            auto const peer_endpoint = _socket->peer_endpoint();
            // Carful: Order matters. First call add_known_client (lazy load of config for this client), before trying to create
            // the endpoint (as the creation method is trying to reason whether the connection is allowed based on the loaded config).
            // Carful: These calls should happen under the lock to guarantee consistency, and before the endpoint might be removed
            // again from the map
            rh->add_known_client(_client, _environment);

            std::stringstream ss;

            if (is_router_) {
                ss << "Assigned client ID 0x" << hex4(_client) << " to \"" << utility::get_client_name(configuration_, _client) << "\" (\""
                   << _environment << "\")";

                // check if is uds socket
                if (_socket->own_port() != VSOMEIP_SEC_PORT_UNUSED) {
                    ss << " @ " << _socket->peer_endpoint().address() << ":" << _socket->peer_endpoint().port();
                } else {
                    vsomeip_sec_client_t sec_client{};
                    _socket->update(sec_client, *configuration_);
                    ss << " @ " << sec_client.user << "/" << sec_client.group;
                }
            }

            auto ep = local_endpoint::create_server_ep(local_endpoint_context{io_, configuration_, routing_host_},
                                                       local_endpoint_params{_client, rh->get_client(), std::move(_socket)},
                                                       std::move(_buffer));
            if (!ep) {
                VSOMEIP_ERROR_P << "endpoint creation failed for client: " << hex4(_client) << ", self: " << this;
                // socket is closed already in the create_server_ep on failure
                // clean-up id
                utility::release_client_id(configuration_->get_network(), _client);
                // clean-up environment
                rh->remove_known_client(_client);
                return;
            }

            if (std::find(blocked_addresses_.begin(), blocked_addresses_.end(), peer_endpoint.address()) != blocked_addresses_.end()) {
                VSOMEIP_WARNING_P << "connection from client " << hex4(_client) << ", " << ep->name() << ", " << ep->name()
                                  << " rejected, dropping, self : " << this;
                utility::release_client_id(configuration_->get_network(), _client);
                rh->remove_known_client(_client);
                ep->stop(true);
                return;
            }

            protocol::config_command config_command;
            config_command.set_client(own_client_id_);
            config_command.insert("hostname", std::string(server_host_));
            std::vector<byte_t> config_buffer;
            protocol::error_e error;
            config_command.serialize(config_buffer, error);

            if (error == protocol::error_e::ERROR_OK) {
                ep->send(&config_buffer[0], static_cast<uint32_t>(config_buffer.size()));
            }

            if (is_router_) {
                protocol::assign_client_ack_command assign_ack_command;
                assign_ack_command.set_client(VSOMEIP_ROUTING_CLIENT);
                assign_ack_command.set_assigned(_client);
                std::vector<byte_t> assign_ack_buffer;
                protocol::error_e ec;
                assign_ack_command.serialize(assign_ack_buffer, ec);

                if (error == protocol::error_e::ERROR_OK) {
                    ep->send(&assign_ack_buffer[0], static_cast<uint32_t>(assign_ack_buffer.size()));
                }

                VSOMEIP_INFO << ss.str();
            }

            // keep the lock acquired to ensure that the endpoint has been transferred.
            // This is important to ensure that on the block and now drop endpoints
            // sequence no endpoint is leaked because it was competing for the endpoint mutex
            connection_handler_(ep);
        } else {
            VSOMEIP_WARNING_P << "Dropping connection from: " << hex4(_client) << ", from former lifecycle: " << _lc_count
                              << ", current lc: " << lc_count_ << ", as routing host is no longer available self: " << this;
            _socket->stop(true);
        }
    } else {
        VSOMEIP_WARNING_P << "Dropping connection from: " << hex4(_client) << ", from former lifecycle: " << _lc_count
                          << ", current lc: " << lc_count_ << ", self: " << this;
        _socket->stop(true);
    }
}

void local_server::start_unlock(uint32_t _lc_count) {
    acceptor_->async_accept([weak_self = weak_from_this(), _lc_count](auto const& _ec, auto _socket) {
        if (auto self = weak_self.lock(); self) {
            self->accept_cbk(_ec, std::move(_socket), _lc_count);
        }
    });
}

local_server::tmp_connection::tmp_connection(std::shared_ptr<local_socket> _socket, bool _is_router, uint32_t _lc_count,
                                             std::weak_ptr<local_server> _parent, std::shared_ptr<configuration> _configuration) :
    is_router_(_is_router), lc_count_{_lc_count}, socket_(std::move(_socket)),
    receive_buffer_(std::make_shared<local_receive_buffer>(_configuration->get_max_message_size_local(),
                                                           _configuration->get_buffer_shrink_threshold())),
    parent_(std::move(_parent)), configuration_(std::move(_configuration)) { }

local_server::tmp_connection::~tmp_connection() = default;

void local_server::tmp_connection::async_receive() {
    socket_->async_receive(
            receive_buffer_->buffer(),
            [self = shared_from_this(), buffer_cp = receive_buffer_](auto const& _ec, size_t _bytes) { self->receive_cbk(_ec, _bytes); });
}

void local_server::tmp_connection::receive_cbk(boost::system::error_code const& _ec, size_t _bytes) {
    if (_ec) {
        VSOMEIP_WARNING_P << "Error encountered: " << _ec.message() << ", dropping connection: " << socket_->to_string();
        socket_->stop(true);
        return;
    }
    next_message_result result;
    if (!receive_buffer_->bump_end(_bytes)) {
        VSOMEIP_ERROR_P << "Inconsistent buffer handling, trying add the read of: " << _bytes
                        << " bytes to the buffer: " << *receive_buffer_ << ", dropping connection: " << socket_->to_string();
        socket_->stop(true);
        return;
    }
    while (receive_buffer_->next_message(result)) {
        if (result.message_data_[protocol::COMMAND_POSITION_ID] == protocol::id_e::ASSIGN_CLIENT_ID && is_router_) {
            auto client_id = assign_client(result.message_data_, result.message_size_);
            hand_over(client_id);
            return;
        } else if (result.message_data_[protocol::COMMAND_POSITION_ID] == protocol::id_e::CONFIG_ID) {
            bool matches{true};
            auto const client = read_config_command(result.message_data_, result.message_size_, matches);
            if (!matches) {
                VSOMEIP_ERROR_P << "Breaking connection > " << socket_->to_string();
                socket_->stop(true);
                return;
            }
            if (!is_router_) {
                hand_over(client);
                return;
            }
        } else {
            VSOMEIP_ERROR_P << "Unexpected command: 0x" << hex2(result.message_data_[protocol::COMMAND_POSITION_ID])
                            << " received. Breaking connection > " << socket_->to_string();
            socket_->stop(true);
            return;
        }
    }
    if (result.error_) {
        VSOMEIP_ERROR_P << "Unable to handle message length. Breaking connection > " << socket_->to_string();
        socket_->stop(true);
        return;
    }
    receive_buffer_->shift_front();
    async_receive();
}

client_t local_server::tmp_connection::assign_client(uint8_t const* _data, uint32_t _message_size) const {

    std::vector<byte_t> its_data(_data, _data + _message_size);
    protocol::assign_client_command command;
    protocol::error_e ec;

    command.deserialize(its_data, ec);
    if (ec != protocol::error_e::ERROR_OK) {
        VSOMEIP_ERROR_P << "Assign client command deserialization failed (" << static_cast<int>(ec) << ")";
        return VSOMEIP_CLIENT_UNSET;
    }

    return utility::request_client_id(configuration_, command.get_name(), command.get_client());
}

client_t local_server::tmp_connection::read_config_command(uint8_t const* _data, uint32_t _message_size, bool& _version_matches) {
    std::vector<byte_t> its_data(_data, _data + _message_size);
    protocol::config_command command;
    protocol::error_e ec;

    command.deserialize(its_data, ec);
    if (ec != protocol::error_e::ERROR_OK) {
        _version_matches = false;
        VSOMEIP_ERROR_P << "Config command deserialization failed (" << static_cast<int>(ec) << ")";
        return VSOMEIP_CLIENT_UNSET;
    }
    if (command.get_version() != protocol::IPC_VERSION) {
        _version_matches = false;
        VSOMEIP_ERROR_P << "Protocol version mismatch detected, expected: " << protocol::IPC_VERSION
                        << ", received: " << command.get_version();
        return VSOMEIP_CLIENT_UNSET;
    }
    if (command.contains("expected_id")) {
        auto str = command.at("expected_id");
        if (str.size() == sizeof(expected_id_)) {
            std::memcpy(&expected_id_, str.data(), sizeof(expected_id_));
        }
    }
    if (!command.contains("hostname")) {
        _version_matches = false;
        VSOMEIP_ERROR_P << "Config command did not contain hostname";
        return VSOMEIP_CLIENT_UNSET;
    }
    client_host_ = command.at("hostname");
    return command.get_client();
}

void local_server::tmp_connection::hand_over(client_t _client) {
    if (auto p = parent_.lock(); p) {
        p->add_connection(_client, expected_id_, std::move(socket_), receive_buffer_, lc_count_, std::move(client_host_));
    }
}
}
