//
// protocol.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <cstdint>

#include <vsomeip_internal/byteorder.hpp>

#ifndef VSOMEIP_INTERNAL_PROTOCOL_HPP
#define VSOMEIP_INTERNAL_PROTOCOL_HPP

#define VSOMEIP_PROTOCOL_OVERHEAD		  	 15
#define VSOMEIP_PROTOCOL_RESERVED		   	  0

#define VSOMEIP_PROTOCOL_ID				   	  4
#define VSOMEIP_PROTOCOL_COMMAND		   	  6
#define VSOMEIP_PROTOCOL_PAYLOAD_SIZE	   	  7
#define VSOMEIP_PROTOCOL_PAYLOAD		  	 11

#if __BYTE_ORDER == __LITTLE_ENDIAN
const uint32_t VSOMEIP_PROTOCOL_START_TAG = 0x076D3767;
const uint32_t VSOMEIP_PROTOCOL_END_TAG =   0x67376D07;
#else
const uint32_t VSOMEIP_PROTOCOL_START_TAG = 0x67376D07;
const uint32_t VSOMEIP_PROTOCOL_END_TAG	 =	 0x076D3767;
#endif

#endif // VSOMEIP_INTERNAL_PROTOCOL_HPP
