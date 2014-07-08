// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_HPP
#define VSOMEIP_SERVICE_DISCOVERY_HPP

#include <boost/asio/io_service.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class configuration;

namespace sd {

class service_discovery {
public:
	virtual ~service_discovery() {};

	virtual std::shared_ptr< configuration > get_configuration() const = 0;
	virtual boost::asio::io_service & get_io() = 0;

	virtual void init() = 0;
	virtual void start() = 0;
	virtual void stop() = 0;

	virtual void offer_service(service_t _service, instance_t _instance) = 0;
	virtual void stop_offer_service(service_t _service, instance_t _instance) = 0;

	virtual void request_service(service_t _service, instance_t _instance,
					major_version_t _major, minor_version_t _minor, ttl_t _ttl) = 0;
	virtual void release_service(service_t _service, instance_t _instance) = 0;

	virtual void send(const std::string &_name, bool _is_announcing) = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_HPP




