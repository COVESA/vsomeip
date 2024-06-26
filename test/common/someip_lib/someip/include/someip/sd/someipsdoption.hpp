// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __SOMEIPSDOPTION_HPP__
#define __SOMEIPSDOPTION_HPP__

#include <someip/types/primitives.hpp>

#include <serialization/fragmentpayload.hpp>

namespace vsomeip_utilities {
namespace someip {
namespace sd {

class SomeIpSdOptionBuilder;

// Inverted field representation to align with Big-Endian byte order
struct SdOptionHeaderRaw_t {
    std::uint8_t reserved = { 0 };
    types::sdOptionType type = { 0 };
    types::sdOptionLength length = { 0 };
};

/**
 * @brief Returns the actual SomeIP length to be placed in the header
 * (disregard last 8 header bytes since they are retrieved together)
 *
 * @return constexpr std::size_t
 */
constexpr std::size_t calcInitialSdOptionHeaderLengthAttr() {
    return sizeof(std::uint8_t);
}

class SomeIpSdOption
    : public serialization::FragmentPayload<SdOptionHeaderRaw_t> {
public:
    // CTor
    SomeIpSdOption();

    /**
     * @brief Returns the payload defined size (as referenced in the header)
     *
     * @return std::size_t
     */
    std::size_t payloadSize() const override;

    /**
     * @brief Create Builder
     *
     * @return SomeIpSdEntryBuilder
     */
    static SomeIpSdOptionBuilder create();

    // Attribute Getters
    types::sdOptionType type() const;
    types::sdOptionLength length() const;
    std::uint8_t reserved() const;

    // Message Attribute Setters
    void setType(const types::sdOptionType& _val);

    template <typename T>
    std::size_t push(const T& _payload) {
        // Push payload and update POD length attribute
        std::size_t processed = serialization::FragmentPayload<SdOptionHeaderRaw_t>::push(_payload);
        m_podPayload.length += static_cast<std::uint16_t>(processed);

        return processed;
    }

    template <typename T>
    std::size_t push(const T&& _payload) {
        auto p(_payload);
        // Push payload and update POD length attribute
        std::size_t processed = serialization::FragmentPayload<SdOptionHeaderRaw_t>::push(p);
        m_podPayload.length += static_cast<std::uint16_t>(processed);

        return processed;
    }

    /**
     * @brief Forces the length of the option to a given value.
     * NOTE: This should only be used to test MALFORMED messages!
     *
     * @param _length
     */
    void forcePayloadLength(const types::sdOptionLength &_length) {
        m_podPayload.length = _length;
    }

    /**
     * @brief Print to stdout utility method
     *
     */
    void print() const override;
};

/**
 * @brief Entry builder
 *
 */
class SomeIpSdOptionBuilder
{
public:
    // CTor/DTor
    SomeIpSdOptionBuilder() = default;
    virtual ~SomeIpSdOptionBuilder() = default;

    operator SomeIpSdOption() const;

    // Setter Methods
    SomeIpSdOptionBuilder& type(const types::sdOptionType& _val);

    template <typename T>
    SomeIpSdOptionBuilder& push(T& _payload) {
        m_entry.push(_payload);
        return *this;
    }

    template <typename T>
    SomeIpSdOptionBuilder& push(T&& _payload) {
        m_entry.push(_payload);
        return *this;
    }


private:
    SomeIpSdOption m_entry;
};

} // sd
} // someip
} // vsomeip_utilities

#endif // __SOMEIPSDOPTION_HPP__
