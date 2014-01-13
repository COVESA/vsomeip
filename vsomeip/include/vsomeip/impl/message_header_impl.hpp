//
// message_header_impl.hpp
//
// Date: 	Nov 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef MESSAGE_HEADER_IMPL_HPP
#define MESSAGE_HEADER_IMPL_HPP

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

	message_base * get_owner() const;
	void set_owner(message_base *_owner);

public:
	service_id service_id_;
	method_id method_id_;
	client_id client_id_;
	session_id session_id_;
	protocol_version protocol_version_;
	interface_version interface_version_;
	message_type message_type_;
	return_code return_code_;

	message_base *owner_;
};

} // namespace vsomeip

#endif // MESSAGE_HEADER_IMPL_HPP
