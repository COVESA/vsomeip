// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <fstream>

#include <vsomeip/runtime.hpp>

#include "logger_ext.hpp"
#include "../include/routing_manager_base.hpp"
#include "../../endpoints/include/local_endpoint.hpp"
#include "../../configuration/include/debounce_filter_impl.hpp"
#include "../../protocol/include/send_command.hpp"
#include "../../security/include/policy_manager_impl.hpp"
#include "../../security/include/security.hpp"
#include "../../tracing/include/connector_impl.hpp"
#include "../../utility/include/bithelper.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip_v3 {

#define VSOMEIP_LOG_PREFIX "rmb"

routing_manager_base::routing_manager_base(routing_manager_host* _host) :
    host_(_host), io_(host_->get_io()), configuration_(host_->get_configuration()), tc_(trace::connector_impl::get()) {
    const std::size_t its_max = configuration_->get_io_thread_count(host_->get_name());
    const uint32_t its_buffer_shrink_threshold = configuration_->get_buffer_shrink_threshold();

    for (std::size_t i = 0; i < its_max; ++i) {
        serializers_.push(std::make_shared<serializer>(its_buffer_shrink_threshold));
        deserializers_.push(std::make_shared<deserializer>(its_buffer_shrink_threshold));
    }
}

boost::asio::io_context& routing_manager_base::get_io() {

    return io_;
}

client_t routing_manager_base::get_client() const {

    return host_->get_client();
}

void routing_manager_base::set_client(const client_t& _client) {

    host_->set_client(_client);
}

session_t routing_manager_base::get_session(bool _is_request) {
    return host_->get_session(_is_request);
}

vsomeip_sec_client_t routing_manager_base::get_sec_client() const {
    return host_->get_sec_client();
}

void routing_manager_base::set_sec_client_port(port_t _port) {

    host_->set_sec_client_port(_port);
}

std::string routing_manager_base::get_client_host() const {
    std::scoped_lock its_env_lock(env_mutex_);
    return env_;
}

void routing_manager_base::set_client_host(const std::string& _client_host) {

    std::scoped_lock its_env_lock(env_mutex_);
    env_ = _client_host;
}

void routing_manager_base::log_network_state(bool _tcp, bool _only_external) const {
#ifndef __linux__
    (void)_tcp;
    (void)_only_external;
#else
    const std::string filename = _tcp ? "/proc/net/tcp" : "/proc/net/udp";

    std::ifstream file(filename);
    if (!file.is_open()) {
        VSOMEIP_ERROR_P << "Could not open " << filename << " for reading";
        return;
    }

    std::string line;
    // skip header line
    if (!std::getline(file, line)) {
        VSOMEIP_ERROR_P << "Failed to read header from " << filename;
        return;
    }

    if (file.peek() == std::ifstream::traits_type::eof()) {
        return;
    }

    // compute external address in hex, same format as /proc/net/tcp
    // e.g., 127.0.0.1 => 0100007F
    auto external_addr = configuration_->get_unicast_address();
    std::ostringstream external_addr_stream;
    external_addr_stream << std::uppercase << std::hex << std::setfill('0') << std::setw(8)
                         << (external_addr.is_v4() ? htonl(external_addr.to_v4().to_uint()) : 0);
    std::string external_addr_hex = external_addr_stream.str();

    VSOMEIP_INFO_P << (_tcp ? "TCP" : "UDP") << " connections";
    VSOMEIP_INFO << "idx, local_addr, remote_addr, state, tx_queue:rx_queue, timer_active:tm_when, retrnsmt, uid, unanswered, inode";
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string idx, local_addr, remote_addr, state, tx_rx_queue, timer, retrnsmt, uid, unanswered, inode;

        // see https://www.kernel.org/doc/Documentation/networking/proc_net_tcp.txt
        // NOTE: there are more fields (especially in /proc/net/udp), but we do not care about these
        if (!(iss >> idx >> local_addr >> remote_addr >> state >> tx_rx_queue >> timer >> retrnsmt >> uid >> unanswered >> inode)) {
            VSOMEIP_ERROR_P << "Failed to parse line: " << line;
            continue;
        }

        if (_only_external && local_addr.substr(0, external_addr_hex.length()) != external_addr_hex) {
            continue;
        }

        // parse local_addr/remote_addr, e.g., 0100007F:7424 to 127.0.0.1:29732
        try {
            std::string local_ip = local_addr.substr(0, 8);
            std::string local_port = local_addr.substr(9, 4);
            std::string remote_ip = remote_addr.substr(0, 8);
            std::string remote_port = remote_addr.substr(9, 4);

            // convert hex to decimal
            unsigned int local_ip_int = htonl(static_cast<unsigned int>(std::stoul(local_ip, nullptr, 16)));
            unsigned int remote_ip_int = htonl(static_cast<unsigned int>(std::stoul(remote_ip, nullptr, 16)));
            auto local_port_int = static_cast<unsigned int>(std::stoul(local_port, nullptr, 16));
            auto remote_port_int = static_cast<unsigned int>(std::stoul(remote_port, nullptr, 16));

            // convert to dotted-decimal notation
            local_addr = boost::asio::ip::address_v4(local_ip_int).to_string() + ":" + std::to_string(local_port_int);
            remote_addr = boost::asio::ip::address_v4(remote_ip_int).to_string() + ":" + std::to_string(remote_port_int);
        } catch (...) {
            // leave the old local_addr/remote_addr
        }

        // use the same format as /proc/net/tcp
        // especially with local TCP communication, there will be *MANY* connections, and the hex encoding does make the logs smaller
        VSOMEIP_INFO << idx << " " << local_addr << " " << remote_addr << " " << state << " " << tx_rx_queue << " " << timer << " "
                     << retrnsmt << " " << uid << " " << unanswered << " " << inode;
    }

#endif
}

void routing_manager_base::request_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                           minor_version_t _minor) {
    auto its_info = find_service(_service, _instance);
    if (its_info) {
        if (_major == its_info->get_major() || DEFAULT_MAJOR == its_info->get_major() || ANY_MAJOR == _major) {
            its_info->add_client(_client);
        } else {
            VSOMEIP_ERROR_P << "Service property mismatch (" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":"
                            << static_cast<std::uint32_t>(its_info->get_major()) << "." << its_info->get_minor()
                            << "] passed: " << static_cast<std::uint32_t>(_major) << ":" << _minor;
        }
    }
}

void routing_manager_base::release_service(client_t _client, service_t _service, instance_t _instance) {
    auto its_info = find_service(_service, _instance);
    if (its_info) {
        its_info->remove_client(_client);
    }
}

std::string const& routing_manager_base::get_name() const {
    return host_->get_name();
}

// ********************************* PROTECTED **************************************

bool routing_manager_base::send_local(std::shared_ptr<local_endpoint>& _target, client_t _client, const byte_t* _data, uint32_t _size,
                                      instance_t _instance, bool _reliable, protocol::id_e _command, uint8_t _status_check,
                                      client_t _sender) const {

    protocol::send_command its_command(_command);
    its_command.set_client(_sender);
    its_command.set_instance(_instance);
    its_command.set_reliable(_reliable);
    its_command.set_status(_status_check);
    its_command.set_target(_client);
    its_command.set_message(std::vector<byte_t>(_data, _data + _size));

    std::vector<byte_t> its_buffer;
    its_command.serialize(its_buffer);

    return _target->send(&its_buffer[0], uint32_t(its_buffer.size()));
}

std::shared_ptr<serializer> routing_manager_base::get_serializer() {

    std::unique_lock<std::mutex> its_lock(serializer_mutex_);
    while (serializers_.empty()) {
        VSOMEIP_INFO_P << "Client 0x" << hex4(get_client()) << " has no available serializer. Waiting...";
        serializer_condition_.wait(its_lock, [this] { return !serializers_.empty(); });
        VSOMEIP_INFO_P << ": Client 0x" << hex4(get_client()) << " now checking for available serializer.";
    }

    auto its_serializer = serializers_.front();
    serializers_.pop();

    return its_serializer;
}

void routing_manager_base::put_serializer(const std::shared_ptr<serializer>& _serializer) {

    std::scoped_lock its_lock(serializer_mutex_);
    serializers_.push(_serializer);
    serializer_condition_.notify_one();
}

std::shared_ptr<deserializer> routing_manager_base::get_deserializer() {

    std::unique_lock<std::mutex> its_lock(deserializer_mutex_);
    while (deserializers_.empty()) {
        VSOMEIP_INFO_P << ": Client 0x" << hex4(get_client()) << "~> all in use!";
        deserializer_condition_.wait(its_lock, [this] { return !deserializers_.empty(); });
        VSOMEIP_INFO_P << "Client 0x" << hex4(get_client()) << "~> wait finished!";
    }

    auto its_deserializer = deserializers_.front();
    deserializers_.pop();

    return its_deserializer;
}

void routing_manager_base::put_deserializer(const std::shared_ptr<deserializer>& _deserializer) {

    std::scoped_lock its_lock(deserializer_mutex_);
    deserializers_.push(_deserializer);
    deserializer_condition_.notify_one();
}

} // namespace vsomeip_v3
