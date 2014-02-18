//
// service_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/service.hpp>
#include <vsomeip/service_discovery/factory.hpp>
#include <vsomeip/service_discovery/message.hpp>
#include <vsomeip/service_discovery/service_entry.hpp>
#include <vsomeip/service_discovery/internal/service_impl.hpp>
#include <vsomeip/service_discovery/internal/events.hpp>

#define SERVICE_DISCOVERY_SERVICE_ID 0xFFFF
#define SERVICE_DISCOVERY_METHOD_ID  0x8100

namespace vsomeip {
namespace service_discovery {

service_impl::service_impl(vsomeip::service *_delegate)
	: delegate_(_delegate) {
}

service_impl::~service_impl() {
	delete delegate_;
}

bool service_impl::register_service(service_id _service, instance_id _instance) {
	return true;
}

bool service_impl::unregister_service(service_id _service, instance_id _instance) {
	return true;
}

void service_impl::start() {
	delegate_->start();
}

void service_impl::stop() {
	delegate_->stop();
}

void service_impl::register_for(receiver *_receiver, service_id _service_id, method_id _method_id) {
	delegate_->register_for(_receiver, _service_id, _method_id);
}

void service_impl::unregister_for(receiver *_receiver, service_id _service_id, method_id _method_id) {
	delegate_->unregister_for(_receiver, _service_id, _method_id);
}

void service_impl::enable_magic_cookies() {
	delegate_->enable_magic_cookies();
}

void service_impl::disable_magic_cookies() {
	delegate_->enable_magic_cookies();
}

bool service_impl::send(const message_base *_message, bool _flush) {
	return delegate_->send(_message, _flush);
}

bool service_impl::send(const uint8_t *_data, uint32_t _length, endpoint *_target, bool _flush) {
	return delegate_->send(_data, _length, _target, _flush);
}

bool service_impl::flush(endpoint *_target) {
	return delegate_->flush(_target);
}

} // namespace service_discovery
} // namespace vsomeip



