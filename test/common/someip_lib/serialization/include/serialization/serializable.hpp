// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __SERIALIZABLE_HPP__
#define __SERIALIZABLE_HPP__

#include <iostream>

namespace vsomeip_utilities {
namespace serialization {

/**
 * @brief Abstract Serializable class
 *
 */
class Serializable {
public:

    /**
     * @brief The Serializable object definition size
     *
     * @return std::size_t
     */
    static std::size_t size() { return sizeof(Serializable); };

    /**
     * @brief The Serializable object instance actual size
     *
     * @return std::size_t
     */
    virtual std::size_t actualSize() const = 0;

    /**
     * @brief Deserialize data from stream
     *
     * @param _stream
     */
    virtual std::size_t deserialize(std::istream& _stream) = 0;

    /**
     * @brief Serialize data to stream
     *
     * @param _stream
     */
    virtual std::size_t serialize(std::ostream& _stream) const = 0;

    /**
     * @brief Print to stdout utility method
     *
     */
    virtual void print() const = 0;
};

} // com
} // vsomeip_utilities

#endif // __SERIALIZABLE_HPP__
