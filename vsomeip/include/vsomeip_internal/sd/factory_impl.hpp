//
// factory_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_FACTORY_IMPL_HPP
#define VSOMEIP_INTERNAL_SD_FACTORY_IMPL_HPP

#include <vsomeip/sd/factory.hpp>
#include <vsomeip_internal/factory_impl.hpp>

namespace vsomeip {
namespace sd {

class factory_impl
		: virtual public factory,
		  virtual public vsomeip::factory_impl {
public:
	static factory * get_instance();

	virtual ~factory_impl();

	message * create_service_discovery_message() const;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_FACTORY_IMPL_HPP
