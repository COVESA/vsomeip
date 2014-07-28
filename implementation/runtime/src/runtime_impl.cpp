// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/defines.hpp>

#include "../include/application_impl.hpp"
#include "../include/runtime_impl.hpp"
#include "../../message/include/message_impl.hpp"
#include "../../message/include/payload_impl.hpp"

namespace vsomeip {

runtime * runtime_impl::get() {
	static runtime_impl the_runtime;
	return &the_runtime;
}

runtime_impl::~runtime_impl() {
}

std::shared_ptr<application> runtime_impl::create_application(
		const std::string &_name) const {
	return std::make_shared < application_impl > (_name);
}

std::shared_ptr<message> runtime_impl::create_request() const {
	std::shared_ptr<message_impl> its_request =
			std::make_shared<message_impl>();
	its_request->set_protocol_version(VSOMEIP_PROTOCOL_VERSION);
	its_request->set_message_type(message_type_e::REQUEST);
	its_request->set_return_code(return_code_e::E_OK);
	return its_request;
}

std::shared_ptr<message> runtime_impl::create_response(
		const std::shared_ptr<message> &_request) const {
	std::shared_ptr<message_impl> its_response =
			std::make_shared<message_impl>();
	its_response->set_service(_request->get_service());
	its_response->set_instance(_request->get_instance());
	its_response->set_method(_request->get_method());
	its_response->set_client(_request->get_client());
	its_response->set_session(_request->get_session());
	its_response->set_interface_version(_request->get_interface_version());
	its_response->set_message_type(message_type_e::RESPONSE);
	its_response->set_return_code(return_code_e::E_OK);
	return its_response;
}

std::shared_ptr<message> runtime_impl::create_notification() const {
	std::shared_ptr<message_impl> its_request =
			std::make_shared<message_impl>();
	its_request->set_protocol_version(VSOMEIP_PROTOCOL_VERSION);
	its_request->set_message_type(message_type_e::NOTIFICATION);
	its_request->set_return_code(return_code_e::E_OK);
	return its_request;
}

std::shared_ptr<payload> runtime_impl::create_payload() const {
	return std::make_shared<payload_impl>();
}

std::shared_ptr<payload> runtime_impl::create_payload(
		const byte_t *_data, uint32_t _size) const {
	return std::make_shared<payload_impl>(_data, _size);
}

std::shared_ptr<payload> runtime_impl::create_payload(
		const std::vector<byte_t> &_data) const {
	return std::make_shared<payload_impl>(_data);
}

} // namespace vsomeip
