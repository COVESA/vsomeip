// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_TCP_SOCKET_HPP_
#define VSOMEIP_V3_TCP_SOCKET_HPP_

#include <boost/asio/ip/tcp.hpp>

#include <functional>

namespace vsomeip_v3 {

/**
 * base class wraper to abstract the boost::asio::ip::tcp::socket and the
 * boost::asio::ip::tcp::acceptor.
 * For a documentation of any method the boost::asio documentation should
 * be consulted.
 **/
class tcp_base_socket {
public:
    using connect_handler = std::function<void(boost::system::error_code const&)>;

    virtual ~tcp_base_socket() = default;

    [[nodiscard]] virtual bool is_open() const = 0;
    [[nodiscard]] virtual int native_handle() = 0;

    virtual void open(boost::asio::ip::tcp::endpoint::protocol_type,
                      boost::system::error_code&) = 0;
    virtual void bind(boost::asio::ip::tcp::endpoint const&, boost::system::error_code&) = 0;

    virtual void close(boost::system::error_code&) = 0;

    static constexpr auto shutdown_both = boost::asio::ip::tcp::socket::shutdown_both;
};

/**
 * specific abstraction for boost::asio::ip::tcp::socket to allow testing without
 * testing boost asio or the OS.
 * For any of the following types or methods the boost::asio documentation should
 * be consulted.
 **/
class tcp_socket : public tcp_base_socket {
public:
    using rw_handler = std::function<void(boost::system::error_code const&, size_t)>;
    using completion_condition = std::function<size_t(boost::system::error_code const&, size_t)>;
    using executor_type = boost::asio::ip::tcp::socket::executor_type;

    virtual ~tcp_socket() = default;

    virtual void cancel(boost::system::error_code&) = 0;
    virtual void shutdown(boost::asio::ip::tcp::socket::shutdown_type,
                          boost::system::error_code&) = 0;

    virtual boost::asio::ip::tcp::endpoint local_endpoint(boost::system::error_code&) const = 0;
    virtual boost::asio::ip::tcp::endpoint remote_endpoint(boost::system::error_code&) const = 0;

    virtual void set_option(boost::asio::ip::tcp::no_delay, boost::system::error_code&) = 0;
    virtual void set_option(boost::asio::ip::tcp::socket::keep_alive,
                            boost::system::error_code&) = 0;
    virtual void set_option(boost::asio::ip::tcp::socket::linger, boost::system::error_code&) = 0;
    virtual void set_option(boost::asio::ip::tcp::socket::reuse_address,
                            boost::system::error_code&) = 0;

    virtual void async_connect(boost::asio::ip::tcp::endpoint const&, connect_handler) = 0;
    virtual void async_receive(boost::asio::mutable_buffer, rw_handler) = 0;
    // usually the ConstBufferSequence is a template parameter
    virtual void async_write(std::vector<boost::asio::const_buffer> const&, rw_handler) = 0;
    virtual void async_write(boost::asio::const_buffer const& b, completion_condition cc,
                             rw_handler handler) = 0;
#if defined(__linux__) || defined(ANDROID)
    /**
     * abstraction for setting the linux specific tcp option
     * TCP_USER_TIMEOUT.
     * On error errno will be set.
     **/
    [[nodiscard]] virtual bool set_user_timeout(unsigned int) = 0;
#endif
#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
    /**
     * abstraction for setting the linux specific option
     * SO_BINDTODEVICE.
     * On error errno will be set.
     **/
    [[nodiscard]] virtual bool bind_to_device(std::string const&) = 0;
    /**
     * abstraction for setting the linux specific function
     * fcntl working on the native socket file descriptor,
     * to check for F_GETFD.
     * On error errno will be set.
     **/
    [[nodiscard]] virtual bool can_read_fd_flags() = 0;
#endif
};

/**
 * specific abstraction for boost::asio::ip::tcp::acceptor to allow testing without
 * testing boost asio or the OS.
 * For any of the following types or methods the boost::asio documentation should
 * be consulted.
 **/
class tcp_acceptor : public tcp_base_socket {
public:
    virtual void set_option(boost::asio::ip::tcp::socket::reuse_address,
                            boost::system::error_code&) = 0;

#if defined(__linux__) || defined(ANDROID)
    /**
     * abstraction for setting the linux specific option
     * IP_FREEBIND.
     * On error errno will be set.
     **/
    [[nodiscard]] virtual bool set_native_option_free_bind() = 0;
#endif

    virtual void listen(int, boost::system::error_code&) = 0;

    virtual void async_accept(tcp_socket&, connect_handler) = 0;

    static constexpr auto max_connection = boost::asio::ip::tcp::acceptor::max_listen_connections;
};

}

#endif
