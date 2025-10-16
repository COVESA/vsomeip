// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_CLIENT_ENDPOINT_IMPL_HPP_
#define VSOMEIP_V3_CLIENT_ENDPOINT_IMPL_HPP_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>
#include <chrono>
#include <thread>

#include <boost/array.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/utility.hpp>
#include <vsomeip/constants.hpp>

#include "buffer.hpp"
#include "endpoint_impl.hpp"
#include "client_endpoint.hpp"
#include "tp.hpp"

namespace boost::asio::ip {
class tcp;
}

namespace vsomeip_v3 {

class endpoint;
class endpoint_host;
class tcp_socket;

template<typename Protocol>
class client_endpoint_impl : public endpoint_impl<Protocol>,
                             public client_endpoint,
                             public std::enable_shared_from_this<client_endpoint_impl<Protocol>> {
public:
    typedef typename Protocol::endpoint endpoint_type;
    using socket_type = std::conditional_t<std::is_same_v<Protocol, boost::asio::ip::tcp>, tcp_socket, typename Protocol::socket>;

    client_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host, const std::shared_ptr<routing_host>& _routing_host,
                         const endpoint_type& _local, const endpoint_type& _remote, boost::asio::io_context& _io,
                         const std::shared_ptr<configuration>& _configuration);
    virtual ~client_endpoint_impl();

    bool send(const uint8_t* _data, uint32_t _size);
    bool send(const std::vector<byte_t>& _cmd_header, const byte_t* _data, uint32_t _size);
    bool send_to(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size);
    bool send_error(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size);
    bool flush();

    void prepare_stop(const endpoint::prepare_stop_handler_t& _handler, service_t _service);
    virtual void stop();
    virtual void restart(bool _force = false) = 0;

    bool is_client() const;

    bool is_established() const;
    bool is_established_or_connected() const;
    void set_established(bool _established);
    void set_connected(bool _connected);
    virtual bool get_remote_address(boost::asio::ip::address& _address) const;
    virtual std::uint16_t get_remote_port() const;

    std::uint16_t get_local_port() const;
    virtual bool is_reliable() const = 0;

    size_t get_queue_size() const;

public:
    void cancel_and_connect_cbk(boost::system::error_code const& _error);
    void connect_cbk(boost::system::error_code const& _error);
    void wait_connect_cbk(boost::system::error_code const& _error);
    void wait_connecting_cbk(boost::system::error_code const& _error);
    virtual void send_cbk(boost::system::error_code const& _error, std::size_t _bytes, const message_buffer_ptr_t& _sent_msg);
    void flush_cbk(boost::system::error_code const& _error);
    bool wait_connecting_timer();

public:
    virtual void connect() = 0;
    virtual void receive() = 0;
    virtual void print_status() = 0;

protected:
    enum class cei_state_e : std::uint8_t { CLOSED, CONNECTING, CONNECTED, ESTABLISHED };

    enum class connecting_timer_state_e : std::uint8_t { IN_PROGRESS, FINISH_SUCCESS, FINISH_ERROR };

    std::pair<message_buffer_ptr_t, uint32_t> get_front();
    virtual void send_queued(std::pair<message_buffer_ptr_t, uint32_t>& _entry) = 0;
    virtual void get_configured_times_from_endpoint(service_t _service, method_t _method, std::chrono::nanoseconds* _debouncing,
                                                    std::chrono::nanoseconds* _maximum_retention) const = 0;
    void shutdown_and_close_socket(bool _recreate_socket, bool _is_error = false);
    void shutdown_and_close_socket_unlocked(bool _recreate_socket);
    void start_connect_timer();
    void start_connecting_timer();
    bool check_message_size(uint32_t _size) const;
    typename endpoint_impl<Protocol>::cms_ret_e segment_message(const std::uint8_t* const _data, std::uint32_t _size);
    bool check_queue_limit(const uint8_t* _data, std::uint32_t _size) const;
    void queue_train(const std::shared_ptr<train>& _train);
    void update_last_departure();
    bool ensure_connected(const boost::system::error_code& _error);
    const char* to_string(cei_state_e state);

protected:
    mutable std::mutex socket_mutex_;
    std::unique_ptr<socket_type> socket_;
    const endpoint_type remote_;

    boost::asio::steady_timer flush_timer_;

    std::mutex connect_timer_mutex_;
    boost::asio::steady_timer connect_timer_;
    std::atomic<uint32_t> connect_timeout_;
    std::atomic<cei_state_e> state_;
    std::atomic<std::uint32_t> reconnect_counter_;

    std::mutex connecting_timer_mutex_;
    boost::asio::steady_timer connecting_timer_;
    std::condition_variable connecting_timer_condition_;
    std::atomic<connecting_timer_state_e> connecting_timer_state_;
    std::atomic<uint32_t> connecting_timeout_;

    // send data
    std::shared_ptr<train> train_;
    std::map<std::chrono::steady_clock::time_point, std::deque<std::shared_ptr<train>>> dispatched_trains_;
    boost::asio::steady_timer dispatch_timer_;
    std::chrono::steady_clock::time_point last_departure_;
    std::atomic<bool> has_last_departure_;

    std::deque<std::pair<message_buffer_ptr_t, uint32_t>> queue_;
    std::size_t queue_size_;

    mutable std::recursive_mutex mutex_;

    std::atomic<bool> was_not_connected_;

    std::atomic<bool> is_sending_;

    boost::asio::io_context::strand strand_;

private:
    virtual std::string get_remote_information() const = 0;
    virtual bool tp_segmentation_enabled(service_t _service, instance_t _instance, method_t _method) const;
    virtual std::uint32_t get_max_allowed_reconnects() const = 0;
    virtual void max_allowed_reconnects_reached() = 0;
    void send_segments(const tp::tp_split_messages_t& _segments, std::uint32_t _separation_time);

    void schedule_train();

    void start_dispatch_timer(const std::chrono::steady_clock::time_point& _now);
    void cancel_dispatch_timer();
    void recreate_socket();
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_CLIENT_ENDPOINT_IMPL_HPP_
