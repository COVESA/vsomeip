// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/asio/ip/udp.hpp>
#include <vsomeip/defines.hpp>

#include "server_endpoint_impl.hpp"
#include "tp_reassembler.hpp"
#include "udp_socket.hpp"

namespace vsomeip_v3 {
using udp_server_endpoint_base_impl = server_endpoint_impl<boost::asio::ip::udp>;

// callback type to sent messages (SD)
using on_unicast_sent_cbk_t = std::function<void(const byte_t*, length_t, const boost::asio::ip::address&)>;
// callback type to own multicast messages received
using on_sent_multicast_received_cbk_t = std::function<void(const byte_t*, length_t, const boost::asio::ip::address&)>;

class udp_server_endpoint_impl : public udp_server_endpoint_base_impl {

public:
    udp_server_endpoint_impl() = delete;
    udp_server_endpoint_impl(const udp_server_endpoint_impl&) = delete;
    udp_server_endpoint_impl(udp_server_endpoint_impl&&) = delete;
    udp_server_endpoint_impl(const std::shared_ptr<boardnet_endpoint_host>& _boardnet_endpoint_host,
                             const std::shared_ptr<boardnet_routing_host>& _routing_host, boost::asio::io_context& _io,
                             const std::shared_ptr<configuration>& _configuration);
    ~udp_server_endpoint_impl() override;

    udp_server_endpoint_impl& operator=(const udp_server_endpoint_impl&) = delete;
    udp_server_endpoint_impl& operator=(udp_server_endpoint_impl&&) = delete;

    void init(const endpoint_type& _local, boost::system::error_code& _error) override;
    void start() override;
    void stop(bool _due_to_error) override;
    void restart(bool _force) override;
    void receive() override;

    bool send_to(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size) override;
    bool send_error(const std::shared_ptr<endpoint_definition> _target, const byte_t* _data, uint32_t _size) override;
    bool send_queued(const target_data_iterator_type _it) override;
    void get_configured_times_from_endpoint(service_t _service, method_t _method, std::chrono::nanoseconds* _debouncing,
                                            std::chrono::nanoseconds* _maximum_retention) const override;

    VSOMEIP_EXPORT void join(const std::string& _address);
    VSOMEIP_EXPORT void join_unlocked(const std::string& _address);
    VSOMEIP_EXPORT void leave(const std::string& _address);
    VSOMEIP_EXPORT void set_multicast_option(const boost::asio::ip::address& _address, bool _is_join, boost::system::error_code& _error);

    void add_default_target(service_t _service, const std::string& _address, uint16_t _port) override;
    void remove_default_target(service_t _service) override;
    bool get_default_target(service_t _service, endpoint_type& _target) const override;

    uint16_t get_local_port() const override;
    bool is_local() const override;

    void print_status() override;
    bool is_reliable() const override;

    // Callback to sent messages
    void set_unicast_sent_callback(const on_unicast_sent_cbk_t& _cbk);
    // to own multicast messages received
    void set_sent_multicast_received_callback(const on_sent_multicast_received_cbk_t& _cbk);
    void set_receive_own_multicast_messages(bool value);

    bool is_joining() const;
    bool is_joined(const std::string& _address) const;
    bool is_joined(const std::string& _address, bool& _received) const;

    /**
     * @brief Block until all data is sent
     *
     * NOTE: that does mean _all_ data - no matter whether it is in the immediate queue, or in the dispatched trains, or in the
     * to-be-dispatched train, so beware of dispatching/debouncing delays!
     */
    void wait_until_sent();

private:
    void start_unlocked();
    void stop_unlocked();
    void init_unlocked(const endpoint_type& _local, boost::system::error_code& _error);

    bool send_queued_unlocked(const target_data_iterator_type _it);
    void leave_unlocked(const std::string& _address);
    void set_broadcast();
    void receive_unicast_unlocked(std::shared_ptr<message_buffer_t> _unicast_recv_buffer);
    void receive_multicast_unlocked(std::shared_ptr<message_buffer_t> _multicast_recv_buffer,
                                    std::shared_ptr<endpoint_type> _multicast_sender);
    bool is_joined_unlocked(const std::string& _address) const;
    bool is_joined_unlocked(const std::string& _address, bool& _received) const;
    std::string get_remote_information(const target_data_iterator_type _it) const override;
    std::string get_remote_information(const endpoint_type& _remote) const override;

    std::string get_address_port_local_unlocked() const;
    bool tp_segmentation_enabled(service_t _service, instance_t _instance, method_t _method) const override;

    void on_unicast_received(const boost::system::error_code& _error, std::size_t _bytes, const message_buffer_t& _unicast_recv_buffer);

    void on_multicast_received(const boost::system::error_code& _error, std::size_t _bytes, const message_buffer_t& _multicast_recv_buffer,
                               const endpoint_type& _multicast_sender);

    void on_message_received_unlocked(const boost::system::error_code& _error, std::size_t _bytes, bool _is_multicast,
                                      const endpoint_type& _remote, const message_buffer_t& _buffer);

    bool is_same_subnet_unlocked(const boost::asio::ip::address& _address) const;

    auto shared_ptr() { return std::shared_ptr<udp_server_endpoint_impl>(shared_from_this(), this); }

private:
    mutable std::mutex sync_;

    std::unique_ptr<udp_socket> unicast_socket_;
    endpoint_type unicast_remote_;

    std::unique_ptr<udp_socket> multicast_socket_;
    std::unique_ptr<endpoint_type> multicast_local_;

    std::atomic<unsigned> lifecycle_idx_; // for unicast jhandler (and otherwise for usei as a whole)
    std::atomic<unsigned> multicast_lifecycle_idx_; // for multicast handler
    std::map<std::string, bool, std::less<>> joined_;
    std::map<std::string, bool, std::less<>> join_status_;

    std::map<service_t, endpoint_type> default_targets_;

    boost::asio::ip::address netmask_;
    uint16_t prefix_{0};

    std::shared_ptr<tp::tp_reassembler> tp_reassembler_;
    boost::asio::steady_timer tp_cleanup_timer_;

    std::chrono::steady_clock::time_point last_sent_;

    // Atomic so the logger can print this variable without a lock
    std::atomic<bool> is_stopped_{true};
    bool is_v4_{true};

    // to tracking sent messages
    on_unicast_sent_cbk_t on_unicast_sent_{nullptr};

    // to receive own multicast messages
    bool receive_own_multicast_messages_{false};
    on_sent_multicast_received_cbk_t on_sent_multicast_received_{nullptr};

    std::string instance_name_;
};

} // namespace vsomeip_v3
