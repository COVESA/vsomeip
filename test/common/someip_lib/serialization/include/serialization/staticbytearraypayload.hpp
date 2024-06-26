// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __STATICBYTEARRAYPAYLOAD_HPP__
#define __STATICBYTEARRAYPAYLOAD_HPP__

#include <cassert>
#include <vector>

#include <serialization/dynamicbytearraypayload.hpp>


namespace vsomeip_utilities {
namespace serialization {

template<std::size_t Size>
class StaticByteArrayPayloadBuilder;

template<std::size_t Size>
class StaticByteArrayPayload : public DynamicByteArrayPayload {
public:
    // CTor
    explicit StaticByteArrayPayload()
        : DynamicByteArrayPayload(Size) {}

    /**
     * @brief Create Fragment Builder
     *
     * @return StaticByteArrayPayloadBuilder
     */
    static StaticByteArrayPayloadBuilder<Size> create() {
        return StaticByteArrayPayloadBuilder<Size>();
    }

    /**
     * @brief The Serializable object definition size
     *
     * @return std::size_t
     */
    inline static std::size_t size() {
        return Size;
    }

    /**
     * @brief The Serializable object instance actual size
     *
     * @return std::size_t
     */
    std::size_t actualSize() const override {
        return size();
    }

    /**
     * @brief Deserialize data from stream
     *
     * @param _stream
     */
    std::size_t deserialize(std::istream& _stream) override {
        std::size_t deserializedBytes = 0;

        // Read backwards into vector to convert from Big to Little  Endian byte ordering
        for (std::size_t i = m_payload.size(); i > 0; --i) {
            // Read byte into vector and break if operation fails
            if (!_stream >> m_payload[i - 1]) {
                break;
            } else {
                deserializedBytes++;
            }
        }

        if (_stream.fail()) {
            std::cerr << __func__ << ": Failed to deserialize Byte Array!\n";
        }

        return deserializedBytes;
    }

    virtual void setPayload(std::initializer_list<std::uint8_t>& _payload) override {
        // Ensure initializer list size matches the payload size
        assert(_payload.size() == Size);

        DynamicByteArrayPayload::setPayload(_payload);
    }

    /**
     * @brief Print to stdout utility method
     *
     */
    virtual void print() const override {
        // Store original format
        std::ios_base::fmtflags prevFmt(std::cout.flags());

        // Print payload
        std::cout << "[" __FILE__ "]"
            << "\n\tSize: " << Size
            << "\n\t--\n\tpayload: "
            << std::hex
            << std::showbase
            << "{\n";

        for (auto byte : m_payload) {
            std::cout << unsigned(byte) << ' ';
        }

        std::cout << "}\n";

        // Restore original format
        std::cout.flags(prevFmt);
    }
};

/**
 * @brief Builder
 *
 */
template<std::size_t Size>
class StaticByteArrayPayloadBuilder {
public:
    // CTor/DTor
    StaticByteArrayPayloadBuilder() = default;
    virtual ~StaticByteArrayPayloadBuilder() = default;

    // Delete default CTors and assignment operators
    StaticByteArrayPayloadBuilder(const StaticByteArrayPayloadBuilder&) = delete;
    StaticByteArrayPayloadBuilder(const StaticByteArrayPayloadBuilder&&) = delete;
    const StaticByteArrayPayloadBuilder& operator=(const StaticByteArrayPayloadBuilder&) = delete;
    const StaticByteArrayPayloadBuilder& operator=(const StaticByteArrayPayloadBuilder&&) = delete;

    operator StaticByteArrayPayload<Size>() const {
        return std::move(m_entry);
    }

    // Setter Methods
    StaticByteArrayPayloadBuilder<Size>& payload(std::initializer_list<std::uint8_t>&& _payload) {
        m_entry.setPayload(_payload);
        return *this;
    }

private:
    StaticByteArrayPayload<Size> m_entry;
};

} // serialization
} // vsomeip_utilities

#endif // __STATICBYTEARRAYPAYLOAD_HPP__
