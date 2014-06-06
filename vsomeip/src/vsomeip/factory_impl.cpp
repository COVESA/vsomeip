//
// factory_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <cstddef>

#include <vsomeip/config.hpp>
#include <vsomeip_internal/application_impl.hpp>
#include <vsomeip_internal/byteorder.hpp>
#include <vsomeip_internal/endpoint_impl.hpp>
#include <vsomeip_internal/application_impl.hpp>
#include <vsomeip_internal/message_impl.hpp>
#include <vsomeip_internal/field_impl.hpp>
#include <vsomeip_internal/factory_impl.hpp>

namespace vsomeip {

factory * factory_impl::get_instance() {
	static factory_impl the_factory;
	return &the_factory;
}

factory_impl::~factory_impl() {
}

application * factory_impl::create_application(const std::string &_name) const {
	return new application_impl(_name);
}

endpoint * factory_impl::get_endpoint(ip_address _address, ip_port _port, ip_protocol _protocol) {

	boost::asio::ip::address endpoint_address(boost::asio::ip::address::from_string(_address));
	uint32_t version = (endpoint_address.is_v4() ? 4 : 6);

	uint32_t identifier = (((uint32_t) _protocol) << 24
			| ((uint32_t) version) << 16 | ((uint32_t) _port));

	std::map< std::string, endpoint * >& matching_identifier =
			endpoints_[identifier];

	endpoint *requested_endpoint = 0;

	auto endpoint_iterator = matching_identifier.find(_address);
	if (endpoint_iterator == matching_identifier.end())
	{
		requested_endpoint = new endpoint_impl(endpoint_address, _port, _protocol);
		matching_identifier[_address] = requested_endpoint;
	} else {
		requested_endpoint = endpoint_iterator->second;
	}

	return requested_endpoint;
}

endpoint * factory_impl::get_endpoint(const uint8_t *_bytes, uint32_t _size) {
	endpoint *the_endpoint = 0;
	if (0 != _bytes && _size > VSOMEIP_MIN_ENDPOINT_SIZE) {
		ip_address address;
		ip_port port;
		ip_protocol protocol;

		protocol = static_cast< ip_protocol >(_bytes[0]);
		port = VSOMEIP_BYTES_TO_WORD(_bytes[1], _bytes[2]);

		if (_size == 7) {
			ipv4_address tmp = VSOMEIP_BYTES_TO_LONG(_bytes[3], _bytes[4], _bytes[5], _bytes[6]);
			boost::asio::ip::address_v4 tmp_address(tmp);
			address = tmp_address.to_string();
		} else {
			boost::asio::ip::address_v6::bytes_type tmp;
			std::memcpy(tmp.data(), &_bytes[3], _size - 3);
			boost::asio::ip::address_v6 tmp_address(tmp);
			address = tmp_address.to_string();
		}

		the_endpoint = get_endpoint(address, port, protocol);
	}

	return the_endpoint;
}

message * factory_impl::create_message() const {
	return new message_impl;
}

message * factory_impl::create_response(const message *_request) const {
	message *response = 0;
	if (0 != _request) {
		response = new message_impl;
		response->set_source(_request->get_target());
		response->set_target(_request->get_source());
		response->set_service_id(_request->get_service_id());
		response->set_instance_id(_request->get_instance_id());
		response->set_method_id(_request->get_method_id());
		response->set_client_id(_request->get_client_id());
		response->set_session_id(_request->get_session_id());
		response->set_message_type(message_type_enum::RESPONSE);
		response->set_return_code(return_code_enum::OK);
	}

	return response;
}

field * factory_impl::create_field(application *_application, service_id _service, instance_id _instance, event_id _event) const {
	return new field_impl(_application, _service, _instance, _event);
}


} // namespace vsomeip


