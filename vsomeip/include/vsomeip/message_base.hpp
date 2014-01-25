//
// message_base.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_MESSAGE_BASE_HPP
#define VSOMEIP_MESSAGE_BASE_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/serializable.hpp>

namespace vsomeip {

class endpoint;

class message_base : virtual public serializable {
public:
	virtual ~message_base() {};

	virtual endpoint * get_endpoint() const = 0;
	virtual void set_endpoint(endpoint *_endpoint) = 0;

	virtual message_id get_message_id() const = 0;
	virtual void set_message_id(message_id _id) = 0;
	virtual service_id get_service_id() const = 0;
	virtual void set_service_id(service_id _id) = 0;
	virtual method_id get_method_id() const = 0;
	virtual void set_method_id(method_id _id) = 0;
	virtual length get_length() const = 0;
	virtual void set_length(length _length) = 0;
	virtual request_id get_request_id() const = 0;
	virtual void set_request_id(request_id _id) = 0;
	virtual client_id get_client_id() const = 0;
	virtual void set_client_id(client_id _id) = 0;
	virtual session_id get_session_id() const = 0;
	virtual void set_session_id(session_id _id) = 0;
	virtual protocol_version get_protocol_version() const = 0;
	virtual void set_protocol_version(protocol_version _version) = 0;
	virtual interface_version get_interface_version() const = 0;
	virtual void set_interface_version(interface_version _version) = 0;
	virtual message_type get_message_type() const = 0;
	virtual void set_message_type(message_type _type) = 0;
	virtual return_code get_return_code() const = 0;
	virtual void set_return_code(return_code _code) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_MESSAGE_BASE_HPP
