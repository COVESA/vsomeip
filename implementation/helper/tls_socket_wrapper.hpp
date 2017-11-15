// Copyright (c) 2017 by Vector Informatik GmbH
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef IMPLEMENTATION_HELPER_TLS_SOCKET_WRAPPER_HPP_
#define IMPLEMENTATION_HELPER_TLS_SOCKET_WRAPPER_HPP_

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#define DEBUG_LEVEL 0

#include <iomanip>
#include <sstream>

#include <deque>
#include <functional>
#include <type_traits>

#include <boost/asio/async_result.hpp>
#include <boost/asio/detail/config.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp_ext.hpp>
#include <boost/asio/ip/udp.hpp>

#include <mbedtls/ctr_drbg.h>
#if DEBUG_LEVEL > 0
#include <mbedtls/debug.h>
#endif
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_ciphersuites.h>

#include "../logging/include/logger.hpp"


namespace vsomeip {
namespace tls {

template<typename Protocol, bool is_client>
class tls_socket_wrapper {
    using endpoint_t = typename Protocol::endpoint;
    using socket_t = typename Protocol::socket;

    using ec_cbk_t = typename std::function<void(const boost::system::error_code  &)>;
    using io_cbk_t = typename std::function<void(const boost::system::error_code &, std::size_t)>;

    static constexpr bool is_udp = std::is_same<Protocol, typename boost::asio::ip::udp>::value;
    static constexpr bool is_udp_ext = std::is_same<Protocol, typename boost::asio::ip::udp_ext>::value;

public:
    using send_cbk_t = io_cbk_t;
    using receive_cbk_t = io_cbk_t;
    using handshake_cbk_t = ec_cbk_t;
    using receive_cbk_ext_t = typename std::function<void(const boost::system::error_code &, std::size_t,
                                                          const boost::asio::ip::address &)>;

    tls_socket_wrapper() = delete;
    tls_socket_wrapper(const tls_socket_wrapper &) = delete;
    tls_socket_wrapper(socket_t * const _socket,
                       std::mutex &_socket_mutex,
                       const std::vector<std::uint8_t> &_psk,
                       const std::string &_pskid,
                       const std::vector<int> &_cipher_suites)
        : socket_{_socket},
          socket_mutex_{_socket_mutex},
          byte_written_{0},
          pending_message_{nullptr},
          byte_read_{0},
          byte_left_{0},
          plain_buffer_{},
          final_timer_{_socket->get_io_service()},
          intermediate_timer_{_socket->get_io_service()},
          handshake_done_{false} {
        mbedtls_ssl_init(&ssl_context_);
        mbedtls_ssl_config_init(&ssl_config_);
        mbedtls_ctr_drbg_init(&ctr_drbg_);
        mbedtls_entropy_init(&entropy_);

        if (is_udp || is_udp_ext) {
            recv_buffer_.resize(VSOMEIP_MAX_UDP_MESSAGE_SIZE);
        } else {
            throw std::logic_error("TLS over TCP is currently not supported.");
        }

        auto ret = 0;
        ret = mbedtls_ctr_drbg_seed(&ctr_drbg_, mbedtls_entropy_func, &entropy_, 0, 0);
        if (ret != 0) {
            throw std::runtime_error("mbedTLS: Entropy seeding failed.");
        }

        ret = mbedtls_ssl_config_defaults(&ssl_config_,
                                          (is_client) ? MBEDTLS_SSL_IS_CLIENT : MBEDTLS_SSL_IS_SERVER,
                                          (is_udp || is_udp_ext) ? MBEDTLS_SSL_TRANSPORT_DATAGRAM : MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT);
        if (ret != 0) {
            throw std::runtime_error("mbedTLS: Failed configuring mbedTLS defaults.");
        }

        // we don't do certificate verification
        mbedtls_ssl_conf_authmode(&ssl_config_, MBEDTLS_SSL_VERIFY_NONE);
        mbedtls_ssl_conf_rng(&ssl_config_, mbedtls_ctr_drbg_random, &ctr_drbg_);
        mbedtls_ssl_conf_dbg(&ssl_config_, debug, stdout);
        mbedtls_ssl_conf_ciphersuites(&ssl_config_, _cipher_suites.data());

#if DEBUG_LEVEL > 0
        mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif

        ret = mbedtls_ssl_conf_psk(&ssl_config_,
                                   static_cast<const unsigned char *>(_psk.data()), _psk.size(),
                                   reinterpret_cast<const unsigned char *>(_pskid.c_str()), _pskid.size());
        if (ret != 0) {
            throw std::runtime_error("mbedTLS: Failed allocating the PSK.");
        }

        ret = mbedtls_ssl_setup(&ssl_context_, &ssl_config_);
        if (ret != 0) {
            throw std::runtime_error("mbedTLS: Failed setting up the SSL context.");
        }

        mbedtls_ssl_set_bio(&ssl_context_, static_cast<void *>(this), bio_write_start_cb, bio_read_cb, nullptr);
        if (is_udp || is_udp_ext) {
            mbedtls_ssl_set_timer_cb(&ssl_context_, static_cast<void *>(this), set_delay_cb, get_delay_cb);
        }

        if (!is_client) {
            // we do not support cookie verification, so disable HelloVerifyRequest
            mbedtls_ssl_conf_dtls_cookies(&ssl_config_, 0, 0, 0);
        }
    }

    virtual ~tls_socket_wrapper() {
        mbedtls_ssl_free(&ssl_context_);
        mbedtls_ssl_config_free(&ssl_config_);
        mbedtls_ctr_drbg_free(&ctr_drbg_);
        mbedtls_entropy_free(&entropy_);
    }

    /**
     * \brief   Can be used to determine if the handshake was completed successfully.
     * \return  TRUE if handshake was completed, otherwise FALSE
     */
    bool is_handshake_done() {
        return handshake_done_;
    }

    /**
     * \brief Performs the TLS handshake that needs to be done prior to the actual communication.
     *        Stores the callback to be invoked once the handshake is finished.
     */
    void async_handshake(handshake_cbk_t _handler) {
        reset();

        handshake_cbk_ = _handler;
        do_handshake();
    }

    /**
     * \brief Performs encryption and asynchronous sending of the encrypted data through the boost socket.
     */
    void async_send(message_buffer_ptr_t _buffer, send_cbk_t _handler) {
        static_assert(tls_socket_wrapper::is_udp, "Only available for UDP.");
        if (!is_handshake_done()) {
            return;
        }

        send_cbk_ = _handler;
        pending_message_ = _buffer;

        // forward to mbed
        auto ret = 0;
        {
            std::lock_guard<std::mutex> lock(ssl_context_mutex_);
            ret = mbedtls_ssl_write(&ssl_context_, _buffer->data(), _buffer->size());
        }
        print_mbedtls_return(ret, __FUNCTION__, __LINE__);

        // bio_write_start always returns WANT_WRITE, if we get something else here then there is sth wrong
        if (ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            reset();
            if (send_cbk_) {
                send_cbk_(boost::asio::error::operation_aborted, 0);
            }
        } else {
            do_async_send();
        }
    }

    /**
     * \brief Performs encryption and asynchronous sending of the encrypted data through the boost socket
     *        to the given peer.
     */
    void async_send_to(message_buffer_ptr_t _buffer, const endpoint_t &_peer, send_cbk_t _handler) {
        if (!is_handshake_done()) {
            return;
        }

        send_cbk_ = _handler;
        internal_peer_ = _peer;
        pending_message_ = _buffer;

        auto ret = 0;
        {
            std::lock_guard<std::mutex> lock(ssl_context_mutex_);
            ret = mbedtls_ssl_write(&ssl_context_, _buffer->data(), _buffer->size());
        }
        print_mbedtls_return(ret, __FUNCTION__, __LINE__);

        if (ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            reset();
            if (send_cbk_) {
                send_cbk_(boost::asio::error::operation_aborted, 0);
            }
        } else {
            do_async_send();
        }
    }

    /**
     * \brief Receives data through the boost socket and decrypts it.
     */
    template<typename P = Protocol>
    typename std::enable_if<std::is_same<P, typename boost::asio::ip::udp>::value, void>::type
    async_receive_from(message_buffer_ptr_t _buffer, endpoint_t &_peer, receive_cbk_t _handler) {
        if (!is_handshake_done()) {
            return;
        }

        receive_cbk_ = _handler;
        plain_buffer_ = _buffer;
        do_async_receive_from(_peer);
    }

    /**
     * \brief Receives data through the boost socket and decrypts it.
     */
    template<typename P = Protocol>
    typename std::enable_if<std::is_same<P, typename boost::asio::ip::udp_ext>::value, void>::type
    async_receive_from(message_buffer_ptr_t _buffer, endpoint_t &_peer, receive_cbk_ext_t _handler) {
        if (!handshake_done_) {
            return;
        }

        receive_cbk_ext_ = _handler;
        plain_buffer_ = _buffer;
        do_async_receive_from(_peer);
    }

    /**
     * \brief Resets the internal state and mbedTLS SSL context.
     */
    void reset() {
        handshake_done_ = false;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            out_queue_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(ssl_context_mutex_);
            if (mbedtls_ssl_session_reset(&ssl_context_) != 0) {
                throw std::runtime_error("mbedTLS: Failed resetting & reinitializing SSL context.");
            }
        }
    }

private:
    void do_async_send() {
        message_buffer_ptr_t buffer;
        {
            std::lock_guard<std::mutex> queue_lock(queue_mutex_);
            if (out_queue_.empty()) {
                return;
            }

            buffer = out_queue_.front();
        }

        std::lock_guard<std::mutex> socket_lock(socket_mutex_);
        if (is_client) {
            socket_->async_send(boost::asio::buffer(*buffer),
                                std::bind(&tls_socket_wrapper::send_cbk, this,
                                          std::placeholders::_1, std::placeholders::_2));
        } else {
            socket_->async_send_to(boost::asio::buffer(*buffer),
                                   internal_peer_,
                                   std::bind(&tls_socket_wrapper::send_cbk, this,
                                             std::placeholders::_1, std::placeholders::_2));
        }
    }

    template<typename T = Protocol>
    typename std::enable_if<!std::is_same<T, typename boost::asio::ip::udp_ext>::value, void>::type
    do_async_receive_from(endpoint_t &_peer) {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        socket_->async_receive_from(boost::asio::buffer(&recv_buffer_[0], recv_buffer_.size()),
                                    _peer,
                                    std::bind(&tls_socket_wrapper::receive_cbk, this,
                                              std::placeholders::_1, std::placeholders::_2));
    }

    template<typename T = Protocol>
    typename std::enable_if<std::is_same<T, typename boost::asio::ip::udp_ext>::value, void>::type
    do_async_receive_from(endpoint_t &_peer) {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        socket_->async_receive_from(boost::asio::buffer(&recv_buffer_[0], recv_buffer_.size()),
                                    _peer,
                                    std::bind(&tls_socket_wrapper::receive_cbk_ext, this,
                                              std::placeholders::_1, std::placeholders::_2,
                                              std::placeholders::_3));
    }

    void do_handshake() {
        auto ret = 0;
        {
            std::lock_guard<std::mutex> lock(ssl_context_mutex_);
            ret = mbedtls_ssl_handshake(&ssl_context_);
        }
        print_mbedtls_return(ret, __FUNCTION__, __LINE__);

        if (ret == 0) {
            handshake_done_ = true;
            call_handshake_cbk(boost::system::error_code());
        } else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
            if (!byte_left_) {
                do_async_receive_from(internal_peer_);
            } else {
                do_handshake();
            }
        } else {
            // in case of an alert mbed unfortunately will not tell us that it still wants to
            // write but instead returns the error code -> try to send what is still left
            do_async_send();
            if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                return;
            }

            reset();

            if (ret == MBEDTLS_ERR_SSL_TIMEOUT) {
                VSOMEIP_WARNING << __FUNCTION__ << "[" << __LINE__ << "]: Handshake timed out.";
                call_handshake_cbk(boost::asio::error::timed_out);
            } else {
                VSOMEIP_ERROR << __FUNCTION__ << "[" << __LINE__ << "]: Handshake failed.";
                call_handshake_cbk(boost::asio::error::operation_aborted);
            }
        }
    }

    std::size_t do_receive(boost::system::error_code &_error) {
        auto ret = 0;
        {
            std::lock_guard<std::mutex> lock(ssl_context_mutex_);
            do {
                ret = mbedtls_ssl_read(&ssl_context_, plain_buffer_->data(), plain_buffer_->size());
            } while (byte_left_ > 0 && ret == MBEDTLS_ERR_SSL_WANT_READ);
        }
        print_mbedtls_return(ret, __FUNCTION__, __LINE__);

        if (ret > 0) {
            return static_cast<std::size_t>(ret);
        } else if (ret == 0) {
            _error = boost::asio::error::eof;
        } else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
            do_async_receive_from(internal_peer_);
        } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            do_async_send();
        } else {
            reset();
            if (ret == MBEDTLS_ERR_SSL_CLIENT_RECONNECT) {
                _error = boost::asio::error::connection_reset;
            } else {
                _error = boost::asio::error::operation_aborted;
            }
        }
        return 0;
    }

    void send_cbk(const boost::system::error_code &_error, std::size_t _bytes) {
        auto ret = 0;
        if (_error) {
            reset();
        }

        if (is_handshake_done()) {
            auto error = _error;
            if (!error) {
                assert(pending_message_);
                {
                    std::lock_guard<std::mutex> lock(ssl_context_mutex_);
                    do {
                        ret = mbedtls_ssl_write(&ssl_context_,
                                                pending_message_->data(),
                                                pending_message_->size());
                    } while (ret == MBEDTLS_ERR_SSL_WANT_WRITE);
                }
                print_mbedtls_return(ret, __FUNCTION__, __LINE__);

                if (ret > 0) {
                    byte_written_ += static_cast<std::size_t>(ret);
                    if (byte_written_ < pending_message_->size()) {
                        VSOMEIP_WARNING << __FUNCTION__ << "[" << __LINE__ << "]: Partial write! "
                                        << byte_written_ << "/" << pending_message_->size()
                                        << " byte of the current message have been processed.";
                        {
                            std::lock_guard<std::mutex> lock(ssl_context_mutex_);
                            ret = mbedtls_ssl_write(&ssl_context_,
                                                    pending_message_->data() + byte_written_,
                                                    pending_message_->size() - byte_written_);
                        }
                        print_mbedtls_return(ret, __FUNCTION__, __LINE__);

                        if (ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                            throw std::logic_error("Expected mbedTLS to return WANT_WRITE but got " + ret);
                        } else {
                            do_async_send();
                            return;
                        }
                    }

                    // message completed
                    if (byte_written_ == pending_message_->size()) {
                        _bytes = byte_written_;
                        byte_written_ = 0;
                        pending_message_ = nullptr;
                    }
                } else if (ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                    reset();
                    error = boost::asio::error::operation_aborted;
                }
            }

            if (send_cbk_) {
                send_cbk_(error, _bytes);
            }
        } else {
            if (_error) {
                call_handshake_cbk(_error);
            } else {
                do_handshake();
            }
        }
    }

    void receive_cbk(const boost::system::error_code &_error, std::size_t _bytes) {
        byte_read_ = 0;
        byte_left_ = _bytes;
        if (_error) {
            reset();
        }

        if (is_handshake_done()) {
            auto error = _error;
            std::size_t bytes_plain = 0;
            if (!error) {
                bytes_plain = do_receive(error);
            }
            if (receive_cbk_) {
                receive_cbk_(error, bytes_plain);
            }
        } else {
            if (_error) {
                call_handshake_cbk(_error);
            } else {
                do_handshake();
            }
        }
    }

    void receive_cbk_ext(const boost::system::error_code &_error, std::size_t _bytes,
                         const boost::asio::ip::address &_destination) {
        byte_read_ = 0;
        byte_left_ = _bytes;
        if (_error) {
            reset();
        }

        if (is_handshake_done()) {
            auto error = _error;
            std::size_t bytes_plain = 0;
            if (!error) {
                bytes_plain = do_receive(error);
            }
            if (receive_cbk_ext_) {
                receive_cbk_ext_(error, bytes_plain, _destination);
            }
        } else {
            if (_error) {
                call_handshake_cbk(_error);
            } else {
                do_handshake();
            }
        }
    }

    void call_handshake_cbk(const boost::system::error_code &_error) {
        if (handshake_cbk_) {
            handshake_cbk_(_error);
        }
    }

    /**
     * \brief   Called by mbedTLS to set / reset the timers.
     */
    void set_delay(std::uint32_t _intermediate_ms, std::uint32_t _final_ms) {
        timer_cancelled_ = (_final_ms == 0);

        // make sure both timer are cancelled with each call to this function
        final_timer_expired_ = false;
        intermediate_timer_expired_ = false;
        final_timer_.cancel();
        intermediate_timer_.cancel();

        if (_final_ms) {
            final_timer_.expires_from_now(std::chrono::milliseconds(_final_ms));
            final_timer_.async_wait(
                    std::bind(&tls_socket_wrapper::final_timer_cbk, this,
                              std::placeholders::_1));
        }

        if (_intermediate_ms) {
            intermediate_timer_.expires_from_now(std::chrono::milliseconds(_intermediate_ms));
            intermediate_timer_.async_wait(
                    std::bind(&tls_socket_wrapper::intermediate_timer_cbk, this,
                              std::placeholders::_1));
        }
    }

    /**
     * \brief   Used by mbedTLS to determine which timers have expired (if any).
     */
    int get_delay() {
        if (timer_cancelled_) {
            return -1;
        } else if (final_timer_expired_) {
            return 2;
        } else if (intermediate_timer_expired_) {
            return 1;
        }

        return 0;
    }

    /**
     * \brief   Called when data has been received and a read through mbedTLS is necessary.
     *          Queues the message, switches the send callback and tells mbed WANT_WRITE.
     */
    int bio_read(unsigned char *_buffer, size_t _length) {
        if (_length < byte_left_) {
            VSOMEIP_WARNING << __FUNCTION__ << "[" << __LINE__ <<
                    "]: More data receive than the mbedTLS buffer (" << _length << " byte) can fit.";
        } else if (byte_left_ == 0) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }

        std::size_t actual = std::min(_length, byte_left_);
        std::copy_n(recv_buffer_.begin(), actual, _buffer);

        byte_read_ += actual;
        byte_left_ -= actual;

        return static_cast<int>(actual);
    }

    /**
     * \brief   Called when initiating a write through mbedtls_ssl_write() or handshake.
     *          Queues the message, switches the send callback and tells mbed WANT_WRITE.
     */
    int bio_write_start(const unsigned char *_buffer, size_t _length) {
        std::lock_guard<std::mutex> queue_lock(queue_mutex_);

        message_buffer_ptr_t its_buffer = std::make_shared<message_buffer_t>();
        its_buffer->insert(its_buffer->begin(), _buffer, _buffer + _length);
        out_queue_.push_back(its_buffer);

        mbedtls_ssl_set_bio(&ssl_context_, static_cast<void *>(this), bio_write_done_cb, bio_read_cb, nullptr);

        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }

    /**
     * \brief   Called when a write completed. Pops the message, switches the callback
     *          and returns the number of byte written.
     * \note    This approach requires that new messages are only sent once the last completed. VSOMEIP makes
     *          sure that this is the case by checking if writing is currently in progress.
     */
    int bio_write_done(const unsigned char *_buffer, size_t _length) {
        (void)_buffer;
        (void)_length;

        std::lock_guard<std::mutex> queue_lock(queue_mutex_);

        auto ret = MBEDTLS_ERR_SSL_WANT_WRITE;
        ret = static_cast<int>(out_queue_.front()->size());
        out_queue_.pop_front();

        mbedtls_ssl_set_bio(&ssl_context_, static_cast<void *>(this), bio_write_start_cb, bio_read_cb, nullptr);

        return ret;
    }

    /**
     * \brief   Handles expiration of the final timer. mbedTLS requires that on expiration of this timer the handshake
     *          will be triggered again.
     */
    void final_timer_cbk(const boost::system::error_code &_error) {
        if (!_error) {
            final_timer_expired_ = true;
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                out_queue_.clear();
            }
            {
                std::lock_guard<std::mutex> lock(ssl_context_mutex_);
                mbedtls_ssl_set_bio(&ssl_context_, static_cast<void *>(this), bio_write_start_cb, bio_read_cb, nullptr);
            }

            do_handshake();
        }
    }

    /**
     * \brief   Handles expiration of the intermediate timer which has no purpose as of now.
     */
    void intermediate_timer_cbk(boost::system::error_code const &_error) {
        if (!_error) {
            intermediate_timer_expired_ = true;
        }
    }

    static int bio_read_cb(void *_ctx, unsigned char *_buffer, size_t _length) {
        return static_cast<tls_socket_wrapper *>(_ctx)->bio_read(_buffer, _length);
    }

    static int bio_write_start_cb(void *_ctx, const unsigned char *_buffer, size_t _length) {
        return static_cast<tls_socket_wrapper *>(_ctx)->bio_write_start(_buffer, _length);
    }

    static int bio_write_done_cb(void *_ctx, const unsigned char *_buffer, size_t _length) {
        return static_cast<tls_socket_wrapper *>(_ctx)->bio_write_done(_buffer, _length);
    }

    static int get_delay_cb(void *_ctx) {
        return static_cast<tls_socket_wrapper *>(_ctx)->get_delay();
    }

    static void set_delay_cb(void *_ctx, uint32_t _intermediate_ms, std::uint32_t _final_ms) {
        static_cast<tls_socket_wrapper *>(_ctx)->set_delay(_intermediate_ms, _final_ms);
    }

    void print_mbedtls_return(int _ec, const std::string &_function, int _line) {
#if DEBUG_LEVEL > 0
        if (_ec > 0) {
            VSOMEIP_INFO << _function << "[" << _line << "]: mbedTLS returned " << _ec << " byte read / written.";
        } else {
            switch (_ec) {
                case MBEDTLS_ERR_SSL_WANT_READ:
                    VSOMEIP_INFO << _function << "[" << _line << "]: mbedTLS returned WANT_READ.";
                    break;
                case MBEDTLS_ERR_SSL_WANT_WRITE:
                    VSOMEIP_INFO << _function << "[" << _line << "]: mbedTLS returned WANT_WRITE.";
                    break;
                case MBEDTLS_ERR_SSL_TIMEOUT:
                    VSOMEIP_ERROR << _function << "[" << _line << "]: mbedTLS handshake timed out.";
                    break;
                case 0:
                    VSOMEIP_INFO << _function << "[" << _line << "]: mbedTLS operation succeeded.";
                    break;
                default:
                    VSOMEIP_ERROR << _function << "[" << _line << "]: mbedTLS returned error code " << _ec;
                    break;
            }
        }
#else
        (void)_ec;
        (void)_function;
        (void)_line;
#endif
    }

    static void debug(void *_ctx, int _level, const char *_file, int _line, const char *_str) {
        (void)_ctx;
        (void)_level;

        std::string file(_file);
        std::string str(_str);

        VSOMEIP_INFO << __FUNCTION__ << "[" << __LINE__ << "]: [" << file << ":" << _line << "]: " << str;
    }

    socket_t * const socket_;
    std::mutex &socket_mutex_;

    handshake_cbk_t handshake_cbk_;
    send_cbk_t send_cbk_;
    receive_cbk_t receive_cbk_;
    receive_cbk_ext_t receive_cbk_ext_;

    endpoint_t internal_peer_;
    boost::asio::ip::address internal_dest_;

    std::mutex queue_mutex_;
    std::deque<message_buffer_ptr_t> out_queue_;
    std::size_t byte_written_;
    message_buffer_ptr_t pending_message_;

    std::size_t byte_read_;
    std::size_t byte_left_;
    message_buffer_t recv_buffer_;
    message_buffer_ptr_t plain_buffer_;

    boost::asio::steady_timer final_timer_;
    boost::asio::steady_timer intermediate_timer_;
    std::atomic<bool> timer_cancelled_;
    std::atomic<bool> final_timer_expired_;
    std::atomic<bool> intermediate_timer_expired_;
    std::atomic<bool> handshake_done_;
    std::mutex ssl_context_mutex_;
    mbedtls_ssl_context ssl_context_;
    mbedtls_entropy_context entropy_;
    mbedtls_ctr_drbg_context ctr_drbg_;
    mbedtls_ssl_config ssl_config_;
};

}  // namespace tls
}  // namespace vsomeip


#endif  // IMPLEMENTATION_HELPER_TLS_SOCKET_WRAPPER_HPP_
