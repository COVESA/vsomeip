// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_SERVER_ENDPOINT_IMPL_HPP_
#define VSOMEIP_V3_SERVER_ENDPOINT_IMPL_HPP_

#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include <boost/array.hpp>
#include <boost/asio/io_service.hpp>

#include "buffer.hpp"
#include "endpoint_impl.hpp"
#include "tp.hpp"

namespace vsomeip_v3 {

template<typename Protocol>
class server_endpoint_impl: public endpoint_impl<Protocol>,
        public std::enable_shared_from_this<server_endpoint_impl<Protocol> > {
public:
    typedef typename Protocol::socket socket_type;
    typedef typename Protocol::endpoint endpoint_type;
    typedef typename std::map<endpoint_type, std::pair<size_t, std::deque<message_buffer_ptr_t>>> queue_type;
    typedef typename queue_type::iterator queue_iterator_type;

    server_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host,
                         const std::shared_ptr<routing_host>& _routing_host,
                         endpoint_type _local, boost::asio::io_service &_io,
                         std::uint32_t _max_message_size,
                         configuration::endpoint_queue_limit_t _queue_limit,
                         const std::shared_ptr<configuration>& _configuration);
    virtual ~server_endpoint_impl();

    bool is_client() const;
    void restart(bool _force);
    bool is_established() const;
    bool is_established_or_connected() const;
    void set_established(bool _established);
    void set_connected(bool _connected);
    bool send(const uint8_t *_data, uint32_t _size);
    bool send(const std::vector<byte_t>& _cmd_header, const byte_t *_data,
              uint32_t _size);

    void prepare_stop(endpoint::prepare_stop_handler_t _handler,
                      service_t _service);
    virtual void stop();
    bool flush(endpoint_type _target,
               const std::shared_ptr<train>& _train);

    size_t get_queue_size() const;

    virtual bool is_reliable() const = 0;
    virtual std::uint16_t get_local_port() const = 0;

public:
    void connect_cbk(boost::system::error_code const &_error);
    void send_cbk(const queue_iterator_type _queue_iterator,
                  boost::system::error_code const &_error, std::size_t _bytes);
    void flush_cbk(endpoint_type _target,
                   const std::shared_ptr<train>& _train,
                   const boost::system::error_code &_error_code);

protected:
    virtual bool send_intern(endpoint_type _target, const byte_t *_data,
                             uint32_t _port);
    virtual void send_queued(const queue_iterator_type _queue_iterator) = 0;
    virtual void get_configured_times_from_endpoint(
            service_t _service, method_t _method,
            std::chrono::nanoseconds *_debouncing,
            std::chrono::nanoseconds *_maximum_retention) const = 0;

    virtual bool get_default_target(service_t _service,
            endpoint_type &_target) const = 0;

    virtual void print_status() = 0;

    typename endpoint_impl<Protocol>::cms_ret_e check_message_size(
            const std::uint8_t * const _data, std::uint32_t _size,
            const endpoint_type& _target);
    bool check_queue_limit(const uint8_t *_data, std::uint32_t _size,
                           std::size_t _current_queue_size) const;
    void queue_train(const queue_iterator_type _queue_iterator,
                     const std::shared_ptr<train>& _train,
                     bool _queue_size_zero_on_entry);
    queue_iterator_type find_or_create_queue_unlocked(const endpoint_type& _target);
    std::shared_ptr<train> find_or_create_train_unlocked(const endpoint_type& _target);

    void send_segments(const tp::tp_split_messages_t &_segments, const endpoint_type &_target);

protected:
    queue_type queues_;

    std::mutex clients_mutex_;
    std::map<client_t, std::map<session_t, endpoint_type> > clients_;

    std::map<endpoint_type, std::shared_ptr<train>> trains_;

    std::map<service_t, endpoint::prepare_stop_handler_t> prepare_stop_handlers_;

    mutable std::mutex mutex_;

    std::mutex sent_mutex_;
    bool is_sending_;
    boost::asio::steady_timer sent_timer_;

private:
    virtual std::string get_remote_information(
            const queue_iterator_type _queue_iterator) const = 0;
    virtual std::string get_remote_information(
            const endpoint_type& _remote) const = 0;
    virtual bool tp_segmentation_enabled(service_t _service,
                                         method_t _method) const = 0;
    void wait_until_debounce_time_reached(const std::shared_ptr<train>& _train) const;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_SERVER_ENDPOINT_IMPL_HPP_
