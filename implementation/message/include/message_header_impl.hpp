// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_MESSAGE_HEADER_IMPL_HPP
#define VSOMEIP_MESSAGE_HEADER_IMPL_HPP

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
	service_t service_;
	method_t method_;
	length_t length_;
	client_t client_;
	session_t session_;
	protocol_version_t protocol_version_;
	interface_version_t interface_version_;
	message_type_e type_;
	return_code_e code_;

	instance_t instance_;
	message_base *owner_;
};

} // namespace vsomeip

#endif // VSOMEIP_MESSAGE_HEADER_IMPL_HPP
