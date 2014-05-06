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

namespace vsomeip {
namespace sd {

class factory_impl
		: virtual public factory {
public:
	static factory * get_instance();

	virtual ~factory_impl();

	service_discovery * create_service_discovery(
			boost::asio::io_service &_service) const;

	message * create_message() const;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_FACTORY_IMPL_HPP
