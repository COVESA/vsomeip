// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_LOCAL_ROUTING_MANAGER_HPP
#define VSOMEIP_LOCAL_ROUTING_MANAGER_HPP

#include <map>
#include <memory>
#include <set>

#include <boost/asio/io_service.hpp>

#include <vsomeip/primitive_types.hpp>

#include "endpoint_host.hpp"

namespace vsomeip {

class endpoint;

class local_routing_manager:
public:
	virtual ~local_routing_manager() {};

	virtual endpoint * find_local_client(client_t _client) = 0;
	virtual endpoint * create_local_client(client_t _client) = 0;
	virtual endpoint * find_or_create_local_client(client_t _client) = 0;
	virtual void remove_local_client(client_t) = 0;

	virtual endpoint * find_local_service(service_t _service, instance_t _instance) = 0;
	virtual void add_local_service(client_t _client, service_t _service, instance_t _instance) = 0;
	virtual void remove_local_service(service_t _service, instance_t _instance) = 0;

	// endpoint_host
	virtual void on_message(const byte_t *_data, length_t _length, endpoint *_receiver) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_LOCAL_ROUTING_MANAGER_HPP
