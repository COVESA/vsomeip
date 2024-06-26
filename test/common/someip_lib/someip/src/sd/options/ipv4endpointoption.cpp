// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <someip/sd/options/ipv4endpointoption.hpp>

#include <com/comutils.hpp>

namespace vsomeip_utilities {
namespace someip {
namespace sd {
namespace options {

/**
 * @brief Create Builder
 *
 * @return SomeIpSdEntryBuilder
 */
Ipv4EndpointOptionBuilder Ipv4EndpointOption::create() {
    return Ipv4EndpointOptionBuilder();
}

// Attribute Getters
std::uint16_t Ipv4EndpointOption::portNumber() const {
    return m_podPayload.portNumber;
}

std::uint8_t Ipv4EndpointOption::protocol() const {
    return m_podPayload.protocol;
}

std::uint32_t Ipv4EndpointOption::ipv4Address() const {
    return m_podPayload.ipv4Address;
}

std::string Ipv4EndpointOption::ipv4AddressString() const {
    return com::utils::ipv4AddressFromInt(m_podPayload.ipv4Address).to_string();
}

// Message Attribute Setters
void Ipv4EndpointOption::setPortNumber(const std::uint16_t& _val) {
    m_podPayload.portNumber = _val;
}

void Ipv4EndpointOption::setProtocol(const std::uint8_t& _val) {
    m_podPayload.protocol = _val;
}

void Ipv4EndpointOption::setIpv4Address(const std::uint32_t& _val) {
    m_podPayload.ipv4Address = _val;
}

bool Ipv4EndpointOption::setIpv4Address(const std::string& _val) {
    bool ret = false;

    std::error_code ec;
    asio::ip::address_v4 addr = com::utils::ipv4AddressFromString(_val, ec);

    if (!ec) {
        setIpv4Address(addr.to_uint());
        ret = true;
    }

    return ret;
}

void Ipv4EndpointOption::setReserved(const std::uint8_t& _val) {
    m_podPayload.reserved = _val;
}

/**
 * @brief Print to stdout utility method
 *
 */
void Ipv4EndpointOption::print() const {
    // Store original format
    std::ios_base::fmtflags prevFmt(std::cout.flags());

    // Print payload
    std::cout << "[" __FILE__ "]"
        << "\n\tPOD size: " << serialization::PodPayload<Ipv4EndpointOptionRaw_t>::size()
        << "\n\t--" << std::hex << std::showbase
        << "\n\tIpv4 Address: " << unsigned(ipv4Address()) << " (" << ipv4AddressString() << ')'
        << "\n\tReserved: " << unsigned(m_podPayload.reserved)
        << "\n\tPort Number: " << std::dec << unsigned(portNumber()) << std::hex
        << "\n\tProtocol: " << unsigned(protocol()) << '\n';

    // Restore original format
    std::cout.flags(prevFmt);
}

Ipv4EndpointOptionBuilder::operator Ipv4EndpointOption() const {
    return std::move(m_entry);
}

// Setter Methods
Ipv4EndpointOptionBuilder& Ipv4EndpointOptionBuilder::portNumber(const std::uint16_t& _val) {
    m_entry.setPortNumber(_val);
    return *this;
}

Ipv4EndpointOptionBuilder& Ipv4EndpointOptionBuilder::protocol(const std::uint8_t& _val) {
    m_entry.setProtocol(_val);
    return *this;
}

Ipv4EndpointOptionBuilder& Ipv4EndpointOptionBuilder::ipv4Address(const std::uint32_t& _val) {
    m_entry.setIpv4Address(_val);
    return *this;
}

Ipv4EndpointOptionBuilder& Ipv4EndpointOptionBuilder::ipv4Address(const std::string& _val) {
    m_entry.setIpv4Address(_val);
    return *this;
}

Ipv4EndpointOptionBuilder& Ipv4EndpointOptionBuilder::reserved(const std::uint8_t& _val) {
    m_entry.setReserved(_val);
    return *this;
}

} // namespace options
} // namespace sd
} // namespace someip
} // namespace vsomeip_utilities
