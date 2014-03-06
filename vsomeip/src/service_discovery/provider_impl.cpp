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

#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include <vsomeip/provider.hpp>
#include <vsomeip/internal/byteorder.hpp>
#include <vsomeip/service_discovery/factory.hpp>
#include <vsomeip/service_discovery/message.hpp>
#include <vsomeip/service_discovery/service_entry.hpp>
#include <vsomeip/service_discovery/internal/provider_impl.hpp>
#include <vsomeip/service_discovery/internal/events.hpp>

#define SERVICE_DISCOVERY_SERVICE_ID 0xFFFF
#define SERVICE_DISCOVERY_METHOD_ID  0x8100

namespace vsomeip {
namespace service_discovery {

provider_impl::provider_impl(vsomeip::provider *_delegate, boost::asio::io_service &_is)
	: delegate_(_delegate), socket_(_is) {
}

provider_impl::~provider_impl() {
	delete delegate_;
}

bool provider_impl::register_service(service_id _service, instance_id _instance) {
	auto found = services_.find(_service);
	if (found != services_.end()) {
		return (found->second.instance_ == _instance);
	}

	struct service_info info = {
		_service, _instance, 0xFF, 0xFFEEDDCC, 0xFFEEDD };
	services_[_service] = info;

	return true;
}

bool provider_impl::unregister_service(service_id _service, instance_id _instance) {
	services_.erase(_service);
	return true;
}

void provider_impl::start() {
	delegate_->start();

	connect();

	// announce all the services
	for (auto i : services_) {
		announce(i.second);
	}
}

void provider_impl::stop() {
	delegate_->stop();
}

void provider_impl::connect() {
	boost::asio::local::stream_protocol::endpoint registry("/tmp/vsomeipd");
	socket_.connect(registry);
}

void provider_impl::register_for(receiver *_receiver, service_id _service_id, method_id _method_id) {
	delegate_->register_for(_receiver, _service_id, _method_id);
}

void provider_impl::unregister_for(receiver *_receiver, service_id _service_id, method_id _method_id) {
	delegate_->unregister_for(_receiver, _service_id, _method_id);
}

void provider_impl::enable_magic_cookies() {
	delegate_->enable_magic_cookies();
}

void provider_impl::disable_magic_cookies() {
	delegate_->enable_magic_cookies();
}

bool provider_impl::send(const message_base *_message, bool _flush) {
	delegate_->send(_message, _flush);
}

bool provider_impl::send(const uint8_t *_data, uint32_t _length, endpoint *_target, bool _flush) {
	return delegate_->send(_data, _length, _target, _flush);
}

bool provider_impl::flush(endpoint *_target) {
	return delegate_->flush(_target);
}

void provider_impl::send_command(const uint8_t *_command, uint32_t _size) {
	std::vector< uint8_t> command;
	command.assign(_command, _command + _size);
	command_queue_.push_back(command);
	socket_.async_send(boost::asio::buffer(command_queue_.back()),
					   boost::bind(&provider_impl::sent_command,
							       this,
							       boost::asio::placeholders::error,
							       boost::asio::placeholders::bytes_transferred));
}

void provider_impl::sent_command(boost::system::error_code const &_error, std::size_t _bytes) {
	if (!_error) {
		command_queue_.pop_front();
	} else {
		// TODO: log "error while sending command"
	}
}

void provider_impl::announce(service_info &_info) {
	uint8_t command[] = { 0xFE, 0xED,
						  0x0, // Command
						  0x0, 0x0, // Service
						  0x0, 0x0, // Instance
						  0x0, // Major version
						  0x0, 0x0, 0x0, 0x0, // Minor Version
						  0x0, 0x0, 0x0, // Time to live
						  0xED, 0xDA };

	// On "wire" we use big endian
	command[3] = VSOMEIP_WORD_BYTE1(_info.service_);
	command[4] = VSOMEIP_WORD_BYTE0(_info.service_);
	command[5] = VSOMEIP_WORD_BYTE1(_info.instance_);
	command[6] = VSOMEIP_WORD_BYTE0(_info.instance_);
	command[7] = _info.major_version_;
	command[8] = VSOMEIP_LONG_BYTE3(_info.minor_version_);
	command[9] = VSOMEIP_LONG_BYTE2(_info.minor_version_);
	command[10] = VSOMEIP_LONG_BYTE1(_info.minor_version_);
	command[11] = VSOMEIP_LONG_BYTE0(_info.minor_version_);
	command[12] = VSOMEIP_LONG_BYTE2(_info.time_to_live_);
	command[13] = VSOMEIP_LONG_BYTE1(_info.time_to_live_);
	command[14] = VSOMEIP_LONG_BYTE0(_info.time_to_live_);

	send_command(command, sizeof(command));
}

} // namespace service_discovery
} // namespace vsomeip



