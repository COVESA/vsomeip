// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_HOST_HPP
#define VSOMEIP_SERVICE_DISCOVERY_HOST_HPP

#include <map>
#include <memory>

#include <boost/asio/io_service.hpp>

namespace vsomeip {

class servicegroup;

namespace sd {

class service_discovery_host {
public:
	virtual ~service_discovery_host() {};

	virtual boost::asio::io_service & get_io() = 0;
	virtual const std::map< std::string, std::shared_ptr< servicegroup > > & get_servicegroups() const = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_HOST_HPP
