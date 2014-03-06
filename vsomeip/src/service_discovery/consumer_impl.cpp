//
// consumer_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/consumer.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/internal/udp_consumer_impl.hpp>
#include <vsomeip/service_discovery/factory.hpp>
#include <vsomeip/service_discovery/message.hpp>
#include <vsomeip/service_discovery/service_entry.hpp>
#include <vsomeip/service_discovery/internal/consumer_impl.hpp>
#include <vsomeip/service_discovery/internal/events.hpp>

#define SERVICE_DISCOVERY_SERVICE_ID 0xFFFF
#define SERVICE_DISCOVERY_METHOD_ID  0x8100

namespace vsomeip {
namespace service_discovery {

consumer_impl::consumer_impl(
		service_id _service_id, instance_id _instance_id,
	    boost::asio::io_service &_is) {
};

consumer_impl::~consumer_impl() {
}



void consumer_impl::start() {

}

void consumer_impl::stop() {

}

void consumer_impl::register_for(vsomeip::receiver*, vsomeip::service_id, vsomeip::method_id) {

}

void consumer_impl::unregister_for(vsomeip::receiver*, vsomeip::service_id, vsomeip::method_id) {

}

void consumer_impl::enable_magic_cookies() {

}

void consumer_impl::disable_magic_cookies() {

}

bool consumer_impl::send(const vsomeip::message_base*, bool) {

}

bool consumer_impl::send(const uint8_t*, uint32_t, bool) {

}

bool consumer_impl::flush() {

}

} // namespace service_discovery
} // namespace vsomeip



