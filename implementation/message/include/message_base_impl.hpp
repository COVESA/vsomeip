// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_MESSAGE_BASE_IMPL_HPP
#define VSOMEIP_MESSAGE_BASE_IMPL_HPP

#include <boost/thread.hpp>

#include <vsomeip/message.hpp>

#include "message_header_impl.hpp"

namespace vsomeip {

class message_base_impl
		: virtual public message_base {
public:
	message_base_impl();
	virtual ~message_base_impl();

	message_t get_message() const;
	void set_message(message_t _message);

	service_t get_service() const;
	void set_service(service_t _service);

	instance_t get_instance() const;
	void set_instance(instance_t _instance);

	method_t get_method() const;
	void set_method(method_t _method);

	request_t get_request() const;

	client_t get_client() const;
	void set_client(client_t _client);

	session_t get_session() const;
	void set_session(session_t _session);

	protocol_version_t get_protocol_version() const;
	void set_protocol_version(protocol_version_t _version);

	interface_version_t get_interface_version() const;
	void set_interface_version(interface_version_t _version);

	message_type_e get_message_type() const;
	void set_message_type(message_type_e _type);

	return_code_e get_return_code() const;
	void set_return_code(return_code_e _code);

	message * get_owner() const;
	void set_owner(message *_owner);

protected: // members
	message_header_impl header_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_MESSAGE_BASE_IMPL_HPP
