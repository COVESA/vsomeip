// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/service_discovery_host.hpp"
#include "../include/service_discovery_proxy.hpp"
#include "../../routing/include/routing_manager.hpp"

namespace vsomeip {
namespace sd {

service_discovery_proxy::service_discovery_proxy(service_discovery_host *_host)
	: host_(_host),routing_(_host->get_routing_manager()), io_(_host->get_routing_manager()->get_io()) {
}

service_discovery_proxy::~service_discovery_proxy() {
}

boost::asio::io_service & service_discovery_proxy::get_io() {
	return io_;
}

void service_discovery_proxy::init() {

}

void service_discovery_proxy::start() {

}

void service_discovery_proxy::stop() {

}

} // namespace sd
} // namespace vsomeip
