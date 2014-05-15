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

#include <functional>

namespace vsomeip {

class message_base;

typedef uint32_t message_handler_id_t;
typedef std::function< void (const message_base *) > message_handler_t;

} // namespace vsomeip

#endif // VSOMEIP_TYPES_HPP
