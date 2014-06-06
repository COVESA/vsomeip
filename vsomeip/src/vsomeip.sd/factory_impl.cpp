//
// factory_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/sd/constants.hpp>
#include <vsomeip_internal/sd/factory_impl.hpp>
#include <vsomeip_internal/sd/message_impl.hpp>
#include <vsomeip_internal/sd/service_discovery_impl.hpp>

namespace vsomeip {

class daemon_impl;

namespace sd {

factory * factory_impl::get_instance() {
	static factory_impl the_factory;
	return &the_factory;
}

factory_impl::~factory_impl() {
}

service_discovery * factory_impl::create_service_discovery(daemon &_owner) const {
	return new service_discovery_impl(_owner);
}

message * factory_impl::create_message() const {
	message *its_message = new message_impl;
	if (0 != its_message) {
		its_message->set_service_id(VSOMEIP_SERVICE_DISCOVERY_SERVICE);
		its_message->set_method_id(VSOMEIP_SERVICE_DISCOVERY_METHOD);
		its_message->set_client_id(VSOMEIP_SERVICE_DISCOVERY_CLIENT);
		// Session-ID will be set when the message is sent
		its_message->set_protocol_version(VSOMEIP_SERVICE_DISCOVERY_PROTOCOL_VERSION);
		its_message->set_interface_version(VSOMEIP_SERVICE_DISCOVERY_INTERFACE_VERSION);
		its_message->set_message_type(VSOMEIP_SERVICE_DISCOVERY_MESSAGE_TYPE);
		its_message->set_return_code(VSOMEIP_SERVICE_DISCOVERY_RETURN_CODE);
	}
	return its_message;
}

} // namespace sd
} // namespace vsomeip


