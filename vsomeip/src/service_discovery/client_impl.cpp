//
// udp_client_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/client.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/internal/udp_client_impl.hpp>
#include <vsomeip/service_discovery/factory.hpp>
#include <vsomeip/service_discovery/message.hpp>
#include <vsomeip/service_discovery/service_entry.hpp>
#include <vsomeip/service_discovery/internal/client_impl.hpp>
#include <vsomeip/service_discovery/internal/events.hpp>

#define SERVICE_DISCOVERY_SERVICE_ID 0xFFFF
#define SERVICE_DISCOVERY_METHOD_ID  0x8100

namespace vsomeip {
namespace service_discovery {

client_impl::client_impl(
		service_id _service_id, instance_id _instance_id,
	    major_version _major_version, time_to_live _time_to_live,
	    boost::asio::io_service *_is)
	: client_behavior_impl(*_is), is_(_is) {
	factory *default_factory = factory::get_default_factory();

	// TODO: read configuration to determine whether to use UDP or TCP
	endpoint *service_discovery_endpoint
		= default_factory->get_endpoint("127.0.0.1", 38223, ip_protocol::UDP, ip_version::V4);
	service_discovery_client_ = new vsomeip::udp_client_impl(default_factory, service_discovery_endpoint, *is_);

	service_discovery_client_->register_for(this,
								 	 	 	SERVICE_DISCOVERY_SERVICE_ID,
								 	 	 	SERVICE_DISCOVERY_METHOD_ID);

	service_id_ = _service_id;
	instance_id_ = _instance_id;
	major_version_ = _major_version;
	time_to_live_ = _time_to_live;
}

client_impl::~client_impl() {
	delete service_discovery_client_;
	delete delegate_;
}

void client_impl::start() {
	service_discovery_client_->start();
	client_behavior_impl::start();
	client_behavior_impl::process_event(ev_configuration_status_change(true));
	client_behavior_impl::process_event(ev_request_change(true));
}

void client_impl::stop() {
	service_discovery_client_->stop();
}

void client_impl::register_for(receiver *_receiver, service_id _service_id, method_id _method_id) {
	if (service_id_ == _service_id) {
		delegate_->register_for(_receiver, _service_id, _method_id);
	}
}

void client_impl::unregister_for(receiver *_receiver, service_id _service_id, method_id _method_id) {
	if (service_id_ == _service_id) {
		delegate_->unregister_for(_receiver, _service_id, _method_id);
	}
}

void client_impl::enable_magic_cookies() {
	delegate_->enable_magic_cookies();
}

void client_impl::disable_magic_cookies() {
	delegate_->enable_magic_cookies();
}

bool client_impl::send(const message_base *_message, bool _flush) {
	return delegate_->send(_message, _flush);
}

bool client_impl::send(const uint8_t *_data, uint32_t _length, bool _flush) {
	return delegate_->send(_data, _length, _flush);
}

bool client_impl::flush() {
	return delegate_->flush();
}

std::size_t client_impl::poll_one() {
	is_->poll_one();
}

std::size_t client_impl::poll() {
	is_->poll();
}

std::size_t client_impl::run() {
	is_->run();
}

void client_impl::receive(const message_base *_message) {
	const message *requests = dynamic_cast< const message * > (_message);
	if (0 != requests) {
		const std::vector<entry *>& entries = requests->get_entries();
		for (auto e : entries) {
			if (e->get_type() == entry_type::OFFER_SERVICE) {
				process_event(ev_offer_service());
			}
		}
	}
}

void client_impl::find_service() {
	message *m = factory::get_default_factory()->create_service_discovery_message();
	service_entry& s = m->create_service_entry();
	s.set_type(entry_type::FIND_SERVICE);
	s.set_service_id(service_id_);
	s.set_instance_id(instance_id_);
	s.set_major_version(major_version_);
	s.set_time_to_live(time_to_live_);
	service_discovery_client_->send(m);
}

} // namespace service_discovery
} // namespace vsomeip



