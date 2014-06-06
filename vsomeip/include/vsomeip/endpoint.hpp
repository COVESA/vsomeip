//
// endpoint.hpp
//
// Author: Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_ENDPOINT_HPP
#define VSOMEIP_ENDPOINT_HPP

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/serializable.hpp>

namespace vsomeip {

class endpoint
		: public serializable {
public:
	virtual ~endpoint() {};

	virtual ip_address get_address() const = 0;
	virtual ip_port get_port() const = 0;
	virtual ip_protocol get_protocol() const = 0;
	virtual ip_protocol_version get_version() const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_ENDPOINT_HPP
