//
// service_registration.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_REGISTRATION_HPP
#define VSOMEIP_SERVICE_REGISTRATION_HPP

#include <boost/shared_ptr.hpp>

namespace vsomeip {
namespace service_discovery {

class service_registration {
public:
	service_registration();

	void start();
	void stop();

private:
	struct state_machine;
	boost::shared_ptr< state_machine > state_machine_;

	static int last_id__;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_INSTANCE_HPP
