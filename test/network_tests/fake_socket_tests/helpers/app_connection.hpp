// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "sockets/fake_tcp_socket_handle.hpp"
#include "data_pipe.hpp"
#include "common/timeout_scale.hpp"

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
     * Injects a payload into the socket identified by @param _target_role..
     **/
    [[nodiscard]] bool inject_command(std::vector<unsigned char> _payload, socket_role _target_role = socket_role::server) const;

    /**
     * Injects a payload into the receiving socket (no parsing)
     **/
    [[nodiscard]] bool inject_message(std::vector<unsigned char> _payload) const;

    /**
     * Clears the command record for both sockets
     **/
    void clear_command_record() const;

    /**
     * Delays message processing on the socket having the corresponding role (can be set ahead of time)
     **/
    [[nodiscard]] bool delay_message_processing(bool _delay, socket_role _role);

    /**
     * Holds (or releases) the boardnet write completions on the socket having the corresponding
     * role (can be set ahead of time). While held, the asynchronous send callbacks that drive
     * server_endpoint_impl::send_cbk are stashed and only fired once released, modelling a slow
     * in-flight write. @see fake_tcp_socket_handle::delay_boardnet_completion.
     **/
    [[nodiscard]] bool delay_boardnet_completion(bool _delay, socket_role _role);

    /**
     * Number of boardnet write completions currently held on the socket of the given role.
     **/
    [[nodiscard]] size_t held_boardnet_completion_count(socket_role _role) const;

    /**
     * Delays send-completion callbacks on the socket having the corresponding role (can be set ahead of time)
     **/
    [[nodiscard]] bool delay_sending(bool _delay, socket_role _role);

    /**
     * sets the fake_tcp_socket_handle on each socket correspondingly (can be set ahead of time)
     **/
    [[nodiscard]] bool set_ignore_inner_close(bool _client, bool _server);

    /**
     * toggles the fake_tcp_socket_handle to (not) report an error if there is no
     * connected socket when async_receive is invoked.
     * if ignore == true:
     *      if _role == client or unspecified -> client will ignore the error
     *      if _role == server or unspecified -> server will ignore the error
     **/
    void set_ignore_nothing_to_read_from(socket_role _role, bool _ignore);

    /**
     * sets the handler on each socket (can be set ahead of time), see socket_manager::set_custom_command_handler()
     **/
    void set_custom_command_handler(vsomeip_command_handler _handler, socket_role _sender);

    /**
     * Installs _pipe on the socket identified by _applied_on (client or server).
     * Can be called before the connection is established; the pipe will be applied
     * when the socket forms. Returns true if staged successfully (socket not yet
     * connected), false if the socket already exists (applying to a live socket
     * risks data loss and is not allowed) or if _applied_on is unspecified.
     **/
    bool setup_data_pipe(std::shared_ptr<data_pipe> const& _pipe, socket_role _applied_on);

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
     * Monotonic count of drop edges on this link.
     */
    size_t disconnect_count() const;

    /**
     * Waits until disconnect_count() exceeds _baseline.
     */
    [[nodiscard]] bool wait_for_disconnect_since(size_t _baseline, std::chrono::milliseconds _timeout) const;

    /**
     * Records one drop edge and wakes waiters. Called once per drop from on_disconnect.
     */
    void record_disconnect();

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

    /**
     * Returns the socket type of the connection if available
     **/
    [[nodiscard]] std::optional<socket_type> get_socket_type() const;

    /**
     * Disconnects if either the local or remote address is equal to the provided one,
     * but not if they both are (as they are an "internal" connection)
     */
    bool disconnect_from(boost::asio::ip::address _address);

    /**
     * Blocks/Unblocks (re)connections to provided address
     */
    void block_reconnect_to_ip(boost::asio::ip::address _address, bool _block_reconnect);

private:
    struct connection_options {
        bool delay_message_processing_{false};
        bool delay_boardnet_completion_{false};
        bool delay_sending_{false};
        bool ignore_inner_close_{false};
        bool ignore_nothing_to_read_from_{false};
        std::optional<std::chrono::milliseconds> block_on_close_time_{};
        vsomeip_command_handler handler_{};
        std::shared_ptr<data_pipe> data_pipe_{};
    };

    bool apply_options(std::unique_lock<std::mutex> _lock);

    std::pair<std::shared_ptr<fake_tcp_socket_handle>, std::shared_ptr<fake_tcp_socket_handle>> promoted() const;

    std::string const name_;
    size_t socket_count_{0};
    size_t disconnect_count_{0}; // drop-edge count, see record_disconnect()

    std::weak_ptr<fake_tcp_socket_handle> client_;
    std::weak_ptr<fake_tcp_socket_handle> server_;

    connection_options client_options_;
    connection_options server_options_;

    std::condition_variable mutable cv_;
    std::mutex mutable mtx_;
};

/**
 * Edge-based connection-drop detection. Create via watch_connection_drop() before the triggering
 * action; wait() then blocks until a drop occurs, regardless of reconnect timing.
 **/
class connection_drop_watch {
public:
    connection_drop_watch(std::shared_ptr<app_connection> _connection, size_t _baseline) :
        connection_(std::move(_connection)), baseline_(_baseline) { }

    /// Waits for a drop edge recorded after the watch was created. The default timeout is scaled
    /// by TEST_TIMEOUT_SCALE so it holds up under valgrind/slow CI.
    [[nodiscard]] bool wait(std::chrono::milliseconds _timeout = common::scaled_timeout(std::chrono::seconds(6))) const {
        return connection_ && connection_->wait_for_disconnect_since(baseline_, _timeout);
    }

private:
    std::shared_ptr<app_connection> connection_;
    size_t baseline_{0};
};
}
