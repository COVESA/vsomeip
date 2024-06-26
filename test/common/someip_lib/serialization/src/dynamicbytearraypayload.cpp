// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <serialization/dynamicbytearraypayload.hpp>

namespace vsomeip_utilities {
namespace serialization {

DynamicByteArrayPayload::DynamicByteArrayPayload(const DynamicByteArrayPayload &_other)
    : m_payload { _other.m_payload } {
}

DynamicByteArrayPayload::DynamicByteArrayPayload(const DynamicByteArrayPayload &&_other)
    : m_payload { std::move(_other.m_payload) } {
}

const DynamicByteArrayPayload &
DynamicByteArrayPayload::operator=(const DynamicByteArrayPayload &_other) {
    m_payload = _other.m_payload;
    return *this;
}

const DynamicByteArrayPayload &
DynamicByteArrayPayload::operator=(const DynamicByteArrayPayload &&_other) {
    m_payload = std::move(_other.m_payload);
    return *this;
}

DynamicByteArrayPayloadBuilder DynamicByteArrayPayload::create() {
    return DynamicByteArrayPayloadBuilder();
}

std::size_t DynamicByteArrayPayload::actualSize() const {
    return m_payload.size();
}

std::size_t DynamicByteArrayPayload::deserialize(std::istream &_stream) {
    std::size_t deserializedBytes = 0;

    // We want to read everything available in the stream
    while (!_stream.fail()) {
        char byte;
        if (_stream.read(&byte, sizeof(byte))) {
            m_payload.emplace(m_payload.begin(), static_cast<std::uint8_t>(byte));
            deserializedBytes++;
        }
    }

    return deserializedBytes;
}

std::size_t DynamicByteArrayPayload::serialize(std::ostream &_stream) const {
    std::size_t serializedBytes = 0;

    // Write into stream backwards to convert from Little to Big Endian byte ordering
    for (std::size_t i = m_payload.size(); i > 0; --i) {
        // write byte into stream and break if operation fails
        if (!(_stream.write(reinterpret_cast<const char *>(&m_payload[i - 1]), sizeof(std::uint8_t)))) {
            break;
        } else {
            serializedBytes++;
        }
    }

    if (_stream.fail()) {
        std::cerr << __func__ << ": Failed to serialize Byte Array!\n";
    }

    return serializedBytes;
}

void DynamicByteArrayPayload::setPayload(std::initializer_list<std::uint8_t> &_payload) {
    // Create new vector with provided payload
    m_payload = std::move(std::vector<std::uint8_t>(_payload));
}

const std::vector<std::uint8_t> &DynamicByteArrayPayload::payload() const {
    return m_payload;
}

void DynamicByteArrayPayload::print() const {
    // Store original format
    std::ios_base::fmtflags prevFmt(std::cout.flags());

    // Print payload
    std::cout << "[" __FILE__ "]"
              << "\n\tSize: " << actualSize()
              << "\n\t--\n\tpayload:" << std::hex << std::showbase;

    for (auto byte : m_payload) {
        std::cout << ' ' << unsigned(byte);
    }

    std::cout << '\n';

    // Restore original format
    std::cout.flags(prevFmt);
}

bool DynamicByteArrayPayload::operator==(const DynamicByteArrayPayload &_other) const {
    return m_payload == _other.payload();
}

DynamicByteArrayPayloadBuilder::operator DynamicByteArrayPayload() const {
    return std::move(m_entry);
}

DynamicByteArrayPayloadBuilder &
DynamicByteArrayPayloadBuilder::payload(std::initializer_list<std::uint8_t> &&_payload) {
    m_entry.setPayload(_payload);
    return *this;
}

} // namespace serialization
} // namespace vsomeip_utilities
