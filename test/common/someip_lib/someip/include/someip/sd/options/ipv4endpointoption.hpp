// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __IPV4ENDPOINTOPTION_HPP__
#define __IPV4ENDPOINTOPTION_HPP__

#include <serialization/podpayload.hpp>

namespace vsomeip_utilities {
namespace someip {
namespace sd {
namespace options {

class Ipv4EndpointOptionBuilder;

// Inverted field representation to align with Big-Endian byte order
struct Ipv4EndpointOptionRaw_t {
    std::uint16_t portNumber = { 0 };
    std::uint8_t protocol = { 0 };
    std::uint8_t reserved = { 0 };
    std::uint32_t ipv4Address = { 0 };
};

class Ipv4EndpointOption
    : public serialization::PodPayload<Ipv4EndpointOptionRaw_t> {
public:
    // CTor
    Ipv4EndpointOption() = default;

    static Ipv4EndpointOptionBuilder create();

    std::uint16_t portNumber() const;
    std::uint8_t protocol() const;
    std::uint32_t ipv4Address() const;
    std::string ipv4AddressString() const;

    void setPortNumber(const std::uint16_t& _val);
    void setProtocol(const std::uint8_t& _val);
    void setIpv4Address(const std::uint32_t& _val);
    bool setIpv4Address(const std::string& _val);
    void setReserved(const std::uint8_t& _val);

    void print() const override;
};

/**
 * @brief Entry builder
 *
 */
class Ipv4EndpointOptionBuilder {
public:
    // CTor/DTor
    Ipv4EndpointOptionBuilder() = default;
    virtual ~Ipv4EndpointOptionBuilder() = default;

    operator Ipv4EndpointOption() const;

    Ipv4EndpointOptionBuilder& portNumber(const std::uint16_t& _val);
    Ipv4EndpointOptionBuilder& protocol(const std::uint8_t& _val);
    Ipv4EndpointOptionBuilder& ipv4Address(const std::uint32_t& _val);
    Ipv4EndpointOptionBuilder& ipv4Address(const std::string& _val);
    Ipv4EndpointOptionBuilder& reserved(const std::uint8_t& _val);

private:
    Ipv4EndpointOption m_entry;
};

} // options
} // sd
} // someip
} // vsomeip_utilities

#endif // __IPV4ENDPOINTOPTION_HPP__
