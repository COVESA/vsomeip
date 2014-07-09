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

#include "../../routing/include/routing_types.hpp"

namespace vsomeip {

class configuration;

namespace sd {

class service_discovery_host {
public:
	virtual ~service_discovery_host() {};

	virtual boost::asio::io_service & get_io() = 0;
	virtual std::shared_ptr< configuration > get_configuration() const = 0;

	virtual void create_service_discovery_endpoint(
					const std::string &_address, uint16_t _port, const std::string &_protocol) = 0;

	virtual service_map_t get_offered_services(const std::string &_name) const = 0;

	virtual void send(client_t _client, std::shared_ptr< message > _message, bool _flush, bool _reliable) = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_HOST_HPP
