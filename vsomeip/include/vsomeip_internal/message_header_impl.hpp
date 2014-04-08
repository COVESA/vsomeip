//
// message_header_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_MESSAGE_HEADER_IMPL_HPP
#define VSOMEIP_INTERNAL_MESSAGE_HEADER_IMPL_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/serializable.hpp>

namespace vsomeip {

class message_base;

class message_header_impl : virtual public serializable {

public:
	message_header_impl();
	message_header_impl(const message_header_impl& header);

	virtual bool serialize(serializer *_to) const;
	virtual bool deserialize(deserializer *_from);

	// internal
	message_base * get_owner() const;
	void set_owner(message_base *_owner);

public:
	service_id service_id_;
	instance_id instance_id_; // NOT(!) part of the SOME/IP header
	method_id method_id_;
	length length_;
	client_id client_id_;
	session_id session_id_;
	protocol_version protocol_version_;
	interface_version interface_version_;
	message_type_enum message_type_;
	return_code_enum return_code_;

	message_base *owner_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_MESSAGE_HEADER_IMPL_HPP
