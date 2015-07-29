// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>

#include <vsomeip/configuration.hpp>
#include <vsomeip/constants.hpp>
#include <vsomeip/logger.hpp>

#define DESIRED_ADDRESS			"10.0.2.15"
#define DESIRED_HAS_CONSOLE		true
#define DESIRED_HAS_FILE		true
#define DESIRED_HAS_DLT			false
#define DESIRED_LOGLEVEL		"debug"
#define DESIRED_LOGFILE			"/home/someip/another-file.log"
#define DESIRED_HOST			"my_application"
#define DESIRED_PROTOCOL		"tcp"
#define DESIRED_PORT			30666

#define DESIRED_RELIABLE_1234_0022	30506
#define DESIRED_UNRELIABLE_1234_0022	31000

#define DESIRED_RELIABLE_1234_0023 	30503
#define DESIRED_UNRELIABLE_1234_0023	vsomeip::ILLEGAL_PORT

#define DESIRED_RELIABLE_2277_0022	30505
#define DESIRED_UNRELIABLE_2277_0022	31001

#define DESIRED_RELIABLE_2266_0022	30505
#define DESIRED_UNRELIABLE_2266_0022	30507

#define DESIRED_RELIABLE_4466_0321	30506
#define DESIRED_UNRELIABLE_4466_0321	30444

#define DESIRED_ADDRESS_1234_0022					"local"
#define DESIRED_MIN_INITIAL_DELAY_1234_0022			10
#define DESIRED_MAX_INITIAL_DELAY_1234_0022			100
#define DESIRED_REPETITION_BASE_DELAY_1234_0022		200
#define DESIRED_REPETITION_MAX_1234_0022			7
#define DESIRED_CYCLIC_OFFER_DELAY_1234_0022		2000
#define DESIRED_CYCLIC_REQUEST_DELAY_1234_0022		2001

#define DESIRED_ADDRESS_2277_0022					"local"
#define DESIRED_MIN_INITIAL_DELAY_2277_0022			100
#define DESIRED_MAX_INITIAL_DELAY_2277_0022			200
#define DESIRED_REPETITION_BASE_DELAY_2277_0022		300
#define DESIRED_REPETITION_MAX_2277_0022			5
#define DESIRED_CYCLIC_OFFER_DELAY_2277_0022		2500
#define DESIRED_CYCLIC_REQUEST_DELAY_2277_0022		2221

#define DESIRED_ADDRESS_4466_0321					"10.0.2.23"

template<class T>
void check(const T &_is, const T &_desired, const std::string &_test) {
	if (_is == _desired) {
		VSOMEIP_INFO << "Test \"" << _test << "\" succeeded.";
	} else {
		VSOMEIP_ERROR << "Test \"" << _test << "\" failed! (" << _is << " != " << _desired << ")";
	}
}

int main(int argc, char **argv) {
	std::string its_path;

	int i = 0;
	while (i < argc-1) {
		if (std::string("--someip") == argv[i]) {
			its_path = argv[i+1];
			break;
		}

		i++;
	}

	if (its_path == "") {
		std::cerr << "Usage: " << argv[0] << " --someip <path>" << std::endl;
		return -1;
	}

	vsomeip::configuration *its_configuration = vsomeip::configuration::get(its_path);

	// 1. Did we get a configuration object?
	if (0 == its_configuration) {
		VSOMEIP_ERROR << "No configuration object. Either memory overflow or loading error detected!";
		return -1;
	}

	// 2. Check host address
	boost::asio::ip::address its_address = its_configuration->get_unicast();
	check<std::string>(its_address.to_string(), DESIRED_ADDRESS, "HOST ADDRESS");

	// 3. Check logging
	bool has_console = its_configuration->has_console_log();
	bool has_file = its_configuration->has_file_log();
	bool has_dlt = its_configuration->has_dlt_log();
	std::string logfile = its_configuration->get_logfile();
	boost::log::trivial::severity_level loglevel = its_configuration->get_loglevel();

	check<bool>(has_console, DESIRED_HAS_CONSOLE, "HAS CONSOLE");
	check<bool>(has_file, DESIRED_HAS_FILE, "HAS FILE");
	check<bool>(has_dlt, DESIRED_HAS_DLT, "HAS DLT");
	check<std::string>(logfile, DESIRED_LOGFILE, "LOGFILE");
	check<std::string>(boost::log::trivial::to_string(loglevel), DESIRED_LOGLEVEL, "LOGLEVEL");

	// 4. Services
	uint16_t its_reliable = its_configuration->get_reliable_port(0x1234, 0x0022);
	uint16_t its_unreliable = its_configuration->get_unreliable_port(0x1234, 0x0022);

	check<uint16_t>(its_reliable, DESIRED_RELIABLE_1234_0022, "RELIABLE_TEST_1234_0022");
	check<uint16_t>(its_unreliable, DESIRED_UNRELIABLE_1234_0022, "UNRELIABLE_TEST_1234_0022");

	its_reliable = its_configuration->get_reliable_port(0x1234, 0x0023);
	its_unreliable = its_configuration->get_unreliable_port(0x1234, 0x0023);

	check<uint16_t>(its_reliable, DESIRED_RELIABLE_1234_0023, "RELIABLE_TEST_1234_0023");
	check<uint16_t>(its_unreliable, DESIRED_UNRELIABLE_1234_0023, "UNRELIABLE_TEST_1234_0023");

	its_reliable = its_configuration->get_reliable_port(0x2277, 0x0022);
	its_unreliable = its_configuration->get_unreliable_port(0x2277, 0x0022);

	check<uint16_t>(its_reliable, DESIRED_RELIABLE_2277_0022, "RELIABLE_TEST_2277_0022");
	check<uint16_t>(its_unreliable, DESIRED_UNRELIABLE_2277_0022, "UNRELIABLE_TEST_2277_0022");

	its_reliable = its_configuration->get_reliable_port(0x4466, 0x0321);
	its_unreliable = its_configuration->get_unreliable_port(0x4466, 0x0321);

	check<uint16_t>(its_reliable, DESIRED_RELIABLE_4466_0321, "RELIABLE_TEST_4466_0321");
	check<uint16_t>(its_unreliable, DESIRED_UNRELIABLE_4466_0321, "UNRELIABLE_TEST_4466_0321");

	its_reliable = its_configuration->get_reliable_port(0x2277, 0x0022);
	its_unreliable = its_configuration->get_unreliable_port(0x2277, 0x0022);

	check<uint16_t>(its_reliable, DESIRED_RELIABLE_2277_0022, "RELIABLE_TEST_2277_0022");
	check<uint16_t>(its_unreliable, DESIRED_UNRELIABLE_2277_0022, "UNRELIABLE_TEST_2277_0022");

	std::string its_address_s = its_configuration->get_unicast(0x1234, 0x0022);
	std::string its_group_name = its_configuration->get_group(0x1234, 0x0022);
	uint32_t its_min_initial_delay = its_configuration->get_min_initial_delay(its_group_name);
	uint32_t its_max_initial_delay = its_configuration->get_max_initial_delay(its_group_name);
	uint32_t its_repetition_base_delay = its_configuration->get_repetition_base_delay(its_group_name);
	uint8_t its_repetition_max = its_configuration->get_repetition_max(its_group_name);
	uint32_t its_cyclic_offer_delay = its_configuration->get_cyclic_offer_delay(its_group_name);
	uint32_t its_cyclic_request_delay = its_configuration->get_cyclic_request_delay(its_group_name);

	check<std::string>(its_address_s, DESIRED_ADDRESS_1234_0022, "ADDRESS_TEST_1234_0022");
	check<uint32_t>(its_min_initial_delay, DESIRED_MIN_INITIAL_DELAY_1234_0022, "MIN_INITIAL_DELAY_TEST_1234_0022");
	check<uint32_t>(its_max_initial_delay, DESIRED_MAX_INITIAL_DELAY_1234_0022, "MAX_INITIAL_DELAY_TEST_1234_0022");
	check<uint32_t>(its_repetition_base_delay, DESIRED_REPETITION_BASE_DELAY_1234_0022, "REPETITION_BASE_DELAY_TEST_1234_0022");
	check<uint8_t>(its_repetition_max, DESIRED_REPETITION_MAX_1234_0022, "REPETITION_MAX_TEST_1234_0022");
	check<uint32_t>(its_cyclic_offer_delay, DESIRED_CYCLIC_OFFER_DELAY_1234_0022, "CYCLIC_OFFER_DELAY_TEST_1234_0022");
	check<uint32_t>(its_cyclic_request_delay, DESIRED_CYCLIC_REQUEST_DELAY_1234_0022, "CYCLIC_REQUEST_DELAY_TEST_1234_0022");

	its_group_name = its_configuration->get_group(0x1234, 0x0023);
	its_min_initial_delay = its_configuration->get_min_initial_delay(its_group_name);
	its_max_initial_delay = its_configuration->get_max_initial_delay(its_group_name);
	its_repetition_base_delay = its_configuration->get_repetition_base_delay(its_group_name);
	its_repetition_max = its_configuration->get_repetition_max(its_group_name);
	its_cyclic_offer_delay = its_configuration->get_cyclic_offer_delay(its_group_name);
	its_cyclic_request_delay = its_configuration->get_cyclic_request_delay(its_group_name);

	check<uint32_t>(its_min_initial_delay, DESIRED_MIN_INITIAL_DELAY_1234_0022, "MIN_INITIAL_DELAY_TEST_1234_0023");
	check<uint32_t>(its_max_initial_delay, DESIRED_MAX_INITIAL_DELAY_1234_0022, "MAX_INITIAL_DELAY_TEST_1234_0023");
	check<uint32_t>(its_repetition_base_delay, DESIRED_REPETITION_BASE_DELAY_1234_0022, "REPETITION_BASE_DELAY_TEST_1234_0023");
	check<uint8_t>(its_repetition_max, DESIRED_REPETITION_MAX_1234_0022, "REPETITION_MAX_TEST_1234_0023");
	check<uint32_t>(its_cyclic_offer_delay, DESIRED_CYCLIC_OFFER_DELAY_1234_0022, "CYCLIC_OFFER_DELAY_TEST_1234_0023");
	check<uint32_t>(its_cyclic_request_delay, DESIRED_CYCLIC_REQUEST_DELAY_1234_0022, "CYCLIC_REQUEST_DELAY_TEST_1234_0023");

	its_group_name = its_configuration->get_group(0x2277, 0x0022);
	its_min_initial_delay = its_configuration->get_min_initial_delay(its_group_name);
	its_max_initial_delay = its_configuration->get_max_initial_delay(its_group_name);
	its_repetition_base_delay = its_configuration->get_repetition_base_delay(its_group_name);
	its_repetition_max = its_configuration->get_repetition_max(its_group_name);
	its_cyclic_offer_delay = its_configuration->get_cyclic_offer_delay(its_group_name);
	its_cyclic_request_delay = its_configuration->get_cyclic_request_delay(its_group_name);

	check<uint32_t>(its_min_initial_delay, DESIRED_MIN_INITIAL_DELAY_2277_0022, "MIN_INITIAL_DELAY_TEST_2277_0022");
	check<uint32_t>(its_max_initial_delay, DESIRED_MAX_INITIAL_DELAY_2277_0022, "MAX_INITIAL_DELAY_TEST_2277_0022");
	check<uint32_t>(its_repetition_base_delay, DESIRED_REPETITION_BASE_DELAY_2277_0022, "REPETITION_BASE_DELAY_TEST_2277_0022");
	check<uint8_t>(its_repetition_max, DESIRED_REPETITION_MAX_2277_0022, "REPETITION_MAX_TEST_2277_0022");
	check<uint32_t>(its_cyclic_offer_delay, DESIRED_CYCLIC_OFFER_DELAY_2277_0022, "CYCLIC_OFFER_DELAY_TEST_2277_0022");
	check<uint32_t>(its_cyclic_request_delay, DESIRED_CYCLIC_REQUEST_DELAY_2277_0022, "CYCLIC_REQUEST_DELAY_TEST_2277_0022");

	its_group_name = its_configuration->get_group(0x2266, 0x0022);
	its_min_initial_delay = its_configuration->get_min_initial_delay(its_group_name);
	its_max_initial_delay = its_configuration->get_max_initial_delay(its_group_name);
	its_repetition_base_delay = its_configuration->get_repetition_base_delay(its_group_name);
	its_repetition_max = its_configuration->get_repetition_max(its_group_name);
	its_cyclic_offer_delay = its_configuration->get_cyclic_offer_delay(its_group_name);
	its_cyclic_request_delay = its_configuration->get_cyclic_request_delay(its_group_name);

	check<uint32_t>(its_min_initial_delay, DESIRED_MIN_INITIAL_DELAY_2277_0022, "MIN_INITIAL_DELAY_TEST_2266_0022");
	check<uint32_t>(its_max_initial_delay, DESIRED_MAX_INITIAL_DELAY_2277_0022, "MAX_INITIAL_DELAY_TEST_2266_0022");
	check<uint32_t>(its_repetition_base_delay, DESIRED_REPETITION_BASE_DELAY_2277_0022, "REPETITION_BASE_DELAY_TEST_2266_0022");
	check<uint8_t>(its_repetition_max, DESIRED_REPETITION_MAX_2277_0022, "REPETITION_MAX_TEST_2266_0022");
	check<uint32_t>(its_cyclic_offer_delay, DESIRED_CYCLIC_OFFER_DELAY_2277_0022, "CYCLIC_OFFER_DELAY_TEST_2266_0022");
	check<uint32_t>(its_cyclic_request_delay, DESIRED_CYCLIC_REQUEST_DELAY_2277_0022, "CYCLIC_REQUEST_DELAY_TEST_2266_0022");

	its_address_s = its_configuration->get_unicast(0x4466, 0x0321);
	check<std::string>(its_address_s, DESIRED_ADDRESS_4466_0321, "ADDRESS_TEST_4466_0321");

	// 5. Service discovery
	std::string protocol = its_configuration->get_service_discovery_protocol();
	uint16_t port = its_configuration->get_service_discovery_port();

	check<std::string>(protocol, DESIRED_PROTOCOL, "SERVICE DISCOVERY PROTOCOL");
	check<uint16_t>(port, DESIRED_PORT, "SERVICE DISCOVERY PORT");

	return 0;
}



