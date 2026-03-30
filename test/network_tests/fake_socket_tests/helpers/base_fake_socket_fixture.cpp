// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "base_fake_socket_fixture.hpp"
#include "test_logging.hpp"

#define LOCAL_LOG TEST_LOG << "[fixture] "
namespace vsomeip_v3::testing {
std::shared_ptr<fake_socket_factory> base_fake_socket_fixture::factory_ = std::make_shared<fake_socket_factory>();

void base_fake_socket_fixture::SetUpTestSuite() {
    vsomeip_v3::set_abstract_factory(factory_);
}

base_fake_socket_fixture::base_fake_socket_fixture() {
    factory_->set_manager(socket_manager_);
}

base_fake_socket_fixture::~base_fake_socket_fixture() {
    // stop normal applications first, let them deregister
    for (auto& name_to_client : name_to_client_) {
        if (!name_to_client.second->is_router()) {
            name_to_client.second->stop();
        }
    }
    // stop routing applications
    for (auto& name_to_client : name_to_client_) {
        if (name_to_client.second->is_router()) {
            name_to_client.second->stop();
        }
    }
    name_to_client_.clear();
    factory_->set_manager(nullptr);
}

void base_fake_socket_fixture::use_configuration(std::string const& file_name) {
    ::setenv("VSOMEIP_CONFIGURATION", file_name.c_str(), 1);
}

void base_fake_socket_fixture::create_app(std::string const& _name) {
    name_to_client_[_name] = std::make_unique<app>(_name);
    LOCAL_LOG << "Application: " << _name << " is created";
}

app* base_fake_socket_fixture::start_client(std::string const& _name) {
    LOCAL_LOG << "Application: " << _name << " will be started";
    auto const it = name_to_client_.find(_name);
    if (it == name_to_client_.end()) {
        LOCAL_LOG << "no app created with this name";
        return nullptr;
    }
    socket_manager_->add(_name);
    if (!it->second->start()) {
        LOCAL_LOG << "Application: " << _name << " could not be started";
        name_to_client_.erase(_name);
    }
    if (!socket_manager_->await_assignment(_name)) {
        LOCAL_LOG << "Application: " << _name << " could not be assigned to an io_context";
        name_to_client_.erase(_name);
    }
    LOCAL_LOG << "Application: " << _name << " is considered started";
    return it->second.get();
}

void base_fake_socket_fixture::stop_client(std::string const _name) {
    auto it = name_to_client_.find(_name);
    if (it == name_to_client_.end()) {
        return;
    }
    it->second->stop();
    name_to_client_.erase(_name);
}

[[nodiscard]] bool base_fake_socket_fixture::await_connectable(std::string const& _name, std::chrono::milliseconds _timeout) {
    auto const it = name_to_client_.find(_name);
    if (it == name_to_client_.end()) {
        LOCAL_LOG << "Application: " << _name << " was not created. It can not be awaited to be connectable for other clients.";
        return false;
    }
    return socket_manager_->await_connectable(_name, _timeout);
}

[[nodiscard]] bool base_fake_socket_fixture::await_connection(std::string const& _client, std::string const& _server,
                                                              std::chrono::milliseconds _timeout) {
    return socket_manager_->await_connection(_client, _server, _timeout);
}

[[nodiscard]] bool base_fake_socket_fixture::disconnect(std::string const& _client_name,
                                                        std::optional<boost::system::error_code> _client_error,
                                                        std::string const& _server_name,
                                                        std::optional<boost::system::error_code> _server_error,
                                                        socket_role _side_to_disconnect) {
    return socket_manager_->disconnect(_client_name, _client_error, _server_name, _server_error, _side_to_disconnect);
}

size_t base_fake_socket_fixture::connection_count(std::string const& _client, std::string const& _server) {
    return socket_manager_->count_established_connections(_client, _server);
}

void base_fake_socket_fixture::report_on_connect(std::string const& _app_name, std::vector<boost::system::error_code> _next_errors) {
    socket_manager_->report_on_connect(_app_name, std::move(_next_errors));
}

void base_fake_socket_fixture::ignore_connections(std::string const& _app_name, size_t _number_of_ignored_connections) {
    socket_manager_->ignore_connections(_app_name, _number_of_ignored_connections);
}

void base_fake_socket_fixture::set_ignore_connections(std::string const& _app_name, bool _ignore_connections) {
    socket_manager_->set_ignore_connections(_app_name, _ignore_connections);
}

[[nodiscard]] bool base_fake_socket_fixture::delay_message_processing(std::string const& _client, std::string const& _server, bool _delay,
                                                                      socket_role _role) {
    return socket_manager_->delay_message_processing(_client, _server, _delay, _role);
}

[[nodiscard]] bool base_fake_socket_fixture::delay_boardnet_sending(boost::asio::ip::udp::endpoint const& _ep, bool _delay) {
    return socket_manager_->delay_boardnet_sending(_ep, _delay);
}

[[nodiscard]] bool base_fake_socket_fixture::set_ignore_inner_close(std::string const& _client, bool _ignore_in_client,
                                                                    std::string const& _server, bool _ignore_in_server) {
    return socket_manager_->set_ignore_inner_close(_client, _ignore_in_client, _server, _ignore_in_server);
}

void base_fake_socket_fixture::set_ignore_nothing_to_read_from(std::string const& _client, std::string const& _server, socket_role _role,
                                                               bool _ignore) {
    return socket_manager_->set_ignore_nothing_to_read_from(_client, _server, _role, _ignore);
}

[[nodiscard]] bool base_fake_socket_fixture::block_on_close_for(std::string const& _client,
                                                                std::optional<std::chrono::milliseconds> _client_block_time,
                                                                std::string const& _server,
                                                                std::optional<std::chrono::milliseconds> _server_block_time) {
    return socket_manager_->block_on_close_for(_client, _client_block_time, _server, _server_block_time);
}

void base_fake_socket_fixture::clear_command_record(std::string const& _client, std::string const& _server) {
    socket_manager_->clear_command_record(_client, _server);
}

bool base_fake_socket_fixture::setup_data_pipe(std::string const& _client, std::string const& _server, socket_role _applied_on,
                                               std::shared_ptr<data_pipe> const& _pipe) {
    return socket_manager_->setup_data_pipe(_client, _server, _applied_on, _pipe);
}

[[nodiscard]] bool base_fake_socket_fixture::wait_for_command(std::string const& _client, std::string const& _server, protocol::id_e _id,
                                                              socket_role _waiting, std::chrono::milliseconds _timeout) {
    return socket_manager_->wait_for_command(_client, _server, _id, _waiting, _timeout);
}

[[nodiscard]] bool base_fake_socket_fixture::wait_for_last_command(std::string const& _client, std::string const& _server,
                                                                   socket_role _waiting, protocol::id_e _id,
                                                                   std::chrono::milliseconds _timeout) {
    return socket_manager_->wait_for_last_command(_client, _server, _waiting, _id, _timeout);
}

[[nodiscard]] bool base_fake_socket_fixture::wait_for_connection_drop(std::string const& _client, std::string const& _server,
                                                                      std::chrono::milliseconds _timeout) {
    return socket_manager_->wait_for_connection_drop(_client, _server, _timeout);
}

void base_fake_socket_fixture::fail_on_bind(std::string const& _app, bool _fail) {
    socket_manager_->fail_on_bind(_app, _fail);
}

void base_fake_socket_fixture::set_ignore_broken_pipe(std::string const& _app_name, bool _set) {
    socket_manager_->set_ignore_broken_pipe(_app_name, _set);
}
std::future<protocol::id_e> base_fake_socket_fixture::drop_command_once(std::string const& _from, std::string const& _to,
                                                                        protocol::id_e _id) {
    return socket_manager_->drop_command_once(_from, _to, _id);
}

void base_fake_socket_fixture::set_custom_command_handler(std::string const& _client, std::string const& _server,
                                                          vsomeip_command_handler const& _handler, socket_role _sender) {
    socket_manager_->set_custom_command_handler(_client, _server, _handler, _sender);
}

void base_fake_socket_fixture::inject_command(std::string const& _client, std::string const& _server,
                                              std::vector<unsigned char>& _payload) {
    socket_manager_->inject_command(_client, _server, _payload);
}
}
