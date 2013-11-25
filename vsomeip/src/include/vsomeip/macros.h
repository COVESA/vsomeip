//
// macros.h
//
// Date: 	Oct 30, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_MACROS_H__
#define __VSOMEIP_MACROS_H__

#include <endian.h>

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define VSOMEIP_BYTES_TO_WORD(x0, x1) ((x0) << 8 | (x1))
#define VSOMEIP_BYTES_TO_LONG(x0, x1, x2, x3) ((x0) << 24 | (x1) << 16 | (x2) << 8 | (x3))

#define VSOMEIP_WORDS_TO_LONG(x0, x1) ((x0) << 16 | (x1))

#define VSOMEIP_WORD_BYTE0(x) ((x) & 0xFF)
#define VSOMEIP_WORD_BYTE1(x) ((x) >> 8)

#define VSOMEIP_LONG_BYTE0(x) ((x) & 0xFF)
#define VSOMEIP_LONG_BYTE1(x) ((x) >> 8)
#define VSOMEIP_LONG_BYTE2(x) ((x) >> 16)
#define VSOMEIP_LONG_BYTE3(x) ((x) >> 24)

#define VSOMEIP_LONG_WORD0(x) ((x) & 0xFFFF)
#define VSOMEIP_LONG_WORD1(x) ((x) >> 16)

#elif __BYTE_ORDER == __BIG_ENDIAN

#define VSOMEIP_BYTES_TO_WORD(x0, x1) ((x1) << 8 | (x0))
#define VSOMEIP_BYTES_TO_LONG(x0, x1, x2, x3) ((x3) << 24 | (x2) << 16 | (x1) << 8 | (x0))

#define VSOMEIP_WORD_BYTE0(x) ((x) >> 8)
#define VSOMEIP_WORD_BYTE1(x) ((x) & 0xFF)

#define VSOMEIP_LONG_BYTE0(x) ((x) >> 24)
#define VSOMEIP_LONG_BYTE1(x) ((x) >> 16)
#define VSOMEIP_LONG_BYTE2(x) ((x) >> 8)
#define VSOMEIP_LONG_BYTE3(x) ((x) & 0xFF)

#define VSOMEIP_LONG_WORD0(x) ((x) >> 16)
#define VSOMEIP_LONG_WORD1(x) ((x) & 0xFFFF)

#else

#error "__BYTE_ORDER is not defined!"

#endif

#endif // __VSOMEIP_MACROS_H__
