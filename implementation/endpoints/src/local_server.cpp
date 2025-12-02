// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#include "../../routing/include/routing_host.hpp"
#include "../../security/include/policy_manager_impl.hpp"

#include <cstdint>
#include <vsomeip/internal/logger.hpp>
#include <boost/asio/error.hpp>

#include <iomanip>

namespace vsomeip_v3 {

local_server::local_server(boost::asio::io_context& _io, std::shared_ptr<local_acceptor> _acceptor,
                           std::shared_ptr<configuration> _configuration, std::weak_ptr<routing_host> _routing_host,
                           std::weak_ptr<endpoint_host> _endpoint_host, bool _is_router) :
    is_router_(_is_router), io_(_io), acceptor_(std::move(_acceptor)), configuration_(std::move(_configuration)),
    routing_host_(std::move(_routing_host)), endpoint_host_(_endpoint_host) { }

local_server::~local_server() = default;

void local_server::start() {
    std::scoped_lock const lock{mtx_};
    start_unlock(lc_count_);
}

void local_server::stop() {
    std::scoped_lock const lock{mtx_};
    ++lc_count_;
    boost::system::error_code ec;
    acceptor_->close(ec);
    if (ec) {
        VSOMEIP_ERROR << "ls::" << __func__ << ": Error encountered: " << ec.message() << ", self: " << this;
    }
    stop_client_connections_unlock();
    if (debounce_) {
        debounce_->stop();
    }
}

void local_server::halt() {
    std::scoped_lock const lock{mtx_};
    ++lc_count_;
    boost::system::error_code ec;
    acceptor_->cancel(ec);
    if (ec) {
        VSOMEIP_ERROR << "ls::" << __func__ << ": Error encountered: " << ec.message() << ", self: " << this;
    }
    stop_client_connections_unlock();
}

void local_server::disconnect_from(client_t _client, bool _due_to_error) {
    std::scoped_lock lock{mtx_};
    if (clients_.find(_client) == clients_.end()) {
        return;
    }
    clients_.at(_client)->stop(_due_to_error);
    clients_.erase(_client);
}

port_t local_server::get_local_port() const {
    return acceptor_->get_local_port();
}

void local_server::print_status() const {
    std::scoped_lock const lock{mtx_};
    VSOMEIP_INFO << "ls::" << __func__ << ": lc: " << lc_count_ << ", connected clients: " << clients_.size() << ", self: " << this;
}

void local_server::accept_cbk(boost::system::error_code const& _ec, std::shared_ptr<local_socket> _socket, uint32_t _lc_count) {
    if (!_ec) {
        std::scoped_lock const lock{mtx_};
        if (_lc_count != lc_count_) {
            VSOMEIP_WARNING << "ls::" << __func__ << ": Dropping connection from former lifecycle: " << _lc_count
                            << ", current lc: " << lc_count_;
            _socket->stop(true);
            return;
        }
        auto connection = std::make_shared<tmp_connection>(std::move(_socket), is_router_, _lc_count, weak_from_this(), configuration_);
        connection->async_receive();
        start_unlock(lc_count_);
        return;
    }
    if (_ec != boost::asio::error::operation_aborted) {
        VSOMEIP_ERROR << "ls::" << __func__ << ": received error: " << _ec.message() << ", self: " << this;
        if (_ec == boost::asio::error::bad_descriptor) {
            auto log_problem = [mem = this] {
                VSOMEIP_FATAL << "ls::async_accept: Bad descriptors can not be dealt with. Lingering in a very bad state"
                              << ", self: " << mem;
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
                            VSOMEIP_INFO << "ls::" << __func__ << ": Retrying to accept connections, after debounce, self: " << self.get();
                            self->start_unlock(_lc);
                        } else {
                            VSOMEIP_WARNING << "ls::" << __func__ << ": Stopping current retry from the lifecycle: " << _lc
                                            << ", current lc: " << self->lc_count_ << ", self: " << self.get();
                        }
                    }
                    return false;
                });
            }
            VSOMEIP_ERROR << "ls::" << __func__ << ": Will try to accept again in 1000ms, self: " << this;
            debounce_->start();
            return;
        }
        std::scoped_lock const lock{mtx_};
        if (_lc_count == lc_count_) {
            VSOMEIP_INFO << "ls::" << __func__ << ": Retrying to accept connections, self: " << this;
            start_unlock(lc_count_);
        } else {
            VSOMEIP_WARNING << "ls::" << __func__ << ": Stopping current retry from the lifecycle: " << _lc_count
                            << ", current lc: " << lc_count_ << ", self: " << this;
        }
    }
}
void local_server::add_connection(client_t _client, std::shared_ptr<local_socket> _socket, std::shared_ptr<local_receive_buffer> _buffer,
                                  uint32_t _lc_count, std::string _environment) {

    std::unique_lock lock{mtx_};
    if (_lc_count == lc_count_) {
        if (auto rh = routing_host_.lock(); rh) {
            auto const peer_endpoint = _socket->peer_endpoint();
            auto ep = local_endpoint::create_server_ep(local_endpoint_context{io_, configuration_, routing_host_, endpoint_host_},
                                                       local_endpoint_params{_client, std::move(_socket)}, std::move(_buffer), is_router_);
            if (!ep) {
                VSOMEIP_ERROR << "ls::" << __func__ << ": endpoint creation failed for client: " << std::hex << std::setfill('0')
                              << std::setw(4) << _client << ", self: " << this;
                // socket is closed already in the create_server_ep on failure
                return;
            }
            // Carful: These calls should happen under the lock to guarantee consistency, and before the endpoint might be removed
            // again from the map
            rh->add_known_client(_client, _environment);
            if (peer_endpoint != boost::asio::ip::tcp::endpoint{}) {
                rh->add_guest(_client, peer_endpoint.address(), peer_endpoint.port() - 1); // -1 taken over from the legacy
            }

            if (auto it = clients_.find(_client); it != clients_.end()) {
                VSOMEIP_WARNING << "ls::" << __func__ << ": Replacing already existing connection to client " << std::hex
                                << std::setfill('0') << std::setw(4) << _client << ", previous connection: " << it->second->name() << "/"
                                << it->second.get() << ", new connection " << ep->name() << "/" << ep.get() << ", self: " << this;
                // Carful: A lock release should not be necessary, as stop of an endpoint must not invoke the error handler
                it->second->stop(true);
            } else {
                VSOMEIP_INFO << "ls::" << __func__ << ": Received a connection from " << std::hex << std::setfill('0') << std::setw(4)
                             << _client << ", new connection " << ep->name() << "/" << ep.get() << ", self: " << this;
            }
            clients_[_client] = ep;
            ep->register_error_handler([weak_self = weak_from_this(), weak_ep = std::weak_ptr<local_endpoint>(ep), _client] {
                if (auto server = weak_self.lock(); server) {
                    if (auto shared_ep = weak_ep.lock(); shared_ep) {
                        server->remove_connection(_client, shared_ep);
                    }
                }
            });
            // Carful: A call to start might end up calling the error handler causing a lock inversion
            lock.unlock();
            ep->start();
        } else {
            VSOMEIP_WARNING << "ls::" << __func__ << ": Dropping connection from: " << std::hex << std::setfill('0') << std::setw(4)
                            << _client << ", from former lifecycle: " << _lc_count << ", current lc: " << lc_count_
                            << ", as routing host is no longer available self: " << this;
            _socket->stop(true);
        }
    } else {
        VSOMEIP_WARNING << "ls::" << __func__ << ": Dropping connection from: " << std::hex << std::setfill('0') << std::setw(4) << _client
                        << ", from former lifecycle: " << _lc_count << ", current lc: " << lc_count_ << ", self: " << this;
        _socket->stop(true);
    }
}

void local_server::remove_connection(client_t _client, std::shared_ptr<local_endpoint> _ep) {
    std::scoped_lock lock{mtx_};
    if (auto it = clients_.find(_client); it == clients_.end()) {
        VSOMEIP_WARNING << "ls::" << __func__ << ": Client " << std::hex << std::setfill('0') << std::setw(4) << _client
                        << " has no registered connection to "
                        << " remove, endpoint > " << this;
    } else {
        if (_ep.get() != it->second.get()) {
            VSOMEIP_WARNING << "ls::" << __func__ << ": Client " << std::hex << std::setfill('0') << std::setw(4) << _client
                            << " has a different recorded connection. Not removing the currently recorded connection: "
                            << it->second->name() << "/" << it->second.get() << ", stopped connection " << _ep->name() << "/" << _ep.get()
                            << ", self: " << this;
        } else {
            clients_.erase(it);
            configuration_->get_policy_manager()->remove_client_to_sec_client_mapping(_client);
        }
    }
    // irrespective of whether this connection was known, the endpoint should be stopped
    _ep->stop(true);
}

void local_server::stop_client_connections_unlock() {
    for (const auto& [key, ep] : clients_) {
        ep->stop(false);
    }
    clients_.clear();
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
        VSOMEIP_ERROR << "ls::" << __func__ << ": Error encountered: " << _ec.message()
                      << ", dropping connection: " << socket_->to_string();
        socket_->stop(true);
        return;
    }
    next_message_result result;
    if (!receive_buffer_->bump_end(_bytes)) {
        VSOMEIP_ERROR << "ls::" << __func__ << ": inconsistent buffer handling, trying add the read of: " << _bytes
                      << " bytes to the buffer: " << *receive_buffer_ << ", dropping connection: " << socket_->to_string();
        socket_->stop(true);
        return;
    }
    if (receive_buffer_->next_message(result)) {
        if ((*receive_buffer_)[protocol::COMMAND_POSITION_ID] == protocol::id_e::ASSIGN_CLIENT_ID && is_router_) {
            auto client_id = assign_client(result.message_size_);

            std::stringstream ss;
            ss << "ls::" << __func__ << ": Assigned client ID " << std::hex << std::setw(4) << client_id << " to \""
               << utility::get_client_name(configuration_, client_id) << "\"";

            // check if is uds socket
            if (socket_->own_port() != VSOMEIP_SEC_PORT_UNUSED) {
                ss << " @ " << socket_->peer_endpoint().address() << ":" << std::dec << socket_->peer_endpoint().port();
            }

            VSOMEIP_INFO << ss.str();

            send_client_id(client_id);
        } else if ((*receive_buffer_)[protocol::COMMAND_POSITION_ID] == protocol::id_e::CONFIG_ID && !is_router_) {
            confirm_connection(result.message_size_);
        } else {
            VSOMEIP_ERROR << "ls::" << __func__ << ": Unexpected command: " << (*receive_buffer_)[protocol::COMMAND_POSITION_ID]
                          << " received. Breaking connection > " << socket_->to_string();
            socket_->stop(true);
        }
        return;
    }
    if (result.error_) {
        VSOMEIP_ERROR << "ls::" << __func__ << ": Unable to handle message length. Breaking connection > " << socket_->to_string();
        socket_->stop(true);
        return;
    }
    async_receive();
}

client_t local_server::tmp_connection::assign_client(size_t _message_size) const {

    std::vector<byte_t> its_data(&(*receive_buffer_)[0], (&(*receive_buffer_)[0]) + _message_size);
    protocol::assign_client_command command;
    protocol::error_e ec;

    command.deserialize(its_data, ec);
    if (ec != protocol::error_e::ERROR_OK) {
        VSOMEIP_ERROR << "ls::" << __func__ << ": assign client command deserialization failed (" << std::dec << static_cast<int>(ec)
                      << ")";
        return VSOMEIP_CLIENT_UNSET;
    }

    return utility::request_client_id(configuration_, command.get_name(), command.get_client());
}

void local_server::tmp_connection::confirm_connection(size_t _message_size) {
    std::vector<byte_t> its_data(&(*receive_buffer_)[0], (&(*receive_buffer_)[0]) + _message_size);
    protocol::config_command command;
    protocol::error_e ec;

    command.deserialize(its_data, ec);
    if (ec != protocol::error_e::ERROR_OK) {
        VSOMEIP_ERROR << "ls::" << __func__ << ": config command deserialization failed (" << std::dec << static_cast<int>(ec) << ")";
        return;
    }
    if (!command.contains("hostname")) {
        VSOMEIP_ERROR << "ls::" << __func__ << ": config command did not contain hostname";
        return;
    }
    hand_over(command.get_client(), command.at("hostname"));
}

void local_server::tmp_connection::send_client_id(client_t _client) {
    protocol::assign_client_ack_command command;
    command.set_client(VSOMEIP_ROUTING_CLIENT);
    command.set_assigned(_client);

    std::vector<byte_t> buffer;
    protocol::error_e ec;
    command.serialize(buffer, ec);
    if (ec != protocol::error_e::ERROR_OK) {
        VSOMEIP_ERROR << "ls::" << __func__ << ": assign client ack command serialization failed (" << std::dec << static_cast<int>(ec)
                      << ")";
        return;
    }

    VSOMEIP_DEBUG << "ls::send_client_id: dispatching client id: " << std::hex << std::setfill('0') << std::setw(4) << _client;
    socket_->async_send(std::move(buffer), [self = shared_from_this(), this, _client](auto const& _ec, size_t, auto) {
        if (!_ec) {
            hand_over(_client, "");
            return;
        }
        VSOMEIP_WARNING << "ls::send_client_id: Received error: " << _ec.message();
        // notice that after this call the last shared_ptr should be cleaned and the tmp_connection is cleared.
    });
}

void local_server::tmp_connection::hand_over(client_t _client, std::string _environment) {
    if (auto p = parent_.lock(); p) {
        p->add_connection(_client, std::move(socket_), receive_buffer_, lc_count_, std::move(_environment));
    }
}
}
