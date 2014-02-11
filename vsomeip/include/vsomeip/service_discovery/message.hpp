//
// message.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_MESSAGE_HPP
#define VSOMEIP_SERVICE_DISCOVERY_MESSAGE_HPP

#include <vector>

#include <vsomeip/message_base.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {
namespace service_discovery {

class entry;
class eventgroup_entry;
class service_entry;

class option;
class configuration_option;
class ipv4_endpoint_option;
class ipv6_endpoint_option;
class load_balancing_option;
class protection_option;

class message : virtual public vsomeip::message_base {
public:
	virtual ~message() {};

	virtual bool get_reboot_flag() const = 0;
	virtual void set_reboot_flag(bool _is_set) = 0;

	virtual bool get_unicast_flag() const = 0;
	virtual void set_unicast_flag(bool _is_set) = 0;

	virtual eventgroup_entry & create_eventgroup_entry() = 0;
	virtual service_entry & create_service_entry() = 0;

	virtual configuration_option & create_configuration_option() = 0;
	virtual ipv4_endpoint_option & create_ipv4_endpoint_option() = 0;
	virtual ipv6_endpoint_option & create_ipv6_endpoint_option() = 0;
	virtual load_balancing_option & create_load_balancing_option() = 0;
	virtual protection_option & create_protection_option() = 0;

	virtual const std::vector<entry *> get_entries() const = 0;
	virtual const std::vector<option *> get_options() const = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_MESSAGE_HPP
