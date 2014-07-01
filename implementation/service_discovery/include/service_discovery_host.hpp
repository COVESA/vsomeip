// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_SERVICE_DISCOVERY_HOST
#define VSOMEIP_SD_SERVICE_DISCOVERY_HOST

#include <memory>

#include <boost/asio/io_service.hpp>

namespace vsomeip {
namespace sd {

class service_discovery;

class service_discovery_host {
public:
	virtual ~service_discovery_host() {};

	virtual boost::asio::io_service & get_io() = 0;

private:
	std::shared_ptr< service_discovery > discovery_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_SERVICE_DISCOVERY_HOST

