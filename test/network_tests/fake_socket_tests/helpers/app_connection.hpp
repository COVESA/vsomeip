// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "fake_tcp_socket_handle.hpp"

#include <mutex>
#include <map>
#include <set>
#include <memory>

namespace vsomeip_v3::testing {

/**
 * Handle for a connection between two applications
 *
 * Primary purpose: Ensure that testing connection options can be set
 * in advance to the actual connection setup
 **/
class app_connection {
public:
    explicit app_connection(std::string const& _name);

    /**
     * called by the socket_manager to set-up the connection and trigger
     * option settings
     **/
    void set_sockets(std::weak_ptr<fake_tcp_socket_handle> _client, std::weak_ptr<fake_tcp_socket_handle> _server);

    /**
     * will wake-up the condition_variable to check connection changes.
     * Called from the socket_manager
     **/
    void notify();

    /**
     * Injects a payload into the receiving socket
     **/
    [[nodiscard]] bool inject_command(std::vector<unsigned char> _payload) const;

    /**
     * Clears the command record for both sockets
     **/
    void clear_command_record() const;

    /**
     * Delays message processing on the socket having the corresponding role (can be set ahead of time)
     **/
    [[nodiscard]] bool delay_message_processing(bool _delay, socket_role _role);

    /**
     * sets the fake_tcp_socket_handle on each socket correspondingly (can be set ahead of time)
     **/
    [[nodiscard]] bool set_ignore_inner_close(bool _client, bool _server);

    /**
     * toggles the fake_tcp_socket_handle to (not) report an error if there is no
     * connected socket when async_receive is invoked.
     **/
    void set_ignore_nothing_to_read_from(socket_role _role, bool _ignore);

    /**
     * sets the handler on each socket (can be set ahead of time), see socket_manager::set_custom_command_handler()
     **/
    void set_custom_command_handler(vsomeip_command_handler _handler, socket_role _sender);

    /**
     * sets the fake_tcp_socket_handle on each socket correspondingly (can be set ahead of time)
     **/
    [[nodiscard]] bool block_on_close_for(std::optional<std::chrono::milliseconds> _client_block_time,
                                          std::optional<std::chrono::milliseconds> _server_block_time);

    /**
     * waits for at most _timeout milliseconds for set_sockets to have been called
     **/
    [[nodiscard]] bool wait_for_connection(std::chrono::milliseconds _timeout = std::chrono::seconds(3)) const;
    /**
     * waits until socket_count_ is at least 1, and the sockets become disconnected
     **/
    [[nodiscard]] bool wait_for_connection_drop(std::chrono::milliseconds _timeout) const;

    /**
     * waits for at most _timeout milliseconds for _id to be received by the receiving socket
     **/
    [[nodiscard]] bool wait_for_command(protocol::id_e _id, socket_role _waiting,
                                        std::chrono::milliseconds _timeout = std::chrono::seconds(3)) const;

    /**
     * waits for at most _timeout milliseconds for _id to be received as the last message by the receiving socket
     **/
    [[nodiscard]] bool wait_for_last_command(protocol::id_e _id, socket_role _waiting,
                                             std::chrono::milliseconds _timeout = std::chrono::seconds(3)) const;

    /**
     * disconnects the two sockets
     **/
    [[nodiscard]] bool disconnect(std::optional<boost::system::error_code> _client_error,
                                  std::optional<boost::system::error_code> _server_error, socket_role _side_to_disconnect);

    /**
     * counts how often set_sockets has been called
     **/
    size_t count() const;

private:
    struct connection_options {
        bool delay_message_processing_{false};
        bool ignore_inner_close_{false};
        bool ignore_nothing_to_read_from_{false};
        std::optional<std::chrono::milliseconds> block_on_close_time_{};
        vsomeip_command_handler handler_{};
    };

    bool apply_options(std::unique_lock<std::mutex> _lock);

    std::pair<std::shared_ptr<fake_tcp_socket_handle>, std::shared_ptr<fake_tcp_socket_handle>> promoted() const;

    std::string const name_;
    size_t socket_count_{0};

    std::weak_ptr<fake_tcp_socket_handle> client_;
    std::weak_ptr<fake_tcp_socket_handle> server_;

    connection_options client_options_;
    connection_options server_options_;

    std::condition_variable mutable cv_;
    std::mutex mutable mtx_;
};
}
