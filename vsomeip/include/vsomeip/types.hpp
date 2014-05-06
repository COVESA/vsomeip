//
// types.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_TYPES_HPP
#define VSOMEIP_TYPES_HPP

namespace vsomeip {

class message_base;

typedef void (*message_handler_t)(const message_base *);

} // namespace vsomeip

#endif // VSOMEIP_TYPES_HPP
