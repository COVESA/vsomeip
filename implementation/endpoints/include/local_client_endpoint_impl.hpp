// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOCAL_CLIENT_ENDPOINT_IMPL_HPP_
#define VSOMEIP_V3_LOCAL_CLIENT_ENDPOINT_IMPL_HPP_

#include "client_endpoint_impl.hpp"

namespace vsomeip_v3 {

class endpoint_host;

/**
 * @brief Parent class for client endpoints employed for local communication, mainly used to define callbacks for asynchronous R/W
 * operations and error handling.
 *
 * @tparam Protocol network protocol.
 */
template<typename Protocol>
class local_client_endpoint_impl : public client_endpoint_impl<Protocol> {
public:
    typedef typename Protocol::endpoint endpoint_type;

    local_client_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host, const std::shared_ptr<routing_host>& _routing_host,
                               const endpoint_type& _local, const endpoint_type& _remote, boost::asio::io_context& _io,
                               const std::shared_ptr<configuration>& _configuration);
    virtual ~local_client_endpoint_impl() = default;

public:
    /**
     * @brief Callback parameterized for write operations of local communication.
     */
    void send_cbk(boost::system::error_code const& _error, std::size_t _bytes, const message_buffer_ptr_t& _sent_msg) override;

    /**
     * @brief Manages the error handling of both R/W operations and forwards to routing manager the vsomeip protocol layer decisions.
     */
    void error_handler();

    /**
     * @defgroup Virtual functions defined by derived Protocol class.
     * @{
     */
    virtual void restart(bool _force = false) = 0;
    virtual bool is_reliable() const = 0;
    virtual void connect() = 0;
    virtual void receive() = 0;
    virtual void print_status() = 0;

protected:
    virtual void send_queued(std::pair<message_buffer_ptr_t, uint32_t>& _entry) = 0;
    virtual void get_configured_times_from_endpoint(service_t _service, method_t _method, std::chrono::nanoseconds* _debouncing,
                                                    std::chrono::nanoseconds* _maximum_retention) const = 0;

private:
    virtual std::string get_remote_information() const = 0;
    virtual std::uint32_t get_max_allowed_reconnects() const = 0;
    virtual void max_allowed_reconnects_reached() = 0;
    /** @} */
};
}

#endif
