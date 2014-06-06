//
// message_header_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/constants.hpp>
#include <vsomeip_internal/deserializer.hpp>
#include <vsomeip_internal/message_base_impl.hpp>
#include <vsomeip_internal/message_header_impl.hpp>
#include <vsomeip_internal/serializer.hpp>

namespace vsomeip {

message_header_impl::message_header_impl()
	: service_id_(0x0), instance_id_(0x0), method_id_(0x0),
	  client_id_(0x0), session_id_(0x0),
	  protocol_version_(0x1), interface_version_(0x0),
	  message_type_(message_type_enum::UNKNOWN),
	  return_code_(return_code_enum::UNKNOWN) {
};

message_header_impl::message_header_impl(const message_header_impl& _header)
	: service_id_(_header.service_id_), instance_id_(_header.instance_id_), method_id_(_header.method_id_),
	  client_id_(_header.client_id_), session_id_(_header.session_id_),
	  protocol_version_(_header.protocol_version_), interface_version_(_header.interface_version_),
	  message_type_(_header.message_type_),
	  return_code_(_header.return_code_) {
};

bool message_header_impl::serialize(serializer *_to) const {
	return (0 != _to
			&& _to->serialize(service_id_)
			&& _to->serialize(method_id_)
			&& _to->serialize(owner_->get_length())
			&& _to->serialize(client_id_)
			&& _to->serialize(session_id_)
			&& _to->serialize(protocol_version_)
			&& _to->serialize(interface_version_)
			&& _to->serialize(static_cast<uint8_t>(message_type_))
			&& _to->serialize(static_cast<uint8_t>(return_code_)));
};

bool message_header_impl::deserialize(deserializer *_from) {
	bool is_successful;

	uint8_t tmp_message_type, tmp_return_code;
	uint32_t tmp_length;

	is_successful = (0 != _from
			&& _from->deserialize(service_id_)
			&& _from->deserialize(method_id_)
			&& _from->deserialize(tmp_length)
			&& _from->deserialize(client_id_)
			&& _from->deserialize(session_id_)
			&& _from->deserialize(protocol_version_)
			&& _from->deserialize(interface_version_)
			&& _from->deserialize(tmp_message_type)
			&& _from->deserialize(tmp_return_code));

	if (is_successful) {
		message_type_ = static_cast< message_type_enum >(tmp_message_type);
		return_code_ = static_cast< return_code_enum >(tmp_return_code);
		length_ = static_cast< length >(tmp_length - VSOMEIP_STATIC_HEADER_SIZE);
	}

	return is_successful;
};

message_base * message_header_impl::get_owner() const {
	return owner_;
}

void message_header_impl::set_owner(message_base *_owner) {
	owner_ = _owner;
}

} // namespace vsomeip


