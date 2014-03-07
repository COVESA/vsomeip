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

	/// Delivers the address (e.g. IPv4 or IPv6) as string representation.
	/// \returns the address
	virtual ip_address get_address() const = 0;
	/// Delivers the port represented by the endpoint.
	/// \returns the port
	virtual ip_port get_port() const = 0;
	/// Delivers the transport protocol used by the endpoint.
	/// \returns protocol (UDP or TCP) used to communicate through this
	/// endpoint
	virtual transport_protocol get_protocol() const = 0;
	/// Delivers the transport protocol version used by the endpoint.
	/// \returns protocol version (V4 or V6) used to communicate through this
	/// endpoint
	virtual transport_protocol_version get_version() const = 0;
};

} // namespace vsomeip

#endif /* ENDPOINT_HPP_ */
