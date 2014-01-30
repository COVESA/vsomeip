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

namespace vsomeip {

/// Representation of a unique Some/IP communication endpoint, consisting of
/// address, protocol, protocol version and port. To create an endpoint, call
/// vsomeip::factory::create_endpoint.
class endpoint {
public:
	virtual ~endpoint() {};

	///
	/// Get the address (e.g. IPv4 or IPv6) as string representation.
	///
	virtual ip_address get_address() const = 0;
	///
	/// Get the port.
	///
	virtual ip_port get_port() const = 0;
	virtual ip_protocol get_protocol() const = 0;
	virtual ip_version get_version() const = 0;

};

} // namespace vsomeip

#endif /* ENDPOINT_HPP_ */
