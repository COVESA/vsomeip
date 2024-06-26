// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <bitset>
#include <iomanip>
#include <iostream>
#include <netinet/ether.h>

#include <boost/asio/ip/address.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include <utils/utils.hpp>

namespace vsomeip_utilities {
namespace utils {

std::error_code intToStdError(int _errc) {
    return std::make_error_code(static_cast<std::errc>(_errc));
}

std::error_code intToStdError(std::errc _errc) {
    return intToStdError(static_cast<int>(_errc));
}

void printBinPayload(const char *_data, std::size_t _size) {
    std::cout << "[PAYLOAD]\n";
    for (std::size_t i = 0; i < _size; ++i) {
        std::cout << 'b' << std::bitset<8>(_data[i]) << ' ';
    }
    std::cout << "\n[!PAYLOAD]\n";
}

void printHexPayload(const char *_data, std::size_t _size) {
    // Store original format
    std::ios_base::fmtflags prevFmt(std::cout.flags());

    // Print payload
    std::cout << "[PAYLOAD]\n";
    // Set hex format
    for (std::size_t i = 0; i < _size; i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << int(_data[i] & 0xFF) << ' ';
    }
    std::cout << "\n[!PAYLOAD]\n";

    // Restore original format
    std::cout.flags(prevFmt);
}

void encodeHardwareAddress(const std::string _address, std::uint8_t *_array) {
    ether_addr *hardwareAddress = ether_aton(_address.c_str());
    for (auto t : boost::adaptors::reverse(hardwareAddress->ether_addr_octet)) {
        *_array = t;
        _array++;
    }
}

std::string decodeHardwareAddress(const std::uint8_t *_array) {
    auto hardwareAddress = std::make_shared<ether_addr>();
    for (auto &t : boost::adaptors::reverse(hardwareAddress->ether_addr_octet)) {
        t = *_array;
        _array++;
    }
    return ether_ntoa(hardwareAddress.get());
}

void encodeIpAddress(const std::string _address, std::uint8_t *_array) {
    auto ipAddress = boost::asio::ip::make_address_v4(_address);
    for (auto t : boost::adaptors::reverse(ipAddress.to_bytes())) {
        *_array = t;
        _array++;
    }
}

std::string decodeIpAddress(const std::uint8_t *_array) {
    boost::asio::ip::address_v4::bytes_type addressBytes;
    for (auto &t : boost::adaptors::reverse(addressBytes)) {
        t = *_array;
        _array++;
    }
    auto ipAddress = boost::asio::ip::make_address_v4(addressBytes);
    return ipAddress.to_string();
}

} // utils
} // vsomeip_utilities
