//
// managed_proxy_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <cstring>

#include <boost/asio/placeholders.hpp>
#include <boost/bind.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip_internal/application_impl.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/constants.hpp>
#include <vsomeip_internal/deserializer.hpp>
#include <vsomeip_internal/enumeration_types.hpp>
#include <vsomeip_internal/managed_proxy_impl.hpp>
#include <vsomeip_internal/log_macros.hpp>
#include <vsomeip_internal/message_queue.hpp>
#include <vsomeip_internal/serializer.hpp>

namespace vsomeip {

managed_proxy_impl::managed_proxy_impl(application_base_impl &_owner)
	: administration_proxy_impl(_owner),
	  log_user(_owner),
	  proxy_base_impl(_owner) {
}

managed_proxy_impl::~managed_proxy_impl() {
}

void managed_proxy_impl::init() {
	administration_proxy_impl::init();
}

void managed_proxy_impl::start() {
	administration_proxy_impl::start();
}

void managed_proxy_impl::stop() {
	administration_proxy_impl::stop();
}

bool managed_proxy_impl::send(message_base *_message, bool _reliable, bool _flush) {
	if (0 == _message) {
		VSOMEIP_WARNING << "managed_proxy_impl::send: called without message object";
		return false;
	}

	instance_id instance = _message->get_instance_id();
	message_type_enum message_type = _message->get_message_type();
	client_id id = owner_.get_id();

	uint32_t message_size =
			VSOMEIP_STATIC_HEADER_SIZE
			+ _message->get_length()
			+ sizeof(instance)
			+ sizeof(_reliable)
			+ sizeof(_flush)
			+ VSOMEIP_PROTOCOL_OVERHEAD;

	boost::shared_ptr< serializer > its_serializer(owner_.get_serializer());
	bool is_successful(its_serializer->serialize(_message));
	if (!is_successful) {
		VSOMEIP_ERROR << "managed_proxy_impl::send: message serialization failed!";
		return false;
	}

	std::vector< uint8_t > message_data(message_size);

	// Start & End Tag
	std::memcpy(&message_data[0], &VSOMEIP_PROTOCOL_START_TAG, sizeof(VSOMEIP_PROTOCOL_START_TAG));
	std::memcpy(&message_data[message_size-4], &VSOMEIP_PROTOCOL_END_TAG, sizeof(VSOMEIP_PROTOCOL_END_TAG));

	// Sender & Message Type
	std::memcpy(&message_data[VSOMEIP_PROTOCOL_ID], &id, sizeof(id));

	if (0xFE & its_serializer->get_data()[2]) {
		message_data[VSOMEIP_PROTOCOL_COMMAND] = static_cast<uint8_t>(command_enum::SOMEIP_MESSAGE);
	} else {
		message_data[VSOMEIP_PROTOCOL_COMMAND] = static_cast<uint8_t>(command_enum::SOMEIP_FIELD);
	}

	// Payload length & Message
	message_size -= VSOMEIP_PROTOCOL_OVERHEAD;
	std::memcpy(&message_data[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], &message_size, sizeof(message_size));
	message_size = its_serializer->get_size();
	std::memcpy(&message_data[VSOMEIP_PROTOCOL_PAYLOAD], its_serializer->get_data(), message_size);
	its_serializer->reset();

	// Instance, Reliable & Flush Parameter
	std::memcpy(&message_data[VSOMEIP_PROTOCOL_PAYLOAD + message_size], &instance, sizeof(instance));
	message_data[VSOMEIP_PROTOCOL_PAYLOAD + message_size + sizeof(instance)] = static_cast< uint8_t >(_reliable);
	message_data[VSOMEIP_PROTOCOL_PAYLOAD + message_size + sizeof(instance) + sizeof(_reliable)] = static_cast< uint8_t >(_flush);

	send_buffers_.push_back(message_data);

	// Now we need to lock
	mutex_.lock();

	message_queue *target_queue = 0;
	if (message_type < message_type_enum::RESPONSE) {
		target_queue = find_target_queue(_message->get_service_id(), instance);
	} else {
		target_queue = find_target_queue(_message->get_client_id());
	}

	if (0 == target_queue)
		target_queue = daemon_queue_.get();

	if (target_queue != daemon_queue_.get() || is_open_) {
		std::vector< uint8_t >& current_message = send_buffers_.back();
		target_queue->async_send(
			current_message.data(),
			current_message.size(),
			0,
			boost::bind(
				&managed_proxy_impl::send_cbk,
				this,
				boost::asio::placeholders::error
			)
		);
	} else {
		VSOMEIP_FATAL << "No queue! Perhaps we need to do something now...";
	}
	mutex_.unlock();

	return true;
}

void managed_proxy_impl::send_cbk(boost::system::error_code const &_error) {
	if (_error) {
		VSOMEIP_ERROR << "Queue " << application_queue_name_
				      << ": Error while sending (" << _error.message() << ")";
	}
	send_buffers_.pop_front();
}

bool managed_proxy_impl::enable_magic_cookies(
			service_id _service, instance_id _instance) {
	return false;
}

bool managed_proxy_impl::disable_magic_cookies(
			service_id _service, instance_id _instance) {
	return false;
}

void managed_proxy_impl::on_message(client_id _id, const uint8_t *_data, uint32_t _size) {
	if (_size < 8) {
		return;
	}

	// deserialize the message
	uint32_t message_size = VSOMEIP_BYTES_TO_LONG(
								_data[4], _data[5], _data[6], _data[7]);

	boost::shared_ptr< deserializer > its_deserializer(owner_.get_deserializer());
	its_deserializer->set_data(_data, message_size + VSOMEIP_STATIC_HEADER_SIZE);
	std::shared_ptr< message > its_message(its_deserializer->deserialize_message());
	its_deserializer->reset();

	if (its_message) {
		instance_id its_instance;
		std::memcpy(&its_instance, &_data[message_size + VSOMEIP_STATIC_HEADER_SIZE], sizeof(its_instance));
		its_message->set_instance_id(its_instance);

		owner_.handle_message(its_message);
	}
}

void managed_proxy_impl::catch_up_registrations() {
	administration_proxy_impl::catch_up_registrations();
	owner_.catch_up_registrations();
}

void managed_proxy_impl::process_command(command_enum _command, client_id _client, const uint8_t *_payload, uint32_t _payload_size) {
	client_id its_client;

	switch (_command) {
	case command_enum::SOMEIP_MESSAGE:
		on_message(_client, _payload, _payload_size);
		break;

	default:
		administration_proxy_impl::process_command(_command, _client, _payload, _payload_size);
		break;
	}
}

message_queue * managed_proxy_impl::find_target_queue(service_id _service, instance_id _instance) const {
	message_queue *requested_queue = 0;

	auto find_service = requested_.find(_service);
	if (find_service != requested_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			requested_queue = find_instance->second.queue_.get();
		}
	}

	return requested_queue;
}

message_queue * managed_proxy_impl::find_target_queue(client_id _client) {
	message_queue *requested_queue = 0;

	auto found_queue_name = other_queue_names_.find(_client);
	if (found_queue_name != other_queue_names_.end()) {
		auto found_queue = queues_.find(found_queue_name->second);
		if (found_queue != queues_.end()) {
			requested_queue = found_queue->second;
		} else {
			requested_queue = new message_queue(owner_.get_sender_service(), this);
			queues_[found_queue_name->second] = requested_queue;
			requested_queue->async_open(
				queue_name_prefix_ + found_queue_name->second,
				boost::bind(
					&managed_proxy_impl::response_cbk,
					this,
					boost::asio::placeholders::error,
					requested_queue
				)
			);
		}
	}

	return requested_queue;
}

void managed_proxy_impl::response_cbk(
		boost::system::error_code const &_error,
		message_queue *_queue) {
}

} // namespace vsomeip
