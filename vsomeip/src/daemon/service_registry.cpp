//
// service_registry.cpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <iomanip>

#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/internal/byteorder.hpp>
#include <vsomeip/service_discovery/internal/service_registry.hpp>

using boost::asio::local::stream_protocol;

namespace vsomeip {
namespace service_discovery {

service_registry::session::session(boost::asio::io_service &_is)
	: socket_(_is) {
}

stream_protocol::socket & service_registry::session::get_socket() {
	return socket_;
}

void service_registry::session::start() {
	socket_.async_read_some(boost::asio::buffer(data_),
			boost::bind(&session::receive,
						shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
}

void service_registry::session::receive(
		const boost::system::error_code &_error,
		   size_t _transferred_bytes) {

	if (!_error) {
		message_.insert(message_.end(), data_.c_array(), data_.c_array() + _transferred_bytes);
		consume_message();
		start();
	} else {
		// TODO: add log message here!
	}
}

service_registry::service_registry(
	boost::asio::io_service &_is, const std::string &_location)
	: is_(_is), acceptor_(_is, stream_protocol::endpoint(_location)) {

	session_ptr s(new session(is_));
	acceptor_.async_accept(
				s->get_socket(),
				boost::bind(&service_registry::handle_accept,
						    this,
						    s,
						    boost::asio::placeholders::error));
}

void service_registry::handle_accept(
								session_ptr _session,
								const boost::system::error_code &_error) {
	if (!_error) {
		_session->start();
		_session.reset(new session(is_));
		acceptor_.async_accept(
					_session->get_socket(),
					boost::bind(&service_registry::handle_accept,
							    this,
							    _session,
							    boost::asio::placeholders::error));
	}
}

void service_registry::session::consume_message() {
	int i = 0;
	bool is_complete = false;

	// Search start tag
	while (i+1 < message_.size()) {
		if (message_[i] == 0xFE && message_[i+1] == 0xED) {
			i += 2;
			break;
		}
		i++;
	}

	// Read command
	if (i+1 < message_.size()) {
		uint8_t command = message_[i++];
		switch (command) {
		case 0:
			if (i + 13 < message_.size()) {
				service_info info;
				info.service_ = VSOMEIP_BYTES_TO_WORD(message_[i], message_[i+1]);
				info.instance_ = VSOMEIP_BYTES_TO_WORD(message_[i+2], message_[i+3]);
				info.major_version_ = message_[i+4];
				info.minor_version_ = VSOMEIP_BYTES_TO_LONG(message_[i+5], message_[i+6],
											message_[i+7], message_[i+8]);
				info.time_to_live_ = VSOMEIP_BYTES_TO_LONG(
											0, message_[i+9],
											message_[i+10],	message_[i+11]);

				info.print();

				i += 14;
				is_complete = true;
			}
			break;
		default:
			// TODO: log "unknown command"
			break;
		}
	}

	if (is_complete)
		message_.erase(message_.begin(), message_.begin() + i);
}

service_info * service_registry::add(service_id _service, instance_id _instance) {
	service_info *found_service_instance = find(_service, _instance);
	if (found_service_instance)
		return found_service_instance;

	service_info s;
	s.service_ = _service;
	s.instance_ = _instance;
	std::map< instance_id, service_info >& found_service = data_[_service];
	found_service[_instance] = s;

	return &found_service[_instance];
}

void service_registry::remove(service_id _service, instance_id _instance) {
	auto found_service = data_.find(_service);
	data_.erase(found_service);
}

service_info * service_registry::find(service_id _service, instance_id _instance) {
	auto found_service = data_.find(_service);
	if (found_service == data_.end())
		return 0;

	if (_instance == 0xFFFF) {
		return &(found_service->second.begin()->second);
	}

	auto found_instance = found_service->second.find(_instance);
	if (found_instance == found_service->second.end())
		return 0;

	return &found_instance->second;
}

} // namespace service_discovery
} // namespace vsomeip
