//
// constants_impl.hpp
//
// Date: 	Jan 13, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_CONSTANTS_IMPL_HPP
#define VSOMEIP_IMPL_CONSTANTS_IMPL_HPP

#include <vsomeip/primitive_types.hpp>

#define VSOMEIP_PROTOCOL_RESERVED 0x0

const uint8_t vsomeip_protocol_reserved_byte
	= VSOMEIP_PROTOCOL_RESERVED;

const uint16_t vsomeip_protocol_reserved_word
	= VSOMEIP_PROTOCOL_RESERVED;

const uint32_t vsomeip_protocol_reserved_long
	= VSOMEIP_PROTOCOL_RESERVED;

#define VSOMEIP_STATIC_HEADER_LENGTH	8

#endif // VSOMEIP_IMPL_CONSTANTS_IMPL_HPP
