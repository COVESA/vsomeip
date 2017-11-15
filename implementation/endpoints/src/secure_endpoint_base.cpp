// Copyright (c) 2017 by Vector Informatik GmbH
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/secure_endpoint_base.hpp"

#include <mbedtls/ssl_ciphersuites.h>
#include <vsomeip/defines.hpp>

namespace vsomeip {

secure_endpoint_base::secure_endpoint_base(bool _confidential, const std::vector<std::uint8_t> &_psk, const std::string &_pskid)
    : cipher_suite_({ ((_confidential) ? MBEDTLS_TLS_PSK_WITH_AES_128_CBC_SHA256 : MBEDTLS_TLS_PSK_WITH_NULL_SHA256), 0 }),
      psk_(_psk),
      pskid_(_pskid) {
}

secure_endpoint_base::~secure_endpoint_base() {
}

std::uint32_t secure_endpoint_base::get_max_dtls_payload_size() {
    constexpr std::uint32_t max_header_size = 13;
    constexpr std::uint32_t sha256_block_size = 64;
    constexpr std::uint32_t iv_block_size = 16;

    auto result = VSOMEIP_MAX_UDP_MESSAGE_SIZE - max_header_size - sha256_block_size;
    if (cipher_suite_[0] == MBEDTLS_TLS_PSK_WITH_AES_128_CBC_SHA256) {
        result -= iv_block_size;
    }

    return result;
}

} // namespace vsomeip
