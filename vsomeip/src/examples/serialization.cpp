//
// serialization.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <iomanip>
#include <iostream>

#include <string>
#include <vector>

#include <vsomeip/message.hpp>
#include <vsomeip/service_discovery/factory.hpp>
#include <vsomeip/service_discovery/message.hpp>
#include <vsomeip/service_discovery/configuration_option.hpp>
#include <vsomeip/service_discovery/ipv4_endpoint_option.hpp>
#include <vsomeip/service_discovery/ipv6_endpoint_option.hpp>
#include <vsomeip/service_discovery/load_balancing_option.hpp>
#include <vsomeip/service_discovery/protection_option.hpp>
#include <vsomeip/service_discovery/service_entry.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/deserializer.hpp>

#include <typeinfo>

namespace sd = vsomeip::service_discovery;

int main() {

	std::cout << "Serializing Application message:" << std::endl;
	vsomeip::message *m = vsomeip::factory::get_default_factory()->create_message();
	m->set_message_id(0x12345678);
	m->set_request_id(0x98765432);
	m->set_interface_version(0x2);
	m->set_message_type(vsomeip::message_type::REQUEST_NO_RETURN);
	m->set_return_code(vsomeip::return_code::OK);

	uint8_t pl[] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x10, 0x11, 0x12, 0x13, 0x15, 0x14 };
	vsomeip::payload& p = m->get_payload();
	p.set_data(pl, sizeof(pl));

	vsomeip::serializer *s = vsomeip::factory::get_default_factory()->create_serializer();
	s->create_data(100);

	if (!s->serialize(m)) {
		std::cout << "Serialization failed!" << std::endl;
	}

	uint8_t *b = s->get_data();
	uint32_t l = s->get_size();

	for (uint32_t i = 0; i < l; i++) {
		if (i % 4 == 0) std::cout << std::endl;
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b[i]  << " ";
	}
	std::cout << std::endl << std::endl;

	std::cout << "Serializing ServiceDiscovery message:" << std::endl;

	sd::message *sdm
		= sd::factory::get_default_factory()->create_service_discovery_message();
	sdm->set_request_id(0x11223344);
	sdm->set_interface_version(0x01);
	sdm->set_message_type(vsomeip::message_type::NOTIFICATION);
	sdm->set_return_code(vsomeip::return_code::OK);
	sdm->set_unicast_flag(true);

	std::cout << 1 << std::endl;
	sd::ipv4_endpoint_option& l_option = sdm->create_ipv4_endpoint_option();
	l_option.set_address(0xC0A80001);
	l_option.set_port(0x1A91);
	l_option.set_protocol(vsomeip::ip_protocol::TCP);

	std::cout << 2 << std::endl;
	sd::ipv4_endpoint_option& l_option2 = sdm->create_ipv4_endpoint_option();
	l_option2.set_address(0xC0A80001);
	l_option2.set_port(0x1A91);
	l_option2.set_protocol(vsomeip::ip_protocol::UDP);

	std::cout << 3 << std::endl;
	sd::service_entry& l_entry = sdm->create_service_entry();
	l_entry.set_type(sd::entry_type::FIND_SERVICE);
	l_entry.set_service_id(0x4711);
	l_entry.set_instance_id(0xFFFF);
	l_entry.set_major_version(0xFF);
	l_entry.set_minor_version(0xFFFFFFFF);
	l_entry.set_time_to_live(3600);

	std::cout << 4 << std::endl;
	sd::service_entry& l_entry2 = sdm->create_service_entry();
	l_entry2.set_type(sd::entry_type::OFFER_SERVICE);
	l_entry2.set_service_id(0x1001);
	l_entry2.set_instance_id(0x0001);
	l_entry2.set_major_version(0x01);
	l_entry2.set_minor_version(0x00000032);
	l_entry2.assign_option(l_option, 1);
	l_entry2.assign_option(l_option2, 1);

	std::cout << 5 << std::endl;
	s->create_data(100);
	if (!s->serialize(sdm)) {
		std::cout << "Serialization failed!" << std::endl;
	}

	std::cout << 6 << std::endl;
	b = s->get_data();
	l = s->get_size();

	std::cout << l << std::endl;

	for (uint32_t i = 0; i < l; i++) {
		if (i % 4 == 0) std::cout << std::endl;
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b[i]  << " ";
	}
	std::cout << std::endl << std::endl;

	std::cout << "Deserializing service discovery message" << std::endl;
	vsomeip::deserializer *d = sd::factory::get_default_factory()->create_deserializer(b, l);

	std::cout << typeid(d).name() << std::endl;

	std::cout << "done." << std::endl;
	vsomeip::message_base *dsdm = d->deserialize_message();

	s->create_data(100);
	s->serialize(dsdm);
	b = s->get_data();
	l = s->get_size();

	for (uint32_t i = 0; i < l; i++) {
		if (i % 4 == 0) std::cout << std::endl;
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b[i]  << " ";
	}
	std::cout << std::endl << std::endl;
}
