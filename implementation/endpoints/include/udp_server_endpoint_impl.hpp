// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_UDP_SERVER_ENDPOINT_IMPL_HPP_
#define VSOMEIP_V3_UDP_SERVER_ENDPOINT_IMPL_HPP_

#include <boost/asio/ip/udp.hpp>
#include <vsomeip/defines.hpp>

#include "server_endpoint_impl.hpp"
#include "tp_reassembler.hpp"

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
    udp_server_endpoint_impl(const std::shared_ptr<endpoint_host>& _endpoint_host, const std::shared_ptr<routing_host>& _routing_host,
                             boost::asio::io_context& _io, const std::shared_ptr<configuration>& _configuration);
    ~udp_server_endpoint_impl() override;

    udp_server_endpoint_impl& operator=(const udp_server_endpoint_impl&) = delete;
    udp_server_endpoint_impl& operator=(udp_server_endpoint_impl&&) = delete;

    void init(const endpoint_type& _local, boost::system::error_code& _error) override;
    void start() override;
    void stop() override;
    void restart(bool _force) override;
    void receive() override;

    bool is_closed() const override;

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
    void set_local_port(uint16_t _port) override;
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

    /// @brief Disconnects from the given client.
    ///
    /// @param _client ID of the remote client.
    void disconnect_from(const client_t _client) override;

private:
    void start_unlocked();
    void stop_unlocked();
    void init_unlocked(const endpoint_type& _local, boost::system::error_code& _error);

    bool send_queued_unlocked(const target_data_iterator_type _it);
    void leave_unlocked(const std::string& _address);
    void set_broadcast();
    void receive_unicast_unlocked();
    void receive_multicast_unlocked();
    bool is_joined_unlocked(const std::string& _address) const;
    bool is_joined_unlocked(const std::string& _address, bool& _received) const;
    std::string get_remote_information(const target_data_iterator_type _it) const override;
    std::string get_remote_information(const endpoint_type& _remote) const override;

    std::string get_address_port_local_unlocked() const;
    bool tp_segmentation_enabled(service_t _service, instance_t _instance, method_t _method) const override;

    void on_unicast_received(boost::system::error_code const& _error, std::size_t _bytes);

    void on_multicast_received(boost::system::error_code const& _error, std::size_t _bytes, const boost::asio::ip::udp::endpoint& _sender,
                               const boost::asio::ip::address& _destination);

    void on_message_received_unlocked(boost::system::error_code const& _error, std::size_t _bytes, bool _is_multicast,
                                      endpoint_type const& _remote, message_buffer_t const& _buffer);

    bool is_same_subnet_unlocked(const boost::asio::ip::address& _address) const;

    auto shared_ptr() { return std::shared_ptr<udp_server_endpoint_impl>(shared_from_this(), this); }

private:
    mutable std::mutex sync_;

    std::shared_ptr<socket_type> unicast_socket_;
    endpoint_type unicast_remote_;
    message_buffer_t unicast_recv_buffer_;

    std::shared_ptr<socket_type> multicast_socket_;
    std::unique_ptr<endpoint_type> multicast_local_;
    message_buffer_t multicast_recv_buffer_;
    std::atomic<unsigned> lifecycle_idx_;
    std::map<std::string, bool, std::less<>> joined_;
    std::map<std::string, bool, std::less<>> join_status_;

    std::map<service_t, endpoint_type> default_targets_;

    boost::asio::ip::address netmask_;
    uint16_t prefix_{0};

    uint16_t local_port_{0};

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

#endif // VSOMEIP_V3_UDP_SERVER_ENDPOINT_IMPL_HPP_
