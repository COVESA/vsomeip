// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <someip/sd/someipsd.hpp>

namespace vsomeip_utilities {
namespace someip {
namespace sd {

// Header someipSD (4bytes) + Entries length (4bytes) + Options length (4bytes)
constexpr std::size_t SD_MIN_PAYLOAD_SIZE {sizeof(vsomeip_utilities::someip::sd::SomeIpSdHeaderRaw_t) +
                                        2 * sizeof(vsomeip_utilities::someip::sd::EntriesLengthRaw_t)};

SomeIpSd::SomeIpSd(const SomeIpSd& _other)
    : serialization::PodPayload<SomeIpSdHeaderRaw_t> { _other }
    , m_entries { _other.m_entries }
    , m_options { _other.m_options } {}


SomeIpSd::SomeIpSd(const SomeIpSd&& _other)
    : serialization::PodPayload<SomeIpSdHeaderRaw_t> { _other }
    , m_entries { std::move(_other.m_entries) }
    , m_options { std::move(_other.m_options) } {}

const SomeIpSd& SomeIpSd::operator=(const SomeIpSd& _other) {
    serialization::PodPayload<SomeIpSdHeaderRaw_t>::operator=(_other);
    m_entries = _other.m_entries;
    m_options = _other.m_options;

    return *this;
}

const SomeIpSd& SomeIpSd::operator=(const SomeIpSd&& _other) {
    serialization::PodPayload<SomeIpSdHeaderRaw_t>::operator=(_other);
    m_entries = std::move(_other.m_entries);
    m_options = std::move(_other.m_options);

    return *this;
}

SomeIpSdBuilder SomeIpSd::create() {
    return SomeIpSdBuilder();
}

std::size_t SomeIpSd::deserialize(std::istream& _stream) {
    std::size_t ret = 0;

    ret += serialization::PodPayload<SomeIpSdHeaderRaw_t>::deserialize(_stream);
    ret += m_entries.deserialize(_stream);
    ret += m_options.deserialize(_stream);

    if (ret < SD_MIN_PAYLOAD_SIZE) {
        std::cerr << __func__ << ": Failed to deserialize!\n";
        ret = 0;
    }

    return ret;
}

std::size_t SomeIpSd::serialize(std::ostream& _stream) const {
    std::size_t ret = 0;

    ret += serialization::PodPayload<SomeIpSdHeaderRaw_t>::serialize(_stream);
    ret += m_entries.serialize(_stream);
    ret += m_options.serialize(_stream);

    if (ret == 0) {
        std::cerr << __func__ << ": Failed to serialize!\n";
    }

    return ret;
}

types::sdFlags_t SomeIpSd::flags() const {
    return m_podPayload.flags;
}

bool SomeIpSd::reboot() const {
    return m_podPayload.flags & 0x80;
}

bool SomeIpSd::unicast() const {
    return m_podPayload.flags & 0x40;
}

bool SomeIpSd::controlFlag() const {
    return m_podPayload.flags & 0x20;
}

std::uint32_t SomeIpSd::reserved() const {
    std::uint32_t ret = static_cast<std::uint32_t>(m_podPayload.reserved[0] | (m_podPayload.reserved[1] >> 8) | (m_podPayload.reserved[2] >> 16));

    return ret;
}

SomeIpSdEntries& SomeIpSd::entries() {
    return m_entries;
}

SomeIpSdOptions& SomeIpSd::options() {
    return m_options;
}

void SomeIpSd::setFlags(const types::sdFlags_t& _flags) {
    m_podPayload.flags = _flags;
}

void SomeIpSd::setReboot(const bool _val) {
    if (_val) {
        m_podPayload.flags |= 0x80;
    } else {
        m_podPayload.flags &= 0x7F;
    }
}

void SomeIpSd::setUnicast(const bool _val) {
    if (_val) {
        m_podPayload.flags |= 0x40;
    } else {
        m_podPayload.flags &= 0xBF;
    }
}

void SomeIpSd::setControlFlag(const bool _val) {
    if (_val) {
        m_podPayload.flags |= 0x20;
    } else {
        m_podPayload.flags &= 0xDF;
    }
}

void SomeIpSd::setReserved(const std::array<std::uint8_t, 3> _val) {
    m_podPayload.reserved[0] = _val[0];
    m_podPayload.reserved[1] = _val[1];
    m_podPayload.reserved[2] = _val[2];
}

void SomeIpSd::print() const {
    // Store original format
    std::ios_base::fmtflags prevFmt(std::cout.flags());

    // Print payload
    std::cout << "[" __FILE__ "]"
        << "\n\tPOD size: " << serialization::PodPayload<SomeIpSdHeaderRaw_t>::size()
        << "\n\t--\n" << std::hex << std::showbase << "[HEADER]"
        << "\n\tflags: " << unsigned(flags())
        << "\n\treserved: " << unsigned(reserved()) << '\n';

    // Restore original format
    std::cout.flags(prevFmt);

    std::cout << "[ENTRIES]\n";
    m_entries.print();
    std::cout << "[OPTIONS]\n";
    m_options.print();

}

SomeIpSdBuilder& SomeIpSdBuilder::flags(const types::sdFlags_t _flags) {
    m_entry.setFlags(_flags);
    return *this;
}

SomeIpSdBuilder& SomeIpSdBuilder::reboot(const bool _val) {
    m_entry.setReboot(_val);
    return *this;
}

SomeIpSdBuilder& SomeIpSdBuilder::unicast(const bool _val) {
    m_entry.setUnicast(_val);
    return *this;
}

SomeIpSdBuilder& SomeIpSdBuilder::controlFlag(const bool _val) {
    m_entry.setControlFlag(_val);
    return *this;
}

SomeIpSdBuilder& SomeIpSdBuilder::pushEntry(SomeIpSdEntry& _entry) {
    m_entry.entries().pushEntry(_entry);
    return *this;
}

SomeIpSdBuilder& SomeIpSdBuilder::pushOptions(SomeIpSdOption& _entry) {
    m_entry.options().pushEntry(_entry);
    return *this;
}

SomeIpSdBuilder& SomeIpSdBuilder::pushEntry(SomeIpSdEntry&& _entry) {
    m_entry.entries().pushEntry(_entry);
    return *this;
}

SomeIpSdBuilder& SomeIpSdBuilder::pushOption(SomeIpSdOption&& _entry) {
    m_entry.options().pushEntry(_entry);
    return *this;
}

SomeIpSdBuilder::operator SomeIpSd() const {
    return std::move(m_entry);
}

} // namespace sd
} // namespace someip
} // namespace vsomeip_utilities
