//
// factory_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_FACTORY_IMPL_HPP
#define VSOMEIP_INTERNAL_FACTORY_IMPL_HPP

#include <vsomeip/factory.hpp>

namespace vsomeip {

class factory_impl
		: public factory {
public:
	virtual ~factory_impl();

	static factory * get_instance();

	application * create_application() const;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_FACTORY_IMPL_HPP

