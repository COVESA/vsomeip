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

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CONFIGURATION_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CONFIGURATION_HPP

#include <vsomeip/internal/configuration_impl.hpp>
#include <vsomeip/service_discovery/configuration.hpp>

namespace vsomeip {
namespace service_discovery {

class configuration_impl
		: virtual public configuration,
		  virtual public vsomeip::configuration_impl {
public:
	static configuration * get_instance();
	configuration_impl();
	~configuration_impl();

	bool use_service_discovery() const;
	bool use_virtual_mode() const;
	const std::string & get_registry_path() const;

	const std::string & get_unicast_address() const;
	const std::string & get_multicast_address() const;
	uint16_t get_port() const;

	uint32_t get_min_initial_delay() const;
	uint32_t get_max_initial_delay() const;
	uint32_t get_repetition_base_delay() const;
	uint8_t get_repetition_max() const;
	uint32_t get_ttl() const;
	uint32_t get_cyclic_offer_delay() const;
	uint32_t get_cyclic_request_delay() const;
	uint32_t get_request_response_delay() const;

private:
	bool use_service_discovery_;
	bool use_virtual_mode_;
	std::string registry_path_;

	std::string unicast_address_;
	std::string multicast_address_;
	uint16_t port_;

	uint32_t min_initial_delay_;
	uint32_t max_initial_delay_;
	uint32_t repetition_base_delay_;
	uint8_t repetition_max_;
	uint32_t ttl_;
	uint32_t cyclic_offer_delay_;
	uint32_t cyclic_request_delay_;
	uint32_t request_response_delay_;

protected:
	void read();
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CONFIGURATION_HPP
