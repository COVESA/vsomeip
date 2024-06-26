// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __FRAGMENTPAYLOAD_HPP__
#define __FRAGMENTPAYLOAD_HPP__

#include <iostream>

#include <serialization/podpayload.hpp>
#include <serialization/streampayload.hpp>

namespace vsomeip_utilities {
namespace serialization {

template<typename HeaderPodT>
class FragmentPayload
    : public serialization::PodPayload<HeaderPodT>
    , public serialization::StreamPayload {
public:
    // CTor
    explicit FragmentPayload() = default;

    // Copy/Move CTors
    FragmentPayload(const FragmentPayload& _other)
        : serialization::PodPayload<HeaderPodT> { _other }
        , serialization::StreamPayload { _other } {}

    FragmentPayload(const FragmentPayload&& _other)
        : serialization::PodPayload<HeaderPodT> { _other }
        , serialization::StreamPayload { _other } {}

    // Copy/Move assignment operators
    const FragmentPayload& operator=(const FragmentPayload& _other) {
        serialization::PodPayload<HeaderPodT>::operator=(_other);
        serialization::StreamPayload::operator=(_other);

        return *this;
    }

    const FragmentPayload& operator=(const FragmentPayload&& _other) {
        serialization::PodPayload<HeaderPodT>::operator=(_other);
        serialization::StreamPayload::operator=(_other);

        return *this;
    }

    /**
     * @brief The Serializable object definition size
     *
     * @return std::size_t
     */
    static std::size_t size() {
        return serialization::PodPayload<HeaderPodT>::size();
    }

    /**
     * @brief The Serializable object instance actual size
     *
     * @return std::size_t
     */
    virtual std::size_t actualSize() const override {
        return size() + serialization::StreamPayload::streamSize();
    }

    /**
     * @brief Returns the payload defined size (as referenced in the header)
     *
     * @return std::size_t
     */
    virtual std::size_t payloadSize() const = 0;

    /**
     * @brief Deserialize data from stream
     *
     * @param _stream
     */
    virtual std::size_t deserialize(std::istream& _stream) override {
        std::size_t headerBytes = 0;
        std::size_t payloadBytes = 0;

        headerBytes = serialization::PodPayload<HeaderPodT>::deserialize(_stream);

        if (headerBytes == serialization::PodPayload<HeaderPodT>::size()) {
            payloadBytes = serialization::StreamPayload::readFromStream(_stream, payloadSize());

            if (payloadBytes != payloadSize()) {
                std::cerr << __func__
                    << ": Error deserializing Payload, deserialized length mismatches header length!\n";
            }

        } else {
            std::cerr << __func__
                    << ": Error deserializing Header, deserialized length mismatches header size!\n";
        }

        return headerBytes + payloadBytes;
    }

    /**
     * @brief Serialize data to stream
     *
     * @param _stream
     */
    virtual std::size_t serialize(std::ostream& _stream) const override {
        std::size_t headerBytes = 0;
        std::size_t payloadBytes = 0;

        headerBytes += serialization::PodPayload<HeaderPodT>::serialize(_stream);
        if (headerBytes == serialization::PodPayload<HeaderPodT>::size()) {
            payloadBytes = serialization::StreamPayload::writeToStream(_stream);

            if (payloadBytes != payloadSize()) {
                std::cerr << __func__
                    << ": Error serializing Payload, serialized length mismatches header length!\n";
            }

        } else {
            std::cerr << __func__
                    << ": Error serializing Header, serialized length mismatches header size!\n";
        }


        return headerBytes + payloadBytes;
    }
};

} // serialization
} // vsomeip_utilities

#endif // __FRAGMENTPAYLOAD_HPP__
