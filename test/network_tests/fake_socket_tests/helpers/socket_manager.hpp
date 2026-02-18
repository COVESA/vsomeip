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

class app_connection;

using fd_t = unsigned short;

/**
 * Helper that
 * 1. connects fake socket and acceptors with app names,
 * 2. keeps track of connections,
 * 3. provides an API for error injections into the fake sockets
 **/
class socket_manager : public std::enable_shared_from_this<socket_manager> {
public:
    ~socket_manager();
    /**
     * this function will assume that the next unknown
     * io_context memory address is belongig to the passed in name.
     * This mapping is used later to identify connections between different
     * applications.
     */
    void add(std::string const& app);

    /**
     * Because some sockets are not closed explicitly
     * (e.g. local_tcp_server_endpoint sockets that were connected to),
     * this function guarantees that these sockets belonging to the application,
     * do not try to use the io_context upon closing.
     * Note that connected sockets (from other applications) are still receiving
     * connection errors.
     */
    void clear_handler(std::string const& app);

    /**
     * Waits until either the timeout expires, or the name could be associated
     * with the memory address of some boost::asio::io_context.
     * @see socket_manager::add()
     */
    [[nodiscard]] bool await_assignment(std::string const& _app, std::chrono::milliseconds _timeout = std::chrono::seconds(3));

    /**
     *
     */
    void fail_on_bind(std::string const& _app, bool fail);

    /**
     * Waits until either the timeout expires, or the application associated
     * with this name called async_accept on some fake_acceptor.
     * Useful to await the start of the routing application.
     */
    [[nodiscard]] bool await_connectable(std::string const& _app, std::chrono::milliseconds _timeout = std::chrono::seconds(3));

    /**
     * Waits until either the timeout expires, or the application associated
     * with _from established a connection to the application associated with _server.
     *
     * Note the implication is that the _server application called async_accept on
     * a fake_tcp_acceptor with some endpoint and that _from called async_connect
     * with some fake_tcp_socket towards this very endpoint.
     */
    [[nodiscard]] bool await_connection(std::string const& _client, std::string const& _server, std::chrono::milliseconds _timeout);

    /**
     * Counts how often the directed connection was established.
     */
    size_t count_established_connections(std::string const& _client, std::string const& _server);

    /**
     * Searches for a directed connection _client_name to _server_name and calls
     * fake_tcp_socket_handle::disconnect and accumulates the result.
     */
    [[nodiscard]] bool disconnect(std::string const& _client_name, std::optional<boost::system::error_code> _client_error,
                                  std::string const& _server_name, std::optional<boost::system::error_code> _server_error,
                                  socket_role _side_to_disconnect = socket_role::unspecified);

    /**
     * Injects the handed over errors on connections attemps to _app_name.
     * The first error in the vector is the first to be injected.
     **/
    void report_on_connect(std::string const& _app_name, std::vector<boost::system::error_code> _next_errors);

    /**
     * Ignores connection attemps towards _app_name for _number_of_ignored_connections
     * times. This implies that no error handler will be invoked.
     **/
    void ignore_connections(std::string const& _app_name, size_t _number_of_ignored_connections);

    /**
     * Puts the associated fake_tcp_acceptor_handle into a state in which any attempt to connect
     * to _app_name is ignored as long as _ignore_connections is set to be true.
     * This is useful to have control "how long" a client might act as if he would be suspended.
     **/
    void set_ignore_connections(std::string const& _app_name, bool _ignore_connections);

    /**
     * Ensures that write calls from _client are reported to be successful, but if _delay == true,
     * the callback of _server is not invoked, but remains waiting until _delay turns true again.
     *
     * @return false, if the connection does not exist.
     **/
    [[nodiscard]] bool delay_message_processing(std::string const& _client, std::string const& _server, bool _delay,
                                                socket_role _role = socket_role::server);

    /**
     * Ensures that a broken connection is not propagated, when the connected socket is closed.
     * The connection is identified by:
     * _client -> _server.
     * if _ignore_in_client is true, the _client socket will ignore closings of _server,
     * if _ignore_in_server is true, the _server socket will ignore closings of _client.
     *
     * Closing can later be triggered with disconnect()
     **/
    [[nodiscard]] bool set_ignore_inner_close(std::string const& _client, bool _ignore_in_client, std::string const& _server,
                                              bool _ignore_in_server);

    /**
     * Ensures that a async_receive will not fail, if the other socket disconnected.
     * This is helpful for simulating suspend sequences.
     * Note: This option is permanent to the connection and needs to be actively reset.
     **/
    void set_ignore_nothing_to_read_from(std::string const& _client, std::string const& _server, socket_role _role, bool _ignore);

    /**
     * searches for the _client -> _server connected sockets and demands from _client to block execution
     * for _client_block_time when close is invoked, equivalent for _server with _server_block_time.
     * @see fake_tcp_socket_handle::block_on_close_for() for further details.
     *
     * @return true, if all non nullopts could be forwarded
     **/
    [[nodiscard]] bool block_on_close_for(std::string const& _client, std::optional<std::chrono::milliseconds> _client_block_time,
                                          std::string const& _server, std::optional<std::chrono::milliseconds> _server_block_time);

    /**
     * Clears all received commands in the _server socket from the _client -> _server connection
     **/
    void clear_command_record(std::string const& _client, std::string const& _server);

    /**
     * Waits for _id to be received in the _client -> _server connection for _timeout amount of time.
     * @return false, if the _id was not received within time.
     **/
    [[nodiscard]] bool wait_for_command(std::string const& _client, std::string const& _server, protocol::id_e _id, socket_role _waiting,
                                        std::chrono::milliseconds _timeout = std::chrono::seconds(3));

    /**
     * Waits for _id to be the last received message in the _client -> _server connection for _timeout amount of time.
     * @return false, if the _id was not received within time.
     **/
    [[nodiscard]] bool wait_for_last_command(std::string const& _client, std::string const& _server, socket_role _waiting,
                                             protocol::id_e _id, std::chrono::milliseconds _timeout = std::chrono::seconds(3));

    /**
     * Waits for the _client -> _server connection to be dropped.
     * If there is no record of this connection it first awaited to have this connection established,
     * If there is currently a connection it is waited until one socket disconnects,
     * If there is no longer any connection true is returned.
     **/
    [[nodiscard]] bool wait_for_connection_drop(std::string const& _client, std::string const& _server,
                                                std::chrono::milliseconds _timeout = std::chrono::seconds(3));

    /**
     * Set whether a write on a disconnected socket should result in a silent error, or a broken pipe
     */
    void set_ignore_broken_pipe(std::string const& _app_name, bool _set);

    /**
     * Called by a fake_tcp_socket_handle when a broken pipe situation occurs.
     * @return true, if the broken pipe should be ignored.
     **/
    [[nodiscard]] bool ignore_broken_pipe(fake_tcp_socket_handle const& _handle);

    /**
     * associates a fake_tcp_socket_handle to a io_context and therefore to an app_name.
     **/
    void add_socket(std::weak_ptr<fake_tcp_socket_handle> _state, boost::asio::io_context* _io);

    /**
     * removes the fd from the internal map of fds.
     **/
    void remove(fd_t fd);

    /**
     * associates a fake_tcp_acceptor_handle to a io_context and therefore to an app_name.
     **/
    void add_acceptor(std::weak_ptr<fake_tcp_acceptor_handle> _state, boost::asio::io_context* _io);

    /**
     * removes the fd from the internal map of fds.
     **/
    void remove_acceptor(fd_t _fd, boost::asio::ip::tcp::endpoint _ep);

    /**
     * associates the fake_tcp_acceptor_handle to the endpoint. This allows fake_tcp_socket_handles
     * to try to connect to the acceptor.
     **/
    [[nodiscard]] bool bind_acceptor(boost::asio::ip::tcp::endpoint const& _ep, std::weak_ptr<fake_tcp_acceptor_handle> _state);

    /**
     **/
    [[nodiscard]] bool bind_socket(fake_tcp_socket_handle const& _handle);

    /**
     * Searches for a fake_tcp_acceptor_handle @see socket_manager::bind_acceptor(),
     * and forwards the connect request from the passed in handle.
     **/
    void connect(boost::asio::ip::tcp::endpoint const& _ep, fake_tcp_socket_handle& _state, connect_handler _handler);

    /**
     * Helper to let the socket_manager know that some acceptor started to wait for connections.
     **/
    void awaiting();

    /**
     * Prepares a drop for the specific vsomeip command @param _id from @param _from towards @param _to, gives a future for when it happens
     *
     * NOTE: not composeable, difficut to use, take care..
     */
    std::future<protocol::id_e> drop_command_once(std::string const& _from, std::string const& _to, protocol::id_e _id);

    /**
     * Forces the delivery of a vsomeip message @param _payload from @param _client to @param _server.
     */
    bool inject_command(std::string const& _client, std::string const& _server, std::vector<unsigned char>& _payload);

    /**
     * Allows setting a custom vsomeip command controller @param _handler to be invoked every time a message
     * is parsed, enables the test to decide what can be delivered or to assert based on the payload.
     * The _sender argument specifies which messages to parse.
     * if _sender == client -> only parses messages from the client to the server
     * if _sender == server -> reverse
     * if _sender == unspecified -> both
     */
    void set_custom_command_handler(std::string const& _client, std::string const& _server, vsomeip_command_handler const& _handler,
                                    socket_role _sender = socket_role::unspecified);

    /**
     * Invoked by a connected socket upon closing the connection
     **/
    void close_connection(std::string const& _one, std::string const& _two, socket_role _closing);

private:
    void try_add(boost::asio::io_context* _io, fd_t _fd, char const* _type);
    std::shared_ptr<app_connection> get_or_create_connection(std::string const& _client, std::string const& _server);

    std::mutex mtx_;
    std::condition_variable assignment_cv_;
    std::condition_variable connectable_cv_;
    std::atomic<fd_t> next_fd_{1};
    std::map<fd_t, std::weak_ptr<fake_tcp_socket_handle>> fd_to_handle_;
    std::map<fd_t, std::weak_ptr<fake_tcp_acceptor_handle>> fd_to_acceptor_states_;
    std::map<boost::asio::ip::tcp::endpoint, std::weak_ptr<fake_tcp_acceptor_handle>> ep_to_acceptor_states_;
    std::map<std::string, boost::asio::io_context*> name_to_context_;
    std::map<boost::asio::io_context*, std::string> context_to_name_;
    std::map<boost::asio::io_context*, std::vector<fd_t>> context_to_fd_;
    std::map<std::string, size_t> connection_name_to_connection_count_;
    std::map<std::string, std::shared_ptr<app_connection>> connections_;
    // these timers are not supposed to ever expire.
    // Instead the callback lifetime of these timers ensures that
    // the socket_manager will be notified once the io_context is destroyed,
    // which is then used to ensure that the referenced io_context (that is now invalid),
    // is no longer used to schedule any other task.
    std::map<std::string, std::unique_ptr<boost::asio::steady_timer>> timers_;

    std::map<std::string, size_t> app_name_to_ignore_connections_count_;
    std::map<std::string, std::vector<boost::system::error_code>> app_to_next_connection_errors_;
    std::set<std::string> connections_to_ignore_;
    std::set<std::string> fail_on_bind_;
    std::set<std::string> ignore_broken_pipe_;
};
}
