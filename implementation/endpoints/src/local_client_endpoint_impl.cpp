// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <string>

#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <vsomeip/internal/logger.hpp>
#include "../include/local_client_endpoint_impl.hpp"
#include "../../utility/include/bithelper.hpp"

namespace vsomeip_v3 {

template<typename Protocol>
local_client_endpoint_impl<Protocol>::local_client_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host,
                                                                 const std::shared_ptr<routing_host>& _routing_host,
                                                                 const endpoint_type& _local, const endpoint_type& _remote,
                                                                 boost::asio::io_context& _io,
                                                                 const std::shared_ptr<configuration>& _configuration) :
    client_endpoint_impl<Protocol>(_endpoint_host, _routing_host, _local, _remote, _io, _configuration) { }

template<typename Protocol>
void local_client_endpoint_impl<Protocol>::send_cbk(boost::system::error_code const& _error, std::size_t _bytes,
                                                    const message_buffer_ptr_t& _sent_msg) {

    if (!_error) {
        client_endpoint_impl<Protocol>::send_cbk(_error, _bytes, _sent_msg);
        return;
    }

    VSOMEIP_WARNING << "lcei::send_cbk received error: " << _error.message() << " (" << std::dec << _error.value()
                    << "), remote: " << get_remote_information() << ", endpoint > " << this << " socket state > "
                    << client_endpoint_impl<Protocol>::to_string(client_endpoint_impl<Protocol>::state_.load());
    if (_error == boost::asio::error::operation_aborted) {
        // If handling op_aborted it means async r/w op have been canceled and socket already closed.
        return;
    }

    print_status();
    {
        std::lock_guard<std::recursive_mutex> its_lock(client_endpoint_impl<Protocol>::mutex_);
        client_endpoint_impl<Protocol>::was_not_connected_ = true;
        client_endpoint_impl<Protocol>::is_sending_ = false;
        if (endpoint_impl<Protocol>::sending_blocked_) {
            client_endpoint_impl<Protocol>::queue_.clear();
            client_endpoint_impl<Protocol>::queue_size_ = 0;
        }
    }
    error_handler();
}

template<typename Protocol>
void local_client_endpoint_impl<Protocol>::error_handler() {
    endpoint::error_handler_t handler{nullptr};
    // Safe to assume that write/read operations can only be performed when the state is either established or connected.
    // Callbacks for asynchronous operations for the same client endpoint will never run concurrently due to asio strand scheduling.
    // If both write and read operations handle an error, only the first one will be handled by routing.
    if (client_endpoint_impl<Protocol>::is_established_or_connected()) {
        std::lock_guard<std::mutex> its_lock(endpoint_impl<Protocol>::error_handler_mutex_);
        handler = endpoint_impl<Protocol>::error_handler_;
        client_endpoint_impl<Protocol>::shutdown_and_close_socket(false);
    } else {
        VSOMEIP_INFO << "lcei::" << __func__ << " connection no longer established/connected " << this;
    }

    if (handler) {
        handler();
    }
}

// Instantiate template
#if defined(__linux__) || defined(__QNX__)
template class local_client_endpoint_impl<boost::asio::local::stream_protocol>;
#endif
template class local_client_endpoint_impl<boost::asio::ip::tcp>;

} // namespace vsomeip_v3
