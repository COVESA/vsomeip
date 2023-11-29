// Copyright (C) 2014-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_BYTEORDER_HPP
#define VSOMEIP_V3_BYTEORDER_HPP

#if defined(__linux__)
#include <endian.h>
#elif defined(__freebsd__)
#include <sys/endian.h>
#else
// TEST IF THERE COULD BE AN ERROR!
//#error "Undefined OS (only Linux/FreeBSD are currently supported)"
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define VSOMEIP_BYTES_TO_WORD(x0, x1) (uint16_t((x0) << 8 | (x1)))
#define VSOMEIP_BYTES_TO_LONG(x0, x1, x2, x3) (uint32_t((x0) << 24 | (x1) << 16 | (x2) << 8 | (x3)))
#define VSOMEIP_BYTES_TO_LONG_LONG(x0, x1, x2, x3, x4, x5, x6, x7) (uint64_t (x0) << 56 | uint64_t (x1) << 48 | uint64_t (x2) << 40 | uint64_t (x3) << 32 | uint64_t (x4) << 24 | uint64_t (x5) << 16 | uint64_t (x6) << 8 | uint64_t (x7))

#define VSOMEIP_WORDS_TO_LONG(x0, x1) (uint32_t((x0) << 16 | (x1)))

#define VSOMEIP_WORD_BYTE0(x) (uint8_t((x) & 0xFF))
#define VSOMEIP_WORD_BYTE1(x) (uint8_t((x) >> 8))

#define VSOMEIP_LONG_BYTE0(x) (uint8_t((x) & 0xFF))
#define VSOMEIP_LONG_BYTE1(x) (uint8_t(((x) >> 8) & 0xFF))
#define VSOMEIP_LONG_BYTE2(x) (uint8_t(((x) >> 16) & 0xFF))
#define VSOMEIP_LONG_BYTE3(x) (uint8_t(((x) >> 24) & 0xFF))

#define VSOMEIP_LONG_LONG_BYTE0(x) (uint8_t((x) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE1(x) (uint8_t(((x) >> 8) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE2(x) (uint8_t(((x) >> 16) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE3(x) (uint8_t(((x) >> 24) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE4(x) (uint8_t(((x) >> 32) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE5(x) (uint8_t(((x) >> 40) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE6(x) (uint8_t(((x) >> 48) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE7(x) (uint8_t(((x) >> 56) & 0xFF))

#define VSOMEIP_LONG_WORD0(x) (uint16_t((x) & 0xFFFF))
#define VSOMEIP_LONG_WORD1(x) (uint16_t(((x) >> 16) & 0xFFFF))

#elif __BYTE_ORDER == __BIG_ENDIAN

#define VSOMEIP_BYTES_TO_WORD(x0, x1) (uint16_t((x1) << 8 | (x0)))
#define VSOMEIP_BYTES_TO_LONG(x0, x1, x2, x3) (uint32_t((x3) << 24 | (x2) << 16 | (x1) << 8 | (x0)))
#define VSOMEIP_BYTES_TO_LONG_LONG(x0, x1, x2, x3, x4, x5, x6, x7) (uint64_t((x7) << 56 | (x6) << 48 | (x5) << 40 | (x4) << 32 | (x3) << 24 | (x2) << 16 | (x1) << 8 | (x0)))

#define VSOMEIP_WORD_BYTE0(x) (uint8_t((x) >> 8))
#define VSOMEIP_WORD_BYTE1(x) (uint8_t((x) & 0xFF))

#define VSOMEIP_LONG_BYTE0(x) (uint8_t(((x) >> 24) & 0xFF))
#define VSOMEIP_LONG_BYTE1(x) (uint8_t(((x) >> 16) & 0xFF))
#define VSOMEIP_LONG_BYTE2(x) (uint8_t(((x) >> 8) & 0xFF))
#define VSOMEIP_LONG_BYTE3(x) (uint8_t((x) & 0xFF))

#define VSOMEIP_LONG_LONG_BYTE0(x) (uint8_t(((x) >> 56) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE1(x) (uint8_t(((x) >> 48) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE2(x) (uint8_t(((x) >> 40) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE3(x) (uint8_t(((x) >> 32) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE4(x) (uint8_t(((x) >> 24) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE5(x) (uint8_t(((x) >> 16) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE6(x) (uint8_t(((x) >> 8) & 0xFF))
#define VSOMEIP_LONG_LONG_BYTE7(x) (uint8_t((x) & 0xFF))

#define VSOMEIP_LONG_WORD0(x) (uint16_t((((x) >> 16) & 0xFFFF))
#define VSOMEIP_LONG_WORD1(x) (uint16_t(((x) & 0xFFFF))

#else

#error "__BYTE_ORDER is not defined!"

#endif

#endif // VSOMEIP_V3_BYTEORDER_HPP_
