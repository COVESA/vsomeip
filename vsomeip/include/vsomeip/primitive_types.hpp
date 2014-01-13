//
// primitive_types.hpp
//
// Date: 	Nov 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef PRIMITIVE_TYPES_HPP
#define PRIMITIVE_TYPES_HPP

#include <cstdint>

namespace vsomeip {

typedef uint32_t message_id;
typedef uint16_t service_id;
typedef uint16_t method_id;

typedef uint32_t length;

typedef uint32_t request_id;
typedef uint16_t client_id;
typedef uint16_t session_id;

typedef uint8_t protocol_version;
typedef uint8_t interface_version;

} // namespace vsomeip

#endif // PRIMITIVE_TYPES_HPP
