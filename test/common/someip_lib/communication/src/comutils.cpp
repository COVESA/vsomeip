// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <com/comutils.hpp>

#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>

#include <utils/utils.hpp>

namespace vsomeip_utilities {
namespace com {
namespace utils {

asio::ip::address_v4 ipv4AddressFromString(const std::string& _addr, std::error_code& _ec) {
    // Build address from string
    boost::system::error_code ec;
    asio::ip::address_v4 addr = asio::ip::make_address_v4(_addr, ec);

    // convert resulting error code to std::error_code
    _ec = vsomeip_utilities::utils::intToStdError(ec.value());

    return addr;
}

asio::ip::address_v6 ipv6AddressFromString(const std::string& _addr, std::error_code& _ec) {
    // Build address from string
    boost::system::error_code ec;
    asio::ip::address_v6 addr = asio::ip::make_address_v6(_addr, ec);

    // convert resulting error code to std::error_code
    _ec = vsomeip_utilities::utils::intToStdError(ec.value());

    return addr;
}

asio::ip::address_v4 ipv4AddressFromInt(std::uint32_t _addr) {
    // Build address from uint
    return asio::ip::make_address_v4(_addr);
}

} // utils
} // com
} // vsomeip_utilities
