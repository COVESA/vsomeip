// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <atomic>

#include <boost/asio/steady_timer.hpp>

#include "buffer.hpp"
#include "boardnet_endpoint.hpp"
#include "../../configuration/include/configuration.hpp"

namespace vsomeip_v3 {

class boardnet_endpoint_host;
class boardnet_routing_host;

template<typename Protocol>
class endpoint_impl : public boardnet_endpoint {
public:
    typedef typename Protocol::endpoint endpoint_type;

    endpoint_impl(const std::shared_ptr<boardnet_endpoint_host>& _boardnet_endpoint_host,
                  const std::shared_ptr<boardnet_routing_host>& _routing_host, boost::asio::io_context& _io,
                  const std::shared_ptr<configuration>& _configuration);
    endpoint_impl(endpoint_impl<Protocol> const&) = delete;
    endpoint_impl(endpoint_impl<Protocol> const&&) = delete;
    virtual ~endpoint_impl() = default;

    void add_default_target(service_t, const std::string&, uint16_t) override;
    void remove_default_target(service_t) override;

    virtual std::uint16_t get_local_port() const = 0;
    virtual bool is_reliable() const = 0;

    virtual void print_status() = 0;

    virtual size_t get_queue_size() const = 0;

public:
    // required
    virtual bool is_client() const = 0;
    virtual void receive() = 0;
    virtual void restart(bool _force) = 0;

protected:
    uint32_t find_magic_cookie(byte_t* _buffer, size_t _size);
    instance_t get_instance(service_t _service);

protected:
    enum class cms_ret_e : uint8_t { MSG_TOO_BIG, MSG_OK, MSG_WAS_SPLIT };

    // Reference to service context
    boost::asio::io_context& io_;

    // References to hosts
    std::weak_ptr<boardnet_endpoint_host> endpoint_host_;
    std::weak_ptr<boardnet_routing_host> routing_host_;

    std::uint32_t max_message_size_;

    std::atomic<bool> sending_blocked_;

    endpoint_type local_;

    configuration::endpoint_queue_limit_t queue_limit_;

    std::shared_ptr<configuration> configuration_;

    bool is_supporting_someip_tp_;
};

} // namespace vsomeip_v3
