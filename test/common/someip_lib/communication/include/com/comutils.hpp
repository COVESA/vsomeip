// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __COMUTILS_HPP__
#define __COMUTILS_HPP__

#include <system_error>

#include <com/comcommon.hpp>

namespace vsomeip_utilities {
namespace com {
namespace utils {

/**
 * @brief Convert string into an asio::ip::address_v4
 *
 * @param _addr String address
 * @param _ec resulting error code
 * @return asio::ip::address_v4
 */
asio::ip::address_v4 ipv4AddressFromString(const std::string& _addr, std::error_code& _ec);

/**
 * @brief Convert string into an asio::ip::address_v6
 *
 * @param _addr String address
 * @param _ec resulting error code
 * @return asio::ip::address_v6
 */
asio::ip::address_v6 ipv6AddressFromString(const std::string& _addr, std::error_code& _ec);

/**
 * @brief Convert uint32_t into an asio::ip::address_v4
 *
 * @param _addr uint32_t address
 * @return asio::ip::address_v4
 */
[[nodiscard]] asio::ip::address_v4 ipv4AddressFromInt(std::uint32_t _add);

} // utils
} // com
} // vsomeip_utilities

#endif // __COMUTILS_HPP__
