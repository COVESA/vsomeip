// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ENDPOINT_IMPL_HPP
#define VSOMEIP_ENDPOINT_IMPL_HPP

#include <map>
#include <memory>
#include <mutex>
#include <atomic>

#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>

#include "buffer.hpp"
#include "endpoint.hpp"
#include "../../configuration/include/configuration.hpp"

namespace vsomeip {

class endpoint_host;

template<typename Protocol>
class endpoint_impl: public virtual endpoint {
public:
    typedef typename Protocol::endpoint endpoint_type;

    endpoint_impl(std::shared_ptr<endpoint_host> _adapter,
                  endpoint_type _local,
                  boost::asio::io_service &_io,
                  std::uint32_t _max_message_size,
                  configuration::endpoint_queue_limit_t _queue_limit);
    virtual ~endpoint_impl();

    void enable_magic_cookies();

    // Dummy implementations as we only need these for UDP (servers)
    // TODO: redesign
    void join(const std::string &);
    void leave(const std::string &);

    void add_default_target(service_t, const std::string &, uint16_t);
    void remove_default_target(service_t);

    // Dummy implementations as we only need these for server endpoints
    // TODO: redesign
    std::uint16_t get_local_port() const;
    bool is_reliable() const;

    void increment_use_count();
    void decrement_use_count();
    uint32_t get_use_count();

    void register_error_handler(error_handler_t _error_handler);
    virtual void print_status() = 0;

public:
    // required
    virtual bool is_client() const = 0;
    virtual void receive() = 0;
    virtual void restart(bool _force) = 0;

protected:
    uint32_t find_magic_cookie(byte_t *_buffer, size_t _size);

protected:
    // Reference to service context
    boost::asio::io_service &service_;

    // Reference to host
    std::weak_ptr<endpoint_host> host_;

    bool is_supporting_magic_cookies_;
    std::atomic<bool> has_enabled_magic_cookies_;

    // Filter configuration
    std::map<service_t, uint8_t> opened_;

    std::uint32_t max_message_size_;

    uint32_t use_count_;

    std::atomic<bool> sending_blocked_;

    std::mutex local_mutex_;
    endpoint_type local_;

    error_handler_t error_handler_;
    std::mutex error_handler_mutex_;

    const configuration::endpoint_queue_limit_t queue_limit_;
};

} // namespace vsomeip

#endif // VSOMEIP_ENDPOINT_IMPL_HPP
