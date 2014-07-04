// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/logger.hpp>

#include "../include/servicegroup.hpp"
#include "../include/service_discovery_host.hpp"
#include "../include/service_discovery_impl.hpp"

namespace vsomeip {
namespace sd {

service_discovery_impl::service_discovery_impl(service_discovery_host *_host)
	: host_(_host),
	  io_(_host->get_io()) {
}

service_discovery_impl::~service_discovery_impl() {
}

boost::asio::io_service & service_discovery_impl::get_io() {
	return io_;
}

void service_discovery_impl::init() {
	const std::set< std::shared_ptr< service_info > > &its_offers = host_->get_services();
}

void service_discovery_impl::start() {
}

void service_discovery_impl::stop() {
}

void service_discovery_impl::offer_service(service_t _service, instance_t _instance) {
	VSOMEIP_DEBUG << "sdi::offer_service [" << std::hex << _service << "." << _instance << "]";
}

void service_discovery_impl::stop_offer_service(service_t _service, instance_t _instance) {
	VSOMEIP_DEBUG << "sdi::stop_offer_service [" << std::hex << _service << "." << _instance << "]";
}

void service_discovery_impl::request_service(service_t _service, instance_t _instance,
		major_version_t _major, minor_version_t _minor, ttl_t _ttl) {
}

void service_discovery_impl::release_service(service_t _service, instance_t _instance) {

}
// Interface endpoint_host
void service_discovery_impl::on_message(const byte_t *_data, length_t _length, endpoint *_receiver) {

}

} // namespace sd
} // namespace vsomeip
