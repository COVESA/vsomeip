// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_TESTING_SOCKET_MANAGER_HPP_
#define VSOMEIP_V3_TESTING_SOCKET_MANAGER_HPP_

#include "fake_tcp_socket_handle.hpp"

#include <mutex>
#include <map>
#include <set>
#include <memory>

namespace vsomeip_v3::testing {

using fd_t = unsigned short;

/**
 * Helper that
 * 1. connects fake socket and acceptors with app names,
 * 2. keeps track of connections,
 * 3. provides an API for error injections into the fake sockets
 **/
class socket_manager : public std::enable_shared_from_this<socket_manager> {
public:
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
     * with _from established a connection to the application associated with _to.
     *
     * Note the implication is that the _to application called async_accept on
     * a fake_tcp_acceptor with some endpoint and that _from called async_connect
     * with some fake_tcp_socket towards this very endpoint.
     */
    [[nodiscard]] bool await_connection(std::string const& _from, std::string const& _to, std::chrono::milliseconds _timeout);

    /**
     * Counts how often the directed connection was established.
     */
    size_t count_established_connections(std::string const& _from, std::string const& _to);

    /**
     * Searches for a directed connection _from_name to _to_name and calls
     * fake_tcp_socket_handle::disconnect and accumulates the result.
     */
    [[nodiscard]] bool disconnect(std::string const& _from_name, std::optional<boost::system::error_code> _from_error,
                                  std::string const& _to_name, std::optional<boost::system::error_code> _to_error,
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
     * Ensures that write calls from _from are reported to be successful, but if _delay == true,
     * the callback of _to is not invoked, but remains waiting until _delay turns true again.
     *
     * @return false, if the connection does not exist.
     **/
    [[nodiscard]] bool delay_message_processing(std::string const& _from, std::string const& _to, bool _delay);

    /**
     * Ensures that a broken connection is not propagated, when the connected socket is closed.
     * The connection is identified by:
     * _from -> _to.
     * if _ignore_in_from is true, the _from socket will ignore closings of _to,
     * if _ignore_in_to is true, the _to socket will ignore closings of _from.
     *
     * Closing can later be triggered with disconnect()
     **/
    [[nodiscard]] bool set_ignore_inner_close(std::string const& _from, bool _ignore_in_from, std::string const& _to, bool _ignore_in_to);

    /**
     * searches for the _from -> _to connected sockets and demands from _from to block execution
     * for _from_block_time when close is invoked, equivalent for _to with _to_block_time.
     * @see fake_tcp_socket_handle::block_on_close_for() for further details.
     *
     * @return true, if all non nullopts could be forwarded
     **/
    [[nodiscard]] bool block_on_close_for(std::string const& _from, std::optional<std::chrono::milliseconds> _from_block_time,
                                          std::string const& _to, std::optional<std::chrono::milliseconds> _to_block_time);

    /**
     * Clears all received commands in the _to socket from the _from -> _to connection
     **/
    void clear_command_record(std::string const& _from, std::string const& _to);

    /**
     * Waits for _id to be received in the _from -> _to connection for _timeout amount of time.
     * @return false, if the _id was not received within time.
     **/
    [[nodiscard]] bool wait_for_command(std::string const& _from, std::string const& _to, protocol::id_e _id,
                                        std::chrono::milliseconds _timeout = std::chrono::seconds(3));

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

private:
    void try_add(boost::asio::io_context* _io, fd_t _fd, char const* _type);

    std::pair<std::weak_ptr<fake_tcp_socket_handle>, std::weak_ptr<fake_tcp_socket_handle>> get_connection(std::string const& _from,
                                                                                                           std::string const& _to);

    std::mutex mtx_;
    std::condition_variable assignment_cv_;
    std::condition_variable connectable_cv_;
    std::condition_variable connection_cv_;
    std::atomic<fd_t> next_fd_{1};
    std::map<fd_t, std::weak_ptr<fake_tcp_socket_handle>> fd_to_handle_;
    std::map<fd_t, std::weak_ptr<fake_tcp_acceptor_handle>> fd_to_acceptor_states_;
    std::map<boost::asio::ip::tcp::endpoint, std::weak_ptr<fake_tcp_acceptor_handle>> ep_to_acceptor_states_;
    std::map<std::string, boost::asio::io_context*> name_to_context_;
    std::map<boost::asio::io_context*, std::string> context_to_name_;
    std::map<boost::asio::io_context*, std::vector<fd_t>> context_to_fd_;
    std::map<std::string, std::pair<std::weak_ptr<fake_tcp_socket_handle>, std::weak_ptr<fake_tcp_socket_handle>>> app_names_to_connection;
    std::map<std::string, size_t> connection_name_to_connection_count_;
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
};
}

#endif
