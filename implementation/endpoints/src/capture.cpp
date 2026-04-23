// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/capture.hpp"

#include <vsomeip/internal/logger.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <map>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

namespace vsomeip_v3 {

namespace {

constexpr const char* k_capture_path_env = "APEX_VSOMEIP_WRITE_PCAP";

constexpr std::uint32_t k_pcap_magic = 0xa1b2c3d4U;
constexpr std::uint16_t k_pcap_major_version = 2U;
constexpr std::uint16_t k_pcap_minor_version = 4U;
constexpr std::uint32_t k_pcap_reserved = 0U;
constexpr std::uint32_t k_pcap_snaplen = 65535U;
constexpr std::uint32_t k_pcap_linktype_raw = 101U;

constexpr std::uint8_t k_ipv4_version_ihl = 0x45U;
constexpr std::uint8_t k_ipv4_ttl = 64U;
constexpr std::uint8_t k_ipv6_version = 0x60U;
constexpr std::uint8_t k_ipv6_hop_limit = 64U;
constexpr std::uint8_t k_tcp_flags_ack_psh = 0x18U;
constexpr std::uint16_t k_tcp_window = 65535U;

constexpr std::size_t k_ipv4_header_size = 20U;
constexpr std::size_t k_ipv6_header_size = 40U;
constexpr std::size_t k_udp_header_size = 8U;
constexpr std::size_t k_tcp_header_size = 20U;
constexpr std::size_t k_max_ipv4_packet_size = 65535U;
constexpr std::size_t k_max_ipv6_payload_size = 65535U;

constexpr std::uint8_t k_ip_protocol_tcp = 6U;
constexpr std::uint8_t k_ip_protocol_udp = 17U;

enum class ip_version_e {
    v4,
    v6
};

struct ip_endpoint_t {
    ip_version_e version{ip_version_e::v4};
    std::array<byte_t, 16U> address{};
    std::uint16_t port{0U};
};

struct packet_endpoints_t {
    ip_endpoint_t source;
    ip_endpoint_t destination;
};

struct tcp_flow_key_t {
    ip_endpoint_t first;
    ip_endpoint_t second;

    bool operator<(const tcp_flow_key_t& _other) const {
        return std::tie(first.version, first.address, first.port, second.version, second.address, second.port)
                < std::tie(_other.first.version, _other.first.address, _other.first.port,
                           _other.second.version, _other.second.address, _other.second.port);
    }
};

struct tcp_flow_state_t {
    std::array<std::uint32_t, 2U> next_sequence{{1U, 1U}};
};

struct capture_state_t {
    bool enabled{false};
    std::FILE* file{nullptr};
    std::mutex mutex;
    std::uint16_t ipv4_identification{0U};
    bool warning_logged{false};
    std::map<tcp_flow_key_t, tcp_flow_state_t> tcp_flows;

    ~capture_state_t() {
        if (file != nullptr) {
            std::fclose(file);
        }
    }
};

struct tcp_flow_lookup_t {
    tcp_flow_key_t key;
    std::size_t direction_index;
};

template <typename Value>
void append_le(std::vector<byte_t>& _buffer, Value _value) {
    for (std::size_t i = 0; i < sizeof(Value); ++i) {
        _buffer.push_back(static_cast<byte_t>((static_cast<std::uint64_t>(_value) >> (i * 8U)) & 0xFFU));
    }
}

template <typename Value>
void append_be(std::vector<byte_t>& _buffer, Value _value) {
    for (std::size_t i = 0; i < sizeof(Value); ++i) {
        const std::size_t its_shift = (sizeof(Value) - 1U - i) * 8U;
        _buffer.push_back(static_cast<byte_t>((static_cast<std::uint64_t>(_value) >> its_shift) & 0xFFU));
    }
}

bool write_all(std::FILE* _file, const byte_t* _data, std::size_t _size) {
    return _size == 0U || std::fwrite(_data, 1U, _size, _file) == _size;
}

bool write_global_header(std::FILE* _file) {
    std::vector<byte_t> its_header;
    its_header.reserve(24U);
    append_le<std::uint32_t>(its_header, k_pcap_magic);
    append_le<std::uint16_t>(its_header, k_pcap_major_version);
    append_le<std::uint16_t>(its_header, k_pcap_minor_version);
    append_le<std::uint32_t>(its_header, k_pcap_reserved);
    append_le<std::uint32_t>(its_header, k_pcap_reserved);
    append_le<std::uint32_t>(its_header, k_pcap_snaplen);
    append_le<std::uint32_t>(its_header, k_pcap_linktype_raw);
    return write_all(_file, its_header.data(), its_header.size());
}

void initialize_capture_state(capture_state_t& _state) {
    const char* its_path = std::getenv(k_capture_path_env);
    if (its_path == nullptr || its_path[0] == '\0') {
        return;
    }

    _state.file = std::fopen(its_path, "wb");
    if (_state.file == nullptr) {
        VSOMEIP_WARNING << "capture::capture: failed to open pcap file " << its_path << ": " << std::strerror(errno);
        return;
    }

    if (!write_global_header(_state.file)) {
        VSOMEIP_WARNING << "capture::capture: failed to write pcap global header to " << its_path;
        std::fclose(_state.file);
        _state.file = nullptr;
        return;
    }

    _state.enabled = true;
}

capture_state_t& get_capture_state() {
    static capture_state_t its_state;
    static const bool its_initialized = [] {
        initialize_capture_state(its_state);
        return true;
    }();
    (void)its_initialized;
    return its_state;
}

void log_capture_warning_once(capture_state_t& _state, const char* _message) {
    if (!_state.warning_logged) {
        _state.warning_logged = true;
        VSOMEIP_WARNING << _message;
    }
}

bool to_ip_endpoint(const boost::asio::ip::address& _address, port_t _port, ip_endpoint_t& _endpoint) {
    _endpoint.address.fill(0U);
    if (_address.is_v4()) {
        _endpoint.version = ip_version_e::v4;
        const auto its_bytes = _address.to_v4().to_bytes();
        std::copy(its_bytes.begin(), its_bytes.end(), _endpoint.address.begin());
    } else if (_address.is_v6()) {
        _endpoint.version = ip_version_e::v6;
        _endpoint.address = _address.to_v6().to_bytes();
    } else {
        return false;
    }
    _endpoint.port = _port;
    return true;
}

bool get_packet_endpoints(const capture_metadata_t& _metadata, packet_endpoints_t& _endpoints) {
    boost::asio::ip::address its_source_address;
    boost::asio::ip::address its_destination_address;
    port_t its_source_port;
    port_t its_destination_port;

    if (_metadata.direction == capture_direction_e::TX) {
        its_source_address = _metadata.local_address;
        its_source_port = _metadata.local_port;
        its_destination_address = _metadata.remote_address;
        its_destination_port = _metadata.remote_port;
    } else {
        its_source_address = _metadata.remote_address;
        its_source_port = _metadata.remote_port;
        its_destination_address =
                _metadata.destination_address.has_value() ? *_metadata.destination_address : _metadata.local_address;
        its_destination_port = _metadata.local_port;
    }

    if (its_source_address.is_v4() != its_destination_address.is_v4()
            || its_source_address.is_v6() != its_destination_address.is_v6()) {
        return false;
    }

    return to_ip_endpoint(its_source_address, its_source_port, _endpoints.source)
            && to_ip_endpoint(its_destination_address, its_destination_port, _endpoints.destination);
}

void checksum_accumulate(std::uint32_t& _sum, const byte_t* _data, std::size_t _size) {
    std::size_t i = 0U;
    for (; i + 1U < _size; i += 2U) {
        _sum += static_cast<std::uint16_t>((static_cast<std::uint16_t>(_data[i]) << 8U) | _data[i + 1U]);
    }
    if (i < _size) {
        _sum += static_cast<std::uint16_t>(static_cast<std::uint16_t>(_data[i]) << 8U);
    }
}

std::uint16_t finalize_checksum(std::uint32_t _sum) {
    while ((_sum >> 16U) != 0U) {
        _sum = (_sum & 0xFFFFU) + (_sum >> 16U);
    }
    return static_cast<std::uint16_t>(~_sum & 0xFFFFU);
}

std::uint16_t compute_ipv4_header_checksum(const std::vector<byte_t>& _header) {
    std::uint32_t its_sum = 0U;
    checksum_accumulate(its_sum, _header.data(), _header.size());
    return finalize_checksum(its_sum);
}

std::uint16_t compute_transport_checksum(const ip_endpoint_t& _source, const ip_endpoint_t& _destination,
                                         std::uint8_t _protocol, const std::vector<byte_t>& _segment,
                                         bool _force_non_zero) {
    std::uint32_t its_sum = 0U;

    if (_source.version == ip_version_e::v4) {
        checksum_accumulate(its_sum, _source.address.data(), 4U);
        checksum_accumulate(its_sum, _destination.address.data(), 4U);
        its_sum += _protocol;
        its_sum += static_cast<std::uint32_t>(_segment.size());
    } else {
        checksum_accumulate(its_sum, _source.address.data(), 16U);
        checksum_accumulate(its_sum, _destination.address.data(), 16U);
        its_sum += static_cast<std::uint32_t>(_segment.size() >> 16U);
        its_sum += static_cast<std::uint32_t>(_segment.size() & 0xFFFFU);
        its_sum += _protocol;
    }

    checksum_accumulate(its_sum, _segment.data(), _segment.size());
    std::uint16_t its_checksum = finalize_checksum(its_sum);
    if (_force_non_zero && its_checksum == 0U) {
        its_checksum = 0xFFFFU;
    }
    return its_checksum;
}

void append_ipv4_header(std::vector<byte_t>& _packet, const packet_endpoints_t& _endpoints, std::uint8_t _protocol,
                        std::size_t _payload_size, std::uint16_t _identification) {
    std::vector<byte_t> its_header;
    its_header.reserve(k_ipv4_header_size);
    append_be<std::uint8_t>(its_header, k_ipv4_version_ihl);
    append_be<std::uint8_t>(its_header, 0U);
    append_be<std::uint16_t>(its_header, static_cast<std::uint16_t>(k_ipv4_header_size + _payload_size));
    append_be<std::uint16_t>(its_header, _identification);
    append_be<std::uint16_t>(its_header, 0U);
    append_be<std::uint8_t>(its_header, k_ipv4_ttl);
    append_be<std::uint8_t>(its_header, _protocol);
    append_be<std::uint16_t>(its_header, 0U);
    its_header.insert(its_header.end(), _endpoints.source.address.begin(), _endpoints.source.address.begin() + 4U);
    its_header.insert(its_header.end(), _endpoints.destination.address.begin(), _endpoints.destination.address.begin() + 4U);
    const std::uint16_t its_checksum = compute_ipv4_header_checksum(its_header);
    its_header[10] = static_cast<byte_t>((its_checksum >> 8U) & 0xFFU);
    its_header[11] = static_cast<byte_t>(its_checksum & 0xFFU);
    _packet.insert(_packet.end(), its_header.begin(), its_header.end());
}

void append_ipv6_header(std::vector<byte_t>& _packet, const packet_endpoints_t& _endpoints, std::uint8_t _next_header,
                        std::size_t _payload_size) {
    append_be<std::uint32_t>(_packet, static_cast<std::uint32_t>(k_ipv6_version) << 24U);
    append_be<std::uint16_t>(_packet, static_cast<std::uint16_t>(_payload_size));
    append_be<std::uint8_t>(_packet, _next_header);
    append_be<std::uint8_t>(_packet, k_ipv6_hop_limit);
    _packet.insert(_packet.end(), _endpoints.source.address.begin(), _endpoints.source.address.begin() + 16U);
    _packet.insert(_packet.end(), _endpoints.destination.address.begin(), _endpoints.destination.address.begin() + 16U);
}

bool build_udp_packet(const packet_endpoints_t& _endpoints, const byte_t* _bytes, std::size_t _len,
                      std::uint16_t _identification, std::vector<byte_t>& _packet) {
    const std::size_t its_network_header_size =
            _endpoints.source.version == ip_version_e::v4 ? k_ipv4_header_size : k_ipv6_header_size;
    const std::size_t its_max_payload = _endpoints.source.version == ip_version_e::v4
            ? (k_max_ipv4_packet_size - its_network_header_size - k_udp_header_size)
            : (k_max_ipv6_payload_size - k_udp_header_size);
    if (_len > its_max_payload) {
        return false;
    }

    std::vector<byte_t> its_udp_segment;
    its_udp_segment.reserve(k_udp_header_size + _len);
    append_be<std::uint16_t>(its_udp_segment, _endpoints.source.port);
    append_be<std::uint16_t>(its_udp_segment, _endpoints.destination.port);
    append_be<std::uint16_t>(its_udp_segment, static_cast<std::uint16_t>(k_udp_header_size + _len));
    append_be<std::uint16_t>(its_udp_segment, 0U);
    its_udp_segment.insert(its_udp_segment.end(), _bytes, _bytes + _len);
    const std::uint16_t its_checksum =
            compute_transport_checksum(_endpoints.source, _endpoints.destination, k_ip_protocol_udp, its_udp_segment, true);
    its_udp_segment[6] = static_cast<byte_t>((its_checksum >> 8U) & 0xFFU);
    its_udp_segment[7] = static_cast<byte_t>(its_checksum & 0xFFU);

    _packet.clear();
    _packet.reserve(its_network_header_size + its_udp_segment.size());
    if (_endpoints.source.version == ip_version_e::v4) {
        append_ipv4_header(_packet, _endpoints, k_ip_protocol_udp, its_udp_segment.size(), _identification);
    } else {
        append_ipv6_header(_packet, _endpoints, k_ip_protocol_udp, its_udp_segment.size());
    }
    _packet.insert(_packet.end(), its_udp_segment.begin(), its_udp_segment.end());
    return true;
}

tcp_flow_lookup_t make_tcp_flow_lookup(const packet_endpoints_t& _endpoints) {
    tcp_flow_lookup_t its_lookup{{_endpoints.source, _endpoints.destination}, 0U};
    if (std::tie(_endpoints.destination.version, _endpoints.destination.address, _endpoints.destination.port)
            < std::tie(_endpoints.source.version, _endpoints.source.address, _endpoints.source.port)) {
        std::swap(its_lookup.key.first, its_lookup.key.second);
        its_lookup.direction_index = 1U;
    }
    return its_lookup;
}

bool write_packet_record(std::FILE* _file, const std::vector<byte_t>& _packet) {
    const auto its_now = std::chrono::system_clock::now().time_since_epoch();
    const auto its_seconds = std::chrono::duration_cast<std::chrono::seconds>(its_now);
    const auto its_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(its_now - its_seconds);

    std::vector<byte_t> its_record_header;
    its_record_header.reserve(16U);
    append_le<std::uint32_t>(its_record_header, static_cast<std::uint32_t>(its_seconds.count()));
    append_le<std::uint32_t>(its_record_header, static_cast<std::uint32_t>(its_microseconds.count()));
    append_le<std::uint32_t>(its_record_header, static_cast<std::uint32_t>(_packet.size()));
    append_le<std::uint32_t>(its_record_header, static_cast<std::uint32_t>(_packet.size()));

    return write_all(_file, its_record_header.data(), its_record_header.size())
            && write_all(_file, _packet.data(), _packet.size());
}

bool build_tcp_packets(capture_state_t& _state, const packet_endpoints_t& _endpoints, const byte_t* _bytes, std::size_t _len,
                       std::vector<std::vector<byte_t>>& _packets) {
    const std::size_t its_network_header_size =
            _endpoints.source.version == ip_version_e::v4 ? k_ipv4_header_size : k_ipv6_header_size;
    const std::size_t its_max_payload = _endpoints.source.version == ip_version_e::v4
            ? (k_max_ipv4_packet_size - its_network_header_size - k_tcp_header_size)
            : (k_max_ipv6_payload_size - k_tcp_header_size);

    const auto its_lookup = make_tcp_flow_lookup(_endpoints);
    auto& its_flow_state = _state.tcp_flows[its_lookup.key];
    std::uint32_t its_sequence = its_flow_state.next_sequence[its_lookup.direction_index];
    const std::uint32_t its_acknowledgement = its_flow_state.next_sequence[1U - its_lookup.direction_index];

    _packets.clear();
    _packets.reserve(std::max<std::size_t>(1U, (_len + its_max_payload - 1U) / its_max_payload));

    std::size_t its_offset = 0U;
    while (its_offset < _len) {
        const std::size_t its_chunk_size = std::min(its_max_payload, _len - its_offset);
        std::vector<byte_t> its_tcp_segment;
        its_tcp_segment.reserve(k_tcp_header_size + its_chunk_size);
        append_be<std::uint16_t>(its_tcp_segment, _endpoints.source.port);
        append_be<std::uint16_t>(its_tcp_segment, _endpoints.destination.port);
        append_be<std::uint32_t>(its_tcp_segment, its_sequence);
        append_be<std::uint32_t>(its_tcp_segment, its_acknowledgement);
        append_be<std::uint8_t>(its_tcp_segment, static_cast<std::uint8_t>(k_tcp_header_size << 2U));
        append_be<std::uint8_t>(its_tcp_segment, k_tcp_flags_ack_psh);
        append_be<std::uint16_t>(its_tcp_segment, k_tcp_window);
        append_be<std::uint16_t>(its_tcp_segment, 0U);
        append_be<std::uint16_t>(its_tcp_segment, 0U);
        its_tcp_segment.insert(its_tcp_segment.end(), _bytes + its_offset, _bytes + its_offset + its_chunk_size);
        const std::uint16_t its_checksum =
                compute_transport_checksum(_endpoints.source, _endpoints.destination, k_ip_protocol_tcp, its_tcp_segment, false);
        its_tcp_segment[16] = static_cast<byte_t>((its_checksum >> 8U) & 0xFFU);
        its_tcp_segment[17] = static_cast<byte_t>(its_checksum & 0xFFU);

        std::vector<byte_t> its_packet;
        its_packet.reserve(its_network_header_size + its_tcp_segment.size());
        if (_endpoints.source.version == ip_version_e::v4) {
            append_ipv4_header(its_packet, _endpoints, k_ip_protocol_tcp, its_tcp_segment.size(), _state.ipv4_identification++);
        } else {
            append_ipv6_header(its_packet, _endpoints, k_ip_protocol_tcp, its_tcp_segment.size());
        }
        its_packet.insert(its_packet.end(), its_tcp_segment.begin(), its_tcp_segment.end());
        _packets.push_back(std::move(its_packet));

        its_sequence += static_cast<std::uint32_t>(its_chunk_size);
        its_offset += its_chunk_size;
    }

    its_flow_state.next_sequence[its_lookup.direction_index] = its_sequence;
    return true;
}

} // namespace

void capture(const byte_t* _bytes, std::size_t _len, const capture_metadata_t& _metadata) {
    capture_state_t& its_state = get_capture_state();
    if (!its_state.enabled) {
        return;
    }

    std::lock_guard<std::mutex> its_lock(its_state.mutex);

    packet_endpoints_t its_endpoints;
    if (!get_packet_endpoints(_metadata, its_endpoints)) {
        log_capture_warning_once(its_state, "capture::capture: dropping packet due to inconsistent endpoint metadata");
        return;
    }

    bool its_success = false;
    if (_metadata.transport == capture_transport_e::UDP) {
        std::vector<byte_t> its_packet;
        const std::uint16_t its_identification = its_state.ipv4_identification++;
        its_success = build_udp_packet(its_endpoints, _bytes, _len, its_identification, its_packet)
                && write_packet_record(its_state.file, its_packet);
    } else {
        std::vector<std::vector<byte_t>> its_packets;
        its_success = build_tcp_packets(its_state, its_endpoints, _bytes, _len, its_packets);
        if (its_success) {
            for (const auto& its_packet : its_packets) {
                if (!write_packet_record(its_state.file, its_packet)) {
                    its_success = false;
                    break;
                }
            }
        }
    }

    if (!its_success) {
        log_capture_warning_once(its_state, "capture::capture: failed to serialize packet to pcap output");
        its_state.enabled = false;
        if (its_state.file != nullptr) {
            std::fclose(its_state.file);
            its_state.file = nullptr;
        }
    }
}

} // namespace vsomeip_v3
