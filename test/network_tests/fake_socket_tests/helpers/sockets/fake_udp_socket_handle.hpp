// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../../implementation/endpoints/include/udp_socket.hpp"
#include "fake_socket_handle.hpp"
#include "../someip_message.hpp"
#include "../attribute_recorder.hpp"

#include <boost/asio.hpp>

#include <optional>
#include <memory>
#include <deque>

namespace vsomeip_v3::testing {

/**
 * unique identifier type for any socket.
 **/
using fd_t = unsigned short;

class socket_manager;
using connect_handler = std::function<void(boost::system::error_code const&)>;
using completion_handler = std::function<void(boost::system::error_code const&)>;

struct fake_udp_socket_handle : public fake_socket_handle {
    using rw_handler = std::function<void(boost::system::error_code const&, size_t)>;

    explicit fake_udp_socket_handle(boost::asio::io_context& _io);
    fake_udp_socket_handle(fake_udp_socket_handle const&) = delete;
    fake_udp_socket_handle& operator=(fake_udp_socket_handle const&) = delete;
    ~fake_udp_socket_handle();

    void init(fd_t _fd, socket_type _type, std::weak_ptr<socket_manager> _socket_manager) override;

    /**
     * @brief Fake udp socket shutdown sequence.
     * 1. Clear input queue;
     * 2. If an async handler is currently owned, the stored handler is invoked with the operation_aborted error;
     * 3. Resets the protocol_type (so is_open() will return false);
     * 4. Resets the connected endpoint (remote binded address) and local endpoint
     **/
    void cancel() override;

    void clear_handler() override;

    void set_app_name([[maybe_unused]] std::string const& _name) override;

    std::string get_app_name() const override;

    /**
     * True, if the protocol type has been set.
     **/
    [[nodiscard]] bool is_open();

    /**
     * @brief Reads the local_endpoint.
     **/
    boost::asio::ip::udp::endpoint local_endpoint();

    /**
     * @brief Sets the protocol type
     **/
    void open(boost::asio::ip::udp::endpoint::protocol_type _type);

    /**
     * @brief Starts the close up sequence identified in cancel().
     **/
    void close();

    /**
     * @brief Sets the local_endpoint
     **/
    [[nodiscard]] bool bind(boost::asio::ip::udp::endpoint const& _ep);

    void set_option(boost::asio::ip::multicast::join_group _join, boost::system::error_code& _ec);

    void set_option(boost::asio::ip::multicast::leave_group _leave, boost::system::error_code& _ec);

    /**
     * @brief Binds a remote endpoint, used for client udp endpoints.
     */
    void async_connect(boost::asio::ip::udp::endpoint const&, connect_handler);

    /**
     * @brief Send payload to binded remote endpoint.
     */
    void async_send(boost::asio::const_buffer const&, rw_handler);

    /**
     * @brief Send payload to @param _endpoint.
     */
    void async_send_to(boost::asio::const_buffer const& _buffer, boost::asio::ip::udp::endpoint _endpoint, rw_handler _handler);

    /**
     * @brief Called registers handler and mechanism keeps reference for buffer and endpoint.
     * When data is received and ready to be delivered to the application, the handle copies the payload to the buffer and sets the
     * endpoint reference and invokes the passed handler.
     */
    void async_receive_from(boost::asio::mutable_buffer _buffer, boost::asio::ip::udp::endpoint& _endpoint, rw_handler _handler);

    void consume(boost::asio::const_buffer const& _buffer, boost::asio::ip::udp::endpoint _src, boost::asio::ip::udp::endpoint _dst);

    /**
     * if _delay == true, will not send messages, but stores them in the internal
     * buffer only. else, will process and send any delayed messages.
     **/
    bool delay_message_processing(bool _delay);

    /**
     * Processes and sends all delayed messages.
     **/
    void process_delayed_messages();

    /*
     * Stash an error code to be processed during the receive operation on this socket.
     */
    void stash_ec(boost::system::error_code _ec);

    attribute_recorder<someip_sd_record_message> received_sd_record_;

private:
    void update_reception_unlocked();

    struct control_data {
        std::vector<unsigned char> buffer_;
        boost::asio::ip::udp::endpoint src_;
        boost::asio::ip::udp::endpoint dst_;
    };

    struct unicast_receptor {
        unicast_receptor(rw_handler _handler, boost::asio::mutable_buffer _buffer, boost::asio::ip::udp::endpoint& _endpoint) :
            rw_handler_{std::move(_handler)}, buffer_{_buffer}, endpoint_{_endpoint} { }
        rw_handler rw_handler_;
        boost::asio::mutable_buffer buffer_;
        boost::asio::ip::udp::endpoint& endpoint_;
    };

    boost::asio::io_context& io_;
    std::weak_ptr<socket_manager> socket_manager_;
    std::optional<boost::asio::ip::udp::endpoint::protocol_type> protocol_type_;
    std::optional<unicast_receptor> receptor_;
    std::deque<control_data> input_;
    std::optional<boost::asio::ip::udp::endpoint> local_ep_;
    std::optional<boost::asio::ip::udp::endpoint> connected_ep_;
    mutable std::mutex mtx_;
    socket_id socket_id_;
    std::atomic<bool> delay_messages_{false};
    std::vector<control_data> delayed_messages_;

    /*
     *  Error code to be delivered during the next receive operation.
     */
    std::optional<boost::system::error_code> stashed_ec_;
};

}
