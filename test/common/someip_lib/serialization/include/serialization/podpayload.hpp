// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __PODPAYLOAD_HPP__
#define __PODPAYLOAD_HPP__

#include <serialization/serializable.hpp>
#include <utils/utils.hpp>

namespace vsomeip_utilities {
namespace serialization {

/**
 * @brief Plain old Data Payload representation
 *
 * @param PayloadType
 */
template <typename PayloadType>
class PodPayload
    : public serialization::Serializable {
public:
    // CTor
    PodPayload() = default;

    // Copy/Move CTors
    PodPayload(const PodPayload& _other) : m_podPayload { _other.m_podPayload } {}
    PodPayload(const PodPayload&& _other) : m_podPayload { std::move(_other.m_podPayload) } {}

    // Copy/Move assignment operators
    virtual const PodPayload& operator=(const PodPayload& _other) {
        m_podPayload = _other.m_podPayload;
        return *this;
    }

    virtual const PodPayload& operator=(const PodPayload&& _other) {
        m_podPayload = std::move(_other.m_podPayload);
        return *this;
    }

    /**
     * @brief The Serializable object definition size
     *
     * @return std::size_t
     */
    static std::size_t size() {
        return sizeof(PayloadType);
    }

    /**
     * @brief The Serializable object instance actual size
     *
     * @return std::size_t
     */
    virtual std::size_t actualSize() const override {
        return size();
    }

    /**
     * @brief Deserialize data from stream
     *
     * @param _stream
     */
    virtual std::size_t deserialize(std::istream& _stream) {
        PayloadType bigEndianPayload;
        _stream.read(reinterpret_cast<char*>(&bigEndianPayload), sizeof(PayloadType));
        m_podPayload = utils::swapEndianness(bigEndianPayload);

        if (_stream.fail()) {
            std::cerr << __func__ << ": Failed to deserialize POD!\n";
            return 0;
        } else {
            return size();
        }
    }

    /**
     * @brief Serialize data to stream
     *
     * @param _stream
     */
    virtual std::size_t serialize(std::ostream& _stream) const {

        PayloadType bigEndianPayload = utils::swapEndianness(m_podPayload);
        _stream.write(reinterpret_cast<const char*>(&bigEndianPayload), sizeof(PayloadType));


        if (_stream.fail()) {
            std::cerr << __func__ << ": Failed to serialize POD!\n";
            return 0;
        } else {
            return size();
        }
    }

protected:
    PayloadType m_podPayload;
};

} // namespace serialization
} // namespace vsomeip_utilities

#endif // __PODPAYLOAD_HPP__
