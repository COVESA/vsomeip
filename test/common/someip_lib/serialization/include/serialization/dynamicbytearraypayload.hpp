// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __DYNAMICBYTEARRAYPAYLOAD_HPP__
#define __DYNAMICBYTEARRAYPAYLOAD_HPP__

#include <vector>

#include <serialization/serializable.hpp>

namespace vsomeip_utilities {
namespace serialization {

class DynamicByteArrayPayloadBuilder;

class DynamicByteArrayPayload : public Serializable {
public:
    // CTor
    explicit DynamicByteArrayPayload(const std::size_t _size = 0) : m_payload(_size, 0) { }

    // Copy/Move CTors
    DynamicByteArrayPayload(const DynamicByteArrayPayload &_other);
    DynamicByteArrayPayload(const DynamicByteArrayPayload &&_other);

    // Copy/Move assignment operators
    virtual const DynamicByteArrayPayload &operator=(const DynamicByteArrayPayload &_other);
    virtual const DynamicByteArrayPayload &operator=(const DynamicByteArrayPayload &&_other);

    /**
     * @brief Create Fragment Builder
     *
     * @return DynamicByteArrayPayload
     */
    static DynamicByteArrayPayloadBuilder create();

    /**
     * @brief The Serializable object definition size
     *
     * @return std::size_t
     */
    inline static std::size_t size() { return 0; }

    /**
     * @brief The Serializable object instance actual size
     *
     * @return std::size_t
     */
    std::size_t actualSize() const override;

    /**
     * @brief Deserialize data from stream
     *
     * @param _stream
     */
    virtual std::size_t deserialize(std::istream &_stream) override;

    /**
     * @brief Serialize data to stream
     *
     * @param _stream
     */
    virtual std::size_t serialize(std::ostream &_stream) const override;

    virtual void setPayload(std::initializer_list<std::uint8_t> &_payload);
    virtual const std::vector<std::uint8_t> &payload() const;

    /**
     * @brief Print to stdout utility method
     *
     */
    virtual void print() const override;

    bool operator==(const DynamicByteArrayPayload &_other) const;

protected:
    std::vector<std::uint8_t> m_payload;
};

/**
 * @brief Builder
 *
 */
class DynamicByteArrayPayloadBuilder {
public:
    // CTor/DTor
    DynamicByteArrayPayloadBuilder() = default;
    virtual ~DynamicByteArrayPayloadBuilder() = default;

    operator DynamicByteArrayPayload() const;

    // Setter Methods
    DynamicByteArrayPayloadBuilder &payload(std::initializer_list<std::uint8_t> &&_payload);

private:
    DynamicByteArrayPayload m_entry;
};

} // serialization
} // vsomeip_utilities

#endif // __DYNAMICBYTEARRAYPAYLOAD_HPP__
