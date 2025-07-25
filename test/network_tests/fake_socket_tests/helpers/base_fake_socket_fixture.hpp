// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "attribute_recorder.hpp"
#include "service_state.hpp"
#include "fake_socket_factory.hpp"
#include "app.hpp"

#include "socket_manager.hpp"

#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <stdlib.h>

namespace vsomeip_v3::testing {

/**
 * Base test fixture to allow the writing of communication based tests.
 * This class ensures that the test setup follows the basic do "x" wait for "y"
 * steps in order for the socket_manager to properly identify applications
 * and inject error into them.
 *
 * A basic test setup would be:
 * 1. instantiate this base class
 * 2. call base_fake_socket_fixture::use_configuration
 * 3. instantiate and start as many vsomeip applications as required by calling
 * create_app() followed by start_client() to gain a handle on a testing::app.
 *
 * In addition this base class offers an API to await for low-level socket events
 * between different application.
 **/
struct base_fake_socket_fixture : ::testing::Test {
    /**
     * Ensures a fake_socket_factory is injected into any yet to be started
     * vsomeip::application.
     */
    static void SetUpTestSuite();

    /**
     * Ensures that the injected fake_socket_factory has a "fresh" socket_manager
     * for every test.
     */
    base_fake_socket_fixture();
    /**
     * stops every created testing::app and set the socket_manager to a nullptr
     * into the global fake_socket_factory.
     */
    ~base_fake_socket_fixture();

    /**
     * this function will set the environment variable that will be used
     * from vsomeip apps to deduce the path to the configuration file.
     * Because this function is not thread save neither libdlt, nor any app
     * should be created, before this function is called.
     */
    void use_configuration(std::string const& file_name);

    /**
     * Creates a testing::app. But does not grant access to it yet.
     */
    void create_app(std::string const& _name);

    /**
     * 0. Checks if an app with this name had been created before. Returns nullptr if not.
     * 1. Registers the name as an identifier with the @socket_manager,
     * 2. start the testing::app
     * 3. waits for the socket_manager to be requested to create the first sockets for this
     * identifier.
     *
     * Note: The last step ensures that two app identifiers can be used to uniquely identify socket
     * connection.
     */
    app* start_client(std::string const& _name);

    /**
     * Waits until _timeout expires or the application identified by _name is awaiting
     * connections.application identified by _name is awaiting connections. This helper allows to
     * start the routing application up front, and only start the subsequent applications once the
     * router can accept the connections.
     *
     * @ret true, if the application is awaiting a connection within the passed in timeout,
     *      false, else.
     */
    [[nodiscard]] bool
    await_connectable(std::string const& _name,
                      std::chrono::milliseconds _timeout = std::chrono::seconds(3));

    /**
     * Waits until _timeout expires or the application identified by _from and _to established a
     * connection. The direction matters. It is awaited that _from connects on an accepting _to.
     *
     * @ret true, if the application is awaiting a connection within the passed in timeout,
     *      false, else.
     */
    [[nodiscard]] bool
    await_connection(std::string const& _from, std::string const& _to,
                     std::chrono::milliseconds _timeout = std::chrono::seconds(1));

    /**
     * Searches for a connection in which _from_name connected to an accepting _to_name.
     * If the connection is found:
     * 1. it will try to inject the passed in _from_error into the async_receive handler of the
     *_from socket.
     * 2. it will try to inject the passed in _to_error into the async_receive handler form the _to
     *socket.
     * 3. it will remove the capability of sending data between the socket. If the socket would be
     *requested to async_send something the passed in handler would be invoked with a broken pipe
     *error.
     *
     * @ret true, if the passed in errors were successfully injected (note two nullopts are always
     *successfully injected) false, else
     **/
    [[nodiscard]] bool disconnect(std::string const& _from_name,
                                  std::optional<boost::system::error_code> _from_error,
                                  std::string const& _to_name,
                                  std::optional<boost::system::error_code> _to_error);

    /**
     * @see socket_manager::connection_count()
     **/
    size_t connection_count(std::string const& _from, std::string const& _to);

    /**
     * @see socket_manager::report_on_connect()
     **/
    void report_on_connect(std::string const& _app_name,
                           std::vector<boost::system::error_code> _next_errors);

    /**
     * @see socket_manager::ignore_connections()
     **/
    void ignore_connections(std::string const& _app_name, size_t _number_of_ignored_connections);

    /**
     * @see socket_manager::set_ignore_connections()
     **/
    void set_ignore_connections(std::string const& _app_name, bool _ignore_connections);

    /**
     * @see socket_manager::delay_message_processing()
     **/
    [[nodiscard]] bool delay_message_processing(std::string const& _from, std::string const& _to,
                                                bool _delay);

    /**
     * @see socket_manager::block_on_close_for()
     **/
    [[nodiscard]] bool block_on_close_for(std::string const& _from,
                                          std::optional<std::chrono::milliseconds> _from_block_time,
                                          std::string const& _to,
                                          std::optional<std::chrono::milliseconds> _to_block_time);

private:
    static std::shared_ptr<fake_socket_factory> factory_;
    std::shared_ptr<socket_manager> socket_manager_ {std::make_shared<socket_manager>()};
    std::map<std::string, std::unique_ptr<app>> name_to_client_;
};
}
