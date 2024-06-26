// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __UTILS_H__
#define __UTILS_H__

#include <system_error>

namespace vsomeip_utilities {
namespace utils {

// Empty string constant
constexpr char EMPTY_STRING[] = "";

enum class metadataCode_e : std::uint8_t {
    E_SERVICE_DISCOVERY = 0x00,
    E_PAYLOAD_SERIALIZATION = 0x01,
    E_PROTOCOL = 0x02
};

/**
 * @brief std::error_code convertion utility method
 *
 * @param _errc Int error
 * @return std::error_code
 */
std::error_code intToStdError(int _errc);

/**
 * @brief std::error_code convertion utility method
 *
 * @param _errc std::errc error
 * @return std::error_code
 */
std::error_code intToStdError(std::errc _errc);

/**
 * @brief Swap _x endianess
 *
 * @tparam T
 * @param _x
 * @return T
 */
template<typename T>
T swapEndianness(const T &_x) {
    T ret;

    char *dst = reinterpret_cast<char *>(&ret);
    const char *src = reinterpret_cast<const char *>(&_x);

    // Swap byte ordering
    for (std::size_t i = sizeof(T); i > 0; --i) {
        *dst++ = src[i - 1];
    }

    return ret;
}

void printBinPayload(const char *_data, std::size_t _size);
void printHexPayload(const char *_data, std::size_t _size);

/** Store an hardware address in the array. */
void encodeHardwareAddress(const std::string _address, std::uint8_t *_array);

/** Retrieve an hardware address from the array. */
std::string decodeHardwareAddress(const std::uint8_t *_array);

/** Store a protocol address in the array. */
void encodeIpAddress(const std::string _address, std::uint8_t *_array);

/** Retrieve a protocol address from the array. */
std::string decodeIpAddress(const std::uint8_t *_array);

} // utils
} // vsomeip_utilities

#endif // __UTILS_H__
