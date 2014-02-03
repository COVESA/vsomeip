//
// registration.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_DAEMON_REGISTRATION_HPP
#define	 VSOMEIP_DAEMON_REGISTRATION_HPP

namespace vsomeip {
namespace daemon {

/// The internal interface provided the following features to local
/// applications:
/// 1. Inform about the status of remote services
/// 2. Require a service
/// 3. Release a service
/// 4. Inform about the status of local services
/// 5. Set the status of a local service
/// 6.
class registration {
public:
	registration();
	void start();

private:
	bool is_required;
};

} // namespace daemon
} // namespace vsomeip

#endif // VSOMEIP_DAEMON_REGISTRATION_HPP
