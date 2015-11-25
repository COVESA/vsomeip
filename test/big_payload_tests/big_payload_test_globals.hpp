// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef BIG_PAYLOAD_TEST_GLOBALS_HPP_
#define BIG_PAYLOAD_TEST_GLOBALS_HPP_

#include <cstdint>

namespace big_payload_test {
    constexpr std::uint32_t BIG_PAYLOAD_SIZE = 1024*600;
    constexpr vsomeip::byte_t DATA_SERVICE_TO_CLIENT = 0xAA;
    constexpr vsomeip::byte_t DATA_CLIENT_TO_SERVICE = 0xFF;
}

#endif /* BIG_PAYLOAD_TEST_GLOBALS_HPP_ */
