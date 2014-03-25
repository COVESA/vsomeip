//
// option.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_OPTION_HPP
#define VSOMEIP_SERVICE_DISCOVERY_OPTION_HPP

#include <vsomeip/serializable.hpp>
#include <vsomeip/service_discovery/enumeration_types.hpp>

namespace vsomeip {
namespace service_discovery {

class option
	: virtual public serializable,
	  virtual public deserializable {
public:
	virtual ~option() {};
	virtual bool operator==(const option &_option) const = 0;

	virtual uint16_t get_length() const = 0;
	virtual option_type get_type() const = 0;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_OPTION_HPP
