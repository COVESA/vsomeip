//
// load_balancing_option_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SERVICE_DISCOVERY_LOAD_BALANCING_OPTION_IMPL_HPP
#define VSOMEIP_INTERNAL_SERVICE_DISCOVERY_LOAD_BALANCING_OPTION_IMPL_HPP

#include <vsomeip/service_discovery/load_balancing_option.hpp>
#include <vsomeip_internal/service_discovery/option_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class load_balancing_option_impl
		: virtual public load_balancing_option,
		  virtual public option_impl {
public:
	load_balancing_option_impl();
	virtual ~load_balancing_option_impl();
	bool operator ==(const option& _other) const;

	priority get_priority() const;
	void set_priority(priority _priority);

	weight get_weight() const;
	void set_weight(weight _weight);

	bool serialize(vsomeip::serializer *_to) const;
	bool deserialize(vsomeip::deserializer *_from);

private:
	priority priority_;
	weight weight_;
};

} // namespace service_discovery
} // namespace vsomeip


#endif // VSOMEIP_INTERNAL_SERVICE_DISCOVERY_LOAD_BALANCING_OPTION_IMPL_HPP
