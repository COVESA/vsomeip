//
// configuration.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_CONFIGURATION_HPP
#define VSOMEIP_SERVICE_DISCOVERY_CONFIGURATION_HPP

#include <vsomeip/configuration.hpp>

namespace vsomeip {
namespace service_discovery {

class configuration
		: virtual public vsomeip::configuration {
public:
	static configuration * get_instance();
	virtual ~configuration() {};

	virtual bool use_service_discovery() const = 0;
	virtual bool use_virtual_mode() const = 0;
	virtual const std::string & get_registry_path() const = 0;

	virtual const std::string & get_unicast_address() const = 0;
	virtual const std::string & get_multicast_address() const = 0;
	virtual uint16_t get_port() const = 0;

	virtual uint32_t get_min_initial_delay() const = 0;
	virtual uint32_t get_max_initial_delay() const = 0;
	virtual uint32_t get_repetition_base_delay() const = 0;
	virtual uint8_t get_repetition_max() const = 0;
	virtual uint32_t get_ttl() const = 0;
	virtual uint32_t get_cyclic_offer_delay() const = 0;
	virtual uint32_t get_cyclic_request_delay() const = 0;
	virtual uint32_t get_request_response_delay() const = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_CONFIGURATION_HPP
