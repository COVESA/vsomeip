// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_TESTING_SHARED_TCP_SOCKET_STATE_HPP_
#define VSOMEIP_V3_TESTING_SHARED_TCP_SOCKET_STATE_HPP_

#include "../../../implementation/endpoints/include/tcp_socket.hpp"
#include "attribute_recorder.hpp"
#include "command_message.hpp"
#include <boost/asio.hpp>
#include <optional>
#include <memory>

namespace vsomeip_v3::testing {

/**
 * unique identifier type for any socket/acceptor.
 **/
using fd_t = unsigned short;

enum class socket_role { unspecified, sender, receiver };
struct socket_id {
    fd_t fd_{};
    socket_role role_{socket_role::unspecified};
    std::string app_name_{};
};

class socket_manager;
using connect_handler = std::function<void(boost::system::error_code const&)>;

/**
 * This class is not expected to be used in isolation but expected to be instantiated
 * by the fake_socket_factory using the socket_manager.
 * Neither it is expected that any test would have the need to directly access
 * any socket. Instead the socket_managers API should be used.
 **/
struct fake_tcp_socket_handle : std::enable_shared_from_this<fake_tcp_socket_handle> {
    using rw_handler = std::function<void(boost::system::error_code const&, size_t)>;

    explicit fake_tcp_socket_handle(boost::asio::io_context& _io);
    fake_tcp_socket_handle(fake_tcp_socket_handle const&) = delete;
    fake_tcp_socket_handle& operator=(fake_tcp_socket_handle const&) = delete;
    ~fake_tcp_socket_handle();

    void init(fd_t fd, std::weak_ptr<socket_manager> _socket_manager);

    /**
     * calls close, used by the fake_tcp_socket.
     **/
    void cancel();

    /**
     * true, if the protocol type has been set.
     * Used by the fake_tcp_socket.
     **/
    [[nodiscard]] bool is_open();

    /**
     * reads the local_endpoint.
     * Used by the fake_tcp_socket.
     **/
    boost::asio::ip::tcp::endpoint local_endpoint();

    /**
     * reads the remote_endpoint.
     * Used by the fake_tcp_socket.
     **/
    boost::asio::ip::tcp::endpoint remote_endpoint();

    /**
     * sets the protocol type
     * Used by the fake_tcp_socket.
     **/
    void open(boost::asio::ip::tcp::endpoint::protocol_type _type);

    /**
     * 1. removes the weak reference to the connected socket (any subsequent call
     * to write will return an error into the handler).
     * 2. resets the protocol_type (so is_open() will return false)
     * 3. if the socket had called async_receive before the stored handler is invoked
     * with the operation_aborted error.
     * 4. tries to lock the weak reference of the connected socket and calls inner_close
     * on the formerly connected socket.
     * Used by the fake_tcp_socket.
     **/
    void close();

    /**
     * Delets the handler stored by a prior async_receive call.
     * Used by the fake_tcp_socket.
     **/
    void shutdown();

    /**
     * sets the local_endpoint
     * Used by the fake_tcp_socket.
     **/
    [[nodiscard]] bool bind(boost::asio::ip::tcp::endpoint const& _ep);

    /**
     * Tries to connect a remote endpoint. Fowards the request to the
     * socket_manager::connect().
     * Used by the fake_tcp_socket.
     **/
    void connect(boost::asio::ip::tcp::endpoint const& _ep, connect_handler _handler);

    /**
     * Tries to write to a connected handle. If no handle is connected injects
     * a broken_pipe error into the passed in handler.
     * Note at the moment all bytes are always consumed, storing them in an internal
     * buffer of the connected handle.
     * If successful injects a success + the number of transmitted bytes into the handler.
     * Used by the fake_tcp_socket.
     **/
    void write(std::vector<boost::asio::const_buffer> const& _buffer, rw_handler _handler);

    /**
     * Stores the passed in buffer and handler.
     * As soon as the internal buffer has some data to be read, it will be forwarded
     * to the handed in buffer until either the internal
     * buffer is empty of the handed in buffer is full. Afterwards the passed in handler
     * is invoked with success and the number of bytes copied.
     * Used by the fake_tcp_socket.
     **/
    void async_receive(boost::asio::mutable_buffer _buffer, rw_handler _handler);

    /**
     * Delets the handler stored by a prior async_receive call.
     * Used by the socket_manager.
     **/
    void clear_handler();

    /**
     * Tries to establish a connection between the passed in handle and the handle itself.
     * 1. Fails if this handle is connected already,
     * 2. adds a weak references to both handles
     * 3. sets the remote endpoint to the local endpoint of the other socket
     * 4. assings roles (sender vs. receiver) to the handlers
     *
     * Used by the socket_manager.
     **/
    [[nodiscard]] bool add_connection(fake_tcp_socket_handle& _socket);

    /**
     * true if the address behind the stored weak reference and the passed in weak reference
     *matches. Used by the socket_manager.
     **/
    [[nodiscard]] bool is_connected(std::weak_ptr<fake_tcp_socket_handle> _to);

    /**
     * Removes the weak reference to the connected socket.
     * If an error is passed in it is tried to be injected
     * into the handler of the last async_receive call.
     * If no receiver is set, the error is stashed until a receiver
     * is passed in.
     *
     * Used by the socket_manager.
     **/
    void disconnect(std::optional<boost::system::error_code> _ec);

    /**
     * if _delay == true, will not invoke the read handler, but stores messages in the internal
     *buffer only. else, will check whether the buffer is filled and dispatch to the stored handler.
     **/
    void delay_processing(bool _delay);

    /**
     * Lets the thread calling ::close sleep for _block_time after informing
     * the connected socket about the closing.
     **/
    void block_on_close_for(std::optional<std::chrono::milliseconds> _block_time);

    void set_app_name(std::string const& _name);
    std::string get_app_name();

    /**
     * If called, then the inner_close call is going to be ignored.
     * This is useful to delay clean-up reactions in one side. Said reaction can then be triggered
     * with disconnect()
     **/
    void ignore_inner_close();

    fd_t fd();

    attribute_recorder<protocol::id_e> received_command_record_;

private:
    void update_reception();
    size_t consume(std::vector<boost::asio::const_buffer> const& _buffer);
    void inner_close();
    struct Receptor {
        boost::asio::mutable_buffer buffer_;
        rw_handler handler_;
    };

    bool ignore_inner_close_{false};
    bool delay_processing_{false};
    socket_id socket_id_;
    boost::asio::io_context& io_;
    std::weak_ptr<socket_manager> socket_manager_;
    std::optional<boost::asio::ip::tcp::endpoint::protocol_type> protocol_type_;
    std::weak_ptr<fake_tcp_socket_handle> connected_socket_;
    std::vector<unsigned char> input_data_;
    std::optional<Receptor> receptor_;
    std::optional<boost::system::error_code> stashed_ec_;
    boost::asio::ip::tcp::endpoint local_ep_;
    boost::asio::ip::tcp::endpoint remote_ep_;
    std::optional<std::chrono::milliseconds> block_on_close_time_;
    std::mutex mtx_;
};

/**
 * This class is not expected to be used in isolation but expected to be instantiated
 * by the fake_socket_factory using the socket_manager.
 * Neither it is expected that any test would have the need to directly access
 * any acceptor directly. Instead the socket_managers API should be used.
 **/
struct fake_tcp_acceptor_handle : std::enable_shared_from_this<fake_tcp_acceptor_handle> {

    explicit fake_tcp_acceptor_handle(boost::asio::io_context& _io);
    fake_tcp_acceptor_handle(fake_tcp_acceptor_handle const&) = delete;
    fake_tcp_acceptor_handle& operator=(fake_tcp_acceptor_handle const&) = delete;
    ~fake_tcp_acceptor_handle();

    void init(fd_t fd, std::weak_ptr<socket_manager> _socket_manager);

    /**
     * Sets the local_endpoint
     * Used by the fake_tcp_acceptor.
     **/
    [[nodiscard]] bool bind(boost::asio::ip::tcp::endpoint const& _ep);

    /**
     * Sets the is_open attribute to true.
     * Used by the fake_tcp_acceptor.
     **/
    void open();

    /**
     * sets the is_open attribute to false.
     * Used by the fake_tcp_acceptor.
     **/
    void close();

    /**
     * Reads the is_open attribute.
     * Used by the fake_tcp_acceptor.
     **/
    [[nodiscard]] bool is_open();

    /**
     * Tries to cast the passed in tcp_socket into a fake_tcp_socket. If not
     * successful injects an error into the passed in handler.
     * Else stores a weak reference to the contained fake_tcp_socket_handle
     * to be used in the next @see fake_tcp_acceptor_handle::connect() call.
     *
     * Used by the fake_tcp_acceptor.
     **/
    void async_accept(tcp_socket& _socket, connect_handler _handler);

    /**
     * Tries to connect the passed in handle to the stored handle received
     * by the last async_accept call.
     * @see fake_tcp_socket_handle::add_connection().
     *
     * Used by the socket_manager
     **/
    [[nodiscard]] std::shared_ptr<fake_tcp_socket_handle> connect(fake_tcp_socket_handle& _state, connect_handler _handler);

    /**
     * true, if async_accept is pending.
     *
     * Note: This does not block as the socket_manager has to potentially await
     * until the acceptor comes into existence.
     *
     * Used by the socket_manager
     **/
    [[nodiscard]] bool is_awaiting_connection();

    void set_app_name(std::string const& _name);
    std::string get_app_name();

private:
    struct connection {
        std::weak_ptr<fake_tcp_socket_handle> socket_;
        connect_handler handler_;
    };
    std::mutex mtx_;
    bool is_open_{false};
    fd_t fd_{0};
    std::optional<connection> connection_;
    boost::asio::io_context& io_;
    std::weak_ptr<socket_manager> socket_manager_;
    std::string app_name_;
    boost::asio::ip::tcp::endpoint endpoint_;
};
}

#endif
