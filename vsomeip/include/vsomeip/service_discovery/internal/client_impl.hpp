//
// client_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_IMPL_HPP

#include <boost/shared_ptr.hpp>

namespace vsomeip {
namespace service_discovery {

class client_impl {
public:
	client_impl();

	void init();
	void start();
	void stop();
	void process();

private:
	struct state_machine;
	boost::shared_ptr< state_machine > state_machine_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_IMPL_HPP
