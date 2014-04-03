//
// message_base_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_MESSAGE_BASE_IMPL_HPP
#define VSOMEIP_INTERNAL_MESSAGE_BASE_IMPL_HPP

#include <boost/thread.hpp>

#include <vsomeip/message.hpp>
#include <vsomeip_internal/message_header_impl.hpp>

namespace vsomeip {

class message_base_impl : virtual public message_base {
public:
	message_base_impl();
	virtual ~message_base_impl();

	const endpoint * get_source() const;
	void set_source(const endpoint *_endpoint);
	const endpoint * get_target() const;
	void set_target(const endpoint *_endpoint);

	message_id get_message_id() const;
	void set_message_id(message_id _id);
	service_id get_service_id() const;
	void set_service_id(service_id _id);
	method_id get_method_id() const;
	void set_method_id(method_id _id);
	request_id get_request_id() const;
	void set_request_id(request_id _id);
	client_id get_client_id() const;
	void set_client_id(client_id _id);
	session_id get_session_id() const;
	void set_session_id(session_id _id);
	protocol_version get_protocol_version() const;
	void set_protocol_version(protocol_version _version);
	interface_version get_interface_version() const;
	void set_interface_version(interface_version _version);
	message_type_enum get_message_type() const;
	void set_message_type(message_type_enum _type);
	return_code_enum get_return_code() const;
	void set_return_code(return_code_enum _code);

	message * get_owner() const;
	void set_owner(message *_owner);

protected: // members
	message_header_impl header_;

	const endpoint *source_;
	const endpoint *target_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_MESSAGE_BASE_IMPL_HPP
