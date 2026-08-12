// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vector>

#include <vsomeip/export.hpp>
#include <vsomeip/primitive_types.hpp>

#include <vsomeip/internal/deserializable.hpp>
#include <vsomeip/internal/serializable.hpp>

namespace vsomeip_v3 {

/**
 *
 * \defgroup vsomeip
 *
 * @{
 *
 */

/**
 *
 * \brief This class implements an array of bytes to be used as
 * payload for SOME/IP messages.
 *
 */
class payload : public serializable, public deserializable {
public:
    virtual ~payload() { }

    /**
     * \brief Returns true if the given payload is equal to this one.
     *
     * \param _other Payload that shall be compared to this payload.
     */
    virtual bool operator==(const payload& _other) const = 0;

    /**
     * \brief Returns pointer to the payload content
     */
    virtual byte_t* get_data() = 0;

    /**
     * \brief Returns constant pointer to the payload content
     */
    virtual const byte_t* get_data() const = 0;

    /**
     * \brief Copies the given data array to the payload object.
     *
     * The current payload content is replaced by the data provided.
     * The given buffer remains untouched.
     *
     * \param _data Pointer to a data buffer.
     * \param _length Length of the data buffer.
     */
    virtual void set_data(const byte_t* _data, length_t _length) = 0;

    /**
     * \brief Copies the given data array to the payload object.
     *
     * The current payload content is replaced by the data provided.
     * The given buffer remains untouched.
     *
     * \param _data Vector containing the data
     */
    virtual void set_data(const std::vector<byte_t>& _data) = 0;

    /**
     * \brief Returns the length of the payload content.
     */
    virtual length_t get_length() const = 0;

    /**
     * \brief Set the maximum length of the payload content.
     *
     * This function must be called before directly copying data using the
     * pointer to the internal buffer.
     */
    virtual void set_capacity(length_t _length) = 0;

    /**
     * \brief Moves the given data array to the payload object.
     *
     * The current payload content is replaced by the data provided.
     * The given buffer is owned by the payload object afterwards.
     *
     * \param _data Vector containing the data
     */
    virtual void set_data(std::vector<byte_t>&& _data) = 0;
};

/** @} */

} // namespace vsomeip_v3
