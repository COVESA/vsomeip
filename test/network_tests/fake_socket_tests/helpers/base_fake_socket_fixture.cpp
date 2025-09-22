// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
    for (auto& name_to_client : name_to_client_) {
        name_to_client.second->stop();
    }
    name_to_client_.clear();
    factory_->set_manager(nullptr);
}

void base_fake_socket_fixture::use_configuration(std::string const& file_name) {
    ::setenv("VSOMEIP_CONFIGURATION", file_name.c_str(), 0);
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

[[nodiscard]] bool base_fake_socket_fixture::await_connection(std::string const& _from, std::string const& _to,
                                                              std::chrono::milliseconds _timeout) {
    return socket_manager_->await_connection(_from, _to, _timeout);
}

[[nodiscard]] bool base_fake_socket_fixture::disconnect(std::string const& _from_name, std::optional<boost::system::error_code> _from_error,
                                                        std::string const& _to_name, std::optional<boost::system::error_code> _to_error,
                                                        socket_role _side_to_disconnect) {
    return socket_manager_->disconnect(_from_name, _from_error, _to_name, _to_error, _side_to_disconnect);
}

size_t base_fake_socket_fixture::connection_count(std::string const& _from, std::string const& _to) {
    return socket_manager_->count_established_connections(_from, _to);
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

[[nodiscard]] bool base_fake_socket_fixture::delay_message_processing(std::string const& _from, std::string const& _to, bool _delay) {
    return socket_manager_->delay_message_processing(_from, _to, _delay);
}

[[nodiscard]] bool base_fake_socket_fixture::set_ignore_inner_close(std::string const& _from, bool _ignore_in_from, std::string const& _to,
                                                                    bool _ignore_in_to) {
    return socket_manager_->set_ignore_inner_close(_from, _ignore_in_from, _to, _ignore_in_to);
}

[[nodiscard]] bool base_fake_socket_fixture::block_on_close_for(std::string const& _from,
                                                                std::optional<std::chrono::milliseconds> _from_block_time,
                                                                std::string const& _to,
                                                                std::optional<std::chrono::milliseconds> _to_block_time) {
    return socket_manager_->block_on_close_for(_from, _from_block_time, _to, _to_block_time);
}

void base_fake_socket_fixture::clear_command_record(std::string const& _from, std::string const& _to) {
    socket_manager_->clear_command_record(_from, _to);
}

/**
 * @see socket_manager::wait_for_command
 **/
[[nodiscard]] bool base_fake_socket_fixture::wait_for_command(std::string const& _from, std::string const& _to, protocol::id_e _id,
                                                              std::chrono::milliseconds _timeout) {
    return socket_manager_->wait_for_command(_from, _to, _id, _timeout);
}
void base_fake_socket_fixture::fail_on_bind(std::string const& _app, bool _fail) {
    socket_manager_->fail_on_bind(_app, _fail);
}
}
