//
// application_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <algorithm>
#include <vector>

#include <boost/asio/placeholders.hpp>
#include <boost/atomic.hpp>
#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>
#include <boost/thread/mutex.hpp>

#include <boost_ext/asio/placeholders.hpp>
#include <boost_ext/process.hpp>

#include <vsomeip/config.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/factory.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip_internal/application_impl.hpp>
#include <vsomeip_internal/byteorder.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/constants.hpp>
#include <vsomeip_internal/deserializer_impl.hpp>
#include <vsomeip_internal/log_macros.hpp>
#include <vsomeip_internal/message_queue.hpp>
#include <vsomeip_internal/protocol.hpp>
#include <vsomeip_internal/serializer_impl.hpp>

using namespace boost::log::trivial;

//#define VSOMEIP_DEVEL
//#define WATCHDOG_TEST

namespace vsomeip {

///////////////////////////////////////////////////////////////////////////////
// Object members
///////////////////////////////////////////////////////////////////////////////
application_impl::application_impl(const std::string &_name)
	: application_base_impl(_name, send_service_),
	  id_(0),
	  receiver_thread_(0),
	  sender_thread_(0),
	  queue_name_prefix_("/vsomeip-"),
	  daemon_queue_name_("0"),
	  daemon_queue_(new message_queue(send_service_, this)),
	  application_queue_(new message_queue(receive_service_, this)),
	  watchdog_timer_(send_service_),
	  retry_timer_(send_service_),
	  retry_timeout_(20),
	  is_open_(false),
	  is_registered_(false) {

	serializer_ = new serializer_impl;
	serializer_->create_data(
					VSOMEIP_MAX_TCP_MESSAGE_SIZE);
	deserializer_ = new deserializer_impl;
}

application_impl::~application_impl() {
}

void application_impl::init(int _options_count, char **_options) {
	static boost::atomic< uint32_t  > queue_id(1);

	// configure application message queue name (use thread id)
	std::stringstream message_queue_id_stream;
	message_queue_id_stream
		<< (int)boost_ext::process::process_id()
		<< "."
		<< queue_id;

	// Message queue name
	application_queue_name_ = message_queue_id_stream.str();

	queue_id++;

	configuration::init(_options_count, _options);
	configuration * its_configuration = configuration::request(name_);

	configure_logging(
		its_configuration->use_console_logger(),
		its_configuration->use_file_logger(),
		its_configuration->use_dlt_logger()
	);

	set_loglevel(its_configuration->get_loglevel());

	set_channel(application_queue_name_);
	id_ = its_configuration->get_client_id();

	VSOMEIP_DEBUG << "Client-ID (configured): " << id_;
	VSOMEIP_DEBUG << "Queue name: " << queue_name_prefix_ << application_queue_name_;

	queues_[daemon_queue_name_] = daemon_queue_.get();
	queues_[application_queue_name_] = application_queue_.get();

	application_base_impl::init(_options_count, _options);
}

void application_impl::start() {
	configuration *vsomeip_configuration = configuration::request(name_);

	// Number of slots the message queue provides
	receiver_slots_ = vsomeip_configuration->get_receiver_slots();

	VSOMEIP_DEBUG
		<< name_ << " provides "
		<< receiver_slots_ << " receiver slots";

	daemon_queue_->async_open(
		queue_name_prefix_ + daemon_queue_name_,
		boost::bind(
			&application_impl::open_cbk,
			this,
			boost::asio::placeholders::error
		)
	);

	application_queue_->async_create(
		queue_name_prefix_ + application_queue_name_,
		receiver_slots_,
		VSOMEIP_QUEUE_SIZE,
		boost::bind(
			&application_impl::create_cbk,
			this,
			boost::asio::placeholders::error
		)
	);

	// start processing
	receiver_thread_.reset(
		new boost::thread(
				boost::bind(
					&application_impl::service,
					this,
					boost::ref(receive_service_)
				)
		)
	);

	sender_thread_.reset(
		new boost::thread(
				boost::bind(
					&application_impl::service,
					this,
					boost::ref(send_service_)
				)
		)
	);

	sender_thread_->join();
	receiver_thread_->join();
}

void application_impl::stop() {
	send_deregister_application();
	application_queue_->async_close(
		boost::bind(
			&application_impl::close_cbk,
			this,
			boost::asio::placeholders::error
		)
	);

	receive_service_.stop();
	send_service_.stop();

	receiver_thread_.reset();
	sender_thread_.reset();
}

void application_impl::service(boost::asio::io_service &_service) {
	_service.run();
}

bool application_impl::request_service(
			service_id _service, instance_id _instance,
			const endpoint *_location) {
	if (_location) {
		auto find_service = requested_.find(_service);
		if (find_service != requested_.end()) {
			auto find_location = find_service->second.find(_instance);
			if (find_location != find_service->second.end()) {
				if (find_location->second.first != _location) {
					if (id_ != 0) {
						send_service_command(
							command_enum::RELEASE_SERVICE,
							_service,
							_instance,
							find_location->second.first
						);
					}
					find_location->second = std::make_pair(_location, daemon_queue_.get());
				}
			} else {
				find_service->second[_instance] = std::make_pair(_location, daemon_queue_.get());
			}
		} else {
			requested_[_service][_instance] = std::make_pair(_location, daemon_queue_.get());
		}

		if (id_ != 0) {
			send_service_command(
				command_enum::REQUEST_SERVICE,
				_service,
				_instance,
				_location
			);
		}
		return true;
	} else {
		VSOMEIP_DEBUG
			<< "Specification of communication endpoint is missing.";
		return false;
	}
}

bool application_impl::release_service(
			service_id _service, instance_id _instance) {

	auto find_service = requested_.find(_service);
	if (find_service != requested_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			if (is_registered_) {
				send_service_command(
					command_enum::RELEASE_SERVICE,
					_service,
					_instance,
					find_instance->second.first
				);
			}
			find_service->second.erase(_instance);
		}
	}

	return true;
}

bool application_impl::provide_service(
	service_id _service, instance_id _instance,
	const endpoint *_location) {

	if (_location) {
		provided_[_service][_instance].second.insert(_location);

		if (is_registered_) {
			send_service_command(
				command_enum::PROVIDE_SERVICE,
				_service,
				_instance,
				_location
			);
		}
	} else {
		VSOMEIP_DEBUG
			<< "Specification of communication endpoint is missing.";
	}
	return true;
}

bool application_impl::withdraw_service(
	service_id _service, instance_id _instance,
	const endpoint *_location) {

	if (_location) {
		provided_[_service][_instance].second.erase(_location);
		if (provided_[_service][_instance].second.size() == 0)
			provided_[_service].erase(_instance);
	} else {
		provided_[_service].erase(_instance);
	}

	if (provided_[_service].size() == 0)
		provided_.erase(_service);

	if (is_registered_) {
		send_service_command(
			command_enum::WITHDRAW_SERVICE,
			_service,
			_instance,
			_location
		);
	}

	return true;
}

bool application_impl::start_service(
			service_id _service, instance_id _instance) {

	auto found_service = provided_.find(_service);
	if (found_service == provided_.end()) {
		return false;
	}

	auto found_instance = found_service->second.find(_instance);
	if (found_instance == found_service->second.end()) {
		return false;
	}

	found_instance->second.first = true;

	if (is_registered_) {
		send_service_command(
			command_enum::START_SERVICE,
			_service,
			_instance
		);
	}

	return true;
}

bool application_impl::stop_service(
			service_id _service, instance_id _instance) {

	auto found_service = provided_.find(_service);
	if (found_service == provided_.end()) {
		return false;
	}

	auto found_instance = found_service->second.find(_instance);
	if (found_instance == found_service->second.end()) {
		return false;
	}

	found_instance->second.first = false;

	if (is_registered_) {
		send_service_command(
			command_enum::STOP_SERVICE,
			_service,
			_instance
		);
	}

	return true;
}

bool application_impl::send(message_base *_message, bool _flush) {
	if (0 == _message) {
		VSOMEIP_DEBUG << "application::send: called without message object";
		return false;
	}

	instance_id instance = _message->get_instance_id();

	message_type_enum message_type = _message->get_message_type();
	if (message_type < message_type_enum::RESPONSE)
		_message->set_client_id(id_);

	uint32_t message_size =
			VSOMEIP_STATIC_HEADER_SIZE
			+ _message->get_length()
			+ sizeof(instance)
			+ sizeof(_flush)
			+ VSOMEIP_PROTOCOL_OVERHEAD;

	bool is_successful(serializer_->serialize(_message));
	if (!is_successful) {
		VSOMEIP_ERROR << "application::send: message serialization failed!";
		return false;
	}

	std::vector< uint8_t > message_data(message_size);

	// Start & End Tag
	std::memcpy(&message_data[0], &VSOMEIP_PROTOCOL_START_TAG, sizeof(VSOMEIP_PROTOCOL_START_TAG));
	std::memcpy(&message_data[message_size-4], &VSOMEIP_PROTOCOL_END_TAG, sizeof(VSOMEIP_PROTOCOL_END_TAG));

	// Sender & Message Type
	std::memcpy(&message_data[VSOMEIP_PROTOCOL_ID], &id_, sizeof(id_));
	message_data[VSOMEIP_PROTOCOL_COMMAND] = static_cast<uint8_t>(command_enum::SOMEIP_MESSAGE);

	// Payload length & Message
	message_size -= VSOMEIP_PROTOCOL_OVERHEAD;
	std::memcpy(&message_data[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], &message_size, sizeof(message_size));
	message_size = serializer_->get_size();
	std::memcpy(&message_data[VSOMEIP_PROTOCOL_PAYLOAD], serializer_->get_data(), message_size);
	serializer_->reset();

	// Instance & Flush Parameter
	std::memcpy(&message_data[VSOMEIP_PROTOCOL_PAYLOAD + message_size], &instance, sizeof(instance));
	message_data[VSOMEIP_PROTOCOL_PAYLOAD + message_size + sizeof(instance)] = static_cast< uint8_t >(_flush);

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
				&application_impl::send_cbk,
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

void application_impl::register_cbk(
		service_id _service, instance_id _instance, method_id _method, receive_cbk_t _cbk) {

	receive_cbks_[_service][_instance][_method].insert(_cbk);

	if (is_registered_) {
		send_callback_command(command_enum::REGISTER_METHOD, _service, _instance, _method, _cbk);
	}
}

void application_impl::deregister_cbk(
	service_id _service, instance_id _instance, method_id _method, receive_cbk_t _cbk) {

	auto found_service = receive_cbks_.find(_service);
	if (found_service != receive_cbks_.end()) {
		auto found_instance = found_service->second.find(_instance);
		if (found_instance != found_service->second.end()) {
			auto found_method = found_service->second.find(_method);
			if (found_method != found_service->second.end()) {
				found_method->second.erase(_method);

			}
		}
	}

	if (0 != id_) {
		send_callback_command(command_enum::DEREGISTER_METHOD, _service, _instance, _method, _cbk);
	}
}

void application_impl::enable_magic_cookies(
			service_id _service, instance_id _instance) {
}

void application_impl::disable_magic_cookies(
			service_id _service, instance_id _instance) {
}

///////////////////////////////////////////////////////////////////////////////
// Internal
///////////////////////////////////////////////////////////////////////////////
message_queue * application_impl::find_target_queue(service_id _service, instance_id _instance) const {
	message_queue *requested_queue = 0;

	auto find_service = requested_.find(_service);
	if (find_service != requested_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			requested_queue = find_instance->second.second.get();
		}
	}

	return requested_queue;
}

message_queue * application_impl::find_target_queue(client_id _id) {
	message_queue *requested_queue = 0;

	auto found_queue_name = other_queue_names_.find(_id);
	if (found_queue_name != other_queue_names_.end()) {
		auto found_queue = queues_.find(found_queue_name->second);
		if (found_queue != queues_.end()) {
			requested_queue = found_queue->second;
		} else {
			requested_queue = new message_queue(send_service_, this);
			queues_[found_queue_name->second] = requested_queue;
			requested_queue->async_open(
				queue_name_prefix_ + found_queue_name->second,
				boost::bind(
					&application_impl::response_cbk,
					this,
					boost::asio::placeholders::error,
					requested_queue
				)
			);
		}
	}

	return requested_queue;
}

void application_impl::remove_requested_services(message_queue *_queue) {
	std::map< service_id, std::set< instance_id > > served_by_queue;

	for (auto& i : requested_) {
		for (auto& j : i.second) {
			if (j.second.second == _queue) {
				j.second.second = 0;
			}
		}
	}
}

void application_impl::do_send(const std::vector< uint8_t > &_buffer) {
	send_buffers_.push_back(_buffer);
	if (is_open_)
		do_send_buffer(send_buffers_.back());
}

void application_impl::do_send_buffer(const std::vector< uint8_t > &_buffer) {
	daemon_queue_->async_send(
		_buffer.data(),
		_buffer.size(),
		0,
		boost::bind(
			&application_impl::send_cbk,
			this,
			boost::asio::placeholders::error
		)
	);
}

void application_impl::do_receive() {
	application_queue_->async_receive(
		receive_buffer_,
		sizeof(receive_buffer_),
		boost::bind(
			&application_impl::receive_cbk,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred,
			boost_ext::asio::placeholders::priority
		)
	);
}

void application_impl::send_register_application() {
	std::vector< uint8_t > registration;

	// Resize to the needed sizeid
	uint32_t payload_size = application_queue_name_.size();
	registration.resize(payload_size + VSOMEIP_PROTOCOL_OVERHEAD);

	std::memcpy(&registration[0], &VSOMEIP_PROTOCOL_START_TAG, sizeof(VSOMEIP_PROTOCOL_START_TAG));
	std::memcpy(&registration[VSOMEIP_PROTOCOL_ID], &id_, sizeof(id_));
	registration[VSOMEIP_PROTOCOL_COMMAND] = static_cast< uint8_t >(command_enum::REGISTER_APPLICATION);
	std::memcpy(&registration[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], &payload_size, sizeof(payload_size));
	std::copy(application_queue_name_.begin(), application_queue_name_.end(), registration.begin() + VSOMEIP_PROTOCOL_PAYLOAD);
	std::memcpy(&registration[payload_size + VSOMEIP_PROTOCOL_PAYLOAD], &VSOMEIP_PROTOCOL_END_TAG, sizeof(VSOMEIP_PROTOCOL_END_TAG));

	// send
	do_send(registration);
}

void application_impl::send_deregister_application() {
	uint8_t deregistration_message[] = {
		0x67, 0x37, 0x6D, 0x07,
		0x00, 0x00,
		static_cast< uint8_t >(command_enum::DEREGISTER_APPLICATION),
		0x00, 0x00, 0x00, 0x00,
		0x07, 0x6D, 0x37, 0x67
	};

	std::memcpy(&deregistration_message[VSOMEIP_PROTOCOL_ID], &id_, sizeof(id_));

	std::vector< uint8_t > deregistration_buffer(
		deregistration_message,
		deregistration_message + sizeof(deregistration_message)
	);

	// send
	do_send(deregistration_buffer);
}

void application_impl::send_callback_command(
		command_enum _command,
		service_id _service, instance_id _instance, method_id _method, receive_cbk_t _cbk) {

	uint8_t registration_message[] = {
		0x67, 0x37, 0x6D, 0x07,
		0x00, 0x00,
		static_cast< uint8_t >(_command),
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x07, 0x6D, 0x37, 0x67
	};

	uint32_t payload_size = 4;

	std::memcpy(&registration_message[VSOMEIP_PROTOCOL_ID], &id_, sizeof(id_));
	std::memcpy(&registration_message[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], &payload_size, sizeof(payload_size));
	std::memcpy(&registration_message[VSOMEIP_PROTOCOL_PAYLOAD], &_service, sizeof(_service));
	std::memcpy(&registration_message[VSOMEIP_PROTOCOL_PAYLOAD+2], &_instance, sizeof(_instance));
	std::memcpy(&registration_message[VSOMEIP_PROTOCOL_PAYLOAD+4], &_method, sizeof(_method));

	std::vector< uint8_t > registration_buffer(
		registration_message,
		registration_message + sizeof(registration_message)
	);

	do_send(registration_buffer);
}

void application_impl::send_service_command(
		command_enum _command,
		service_id _service,
		instance_id _instance,
		const endpoint *_location) {

	std::vector< uint8_t > command;

	uint32_t payload_size = 4;
	if (_location) {
		serializer_->serialize(reinterpret_cast<const serializable *>(_location));
		payload_size += serializer_->get_size();
	}

	command.resize(payload_size + VSOMEIP_PROTOCOL_OVERHEAD);

	std::memcpy(&command[0], &VSOMEIP_PROTOCOL_START_TAG, sizeof(VSOMEIP_PROTOCOL_START_TAG));
	std::memcpy(&command[VSOMEIP_PROTOCOL_ID], &id_, sizeof(id_));
	command[VSOMEIP_PROTOCOL_COMMAND] = static_cast<uint8_t>(_command);
	std::memcpy(&command[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], &payload_size, sizeof(payload_size));
	std::memcpy(&command[VSOMEIP_PROTOCOL_PAYLOAD], &_service, sizeof(_service));
	std::memcpy(&command[VSOMEIP_PROTOCOL_PAYLOAD+2], &_instance, sizeof(_instance));
	if (_location)
		std::memcpy(&command[VSOMEIP_PROTOCOL_PAYLOAD+4], serializer_->get_data(), serializer_->get_size());
	std::memcpy(&command[payload_size + VSOMEIP_PROTOCOL_PAYLOAD], &VSOMEIP_PROTOCOL_END_TAG, sizeof(VSOMEIP_PROTOCOL_END_TAG));

	serializer_->reset();

	do_send(command);
}

void application_impl::process_early_registrations() {
	for (auto s : provided_) {
		for (auto i : s.second) {
			for (auto l : i.second.second) {
				send_service_command(
					command_enum::PROVIDE_SERVICE,
					s.first,
					i.first,
					l
				);
			}
		}
	}

	for (auto s : provided_) {
		for (auto i : s.second) {
			if (i.second.first) {
				send_service_command(
					command_enum::START_SERVICE,
					s.first,
					i.first
				);
			}
		}
	}

	for (auto s : requested_) {
		for (auto i : s.second) {
			send_service_command(
				command_enum::REQUEST_SERVICE,
				s.first,
				i.first,
				i.second.first
			);
		}
	}

	for (auto s : receive_cbks_) {
		for (auto i : s.second) {
			for (auto m : i.second) {
				for (auto c : m.second) {
					send_callback_command(
							command_enum::REGISTER_METHOD,
							s.first,
							i.first,
							m.first,
							c);
				}
			}
		}
	}
}


void application_impl::on_application_info(const uint8_t *_data, uint32_t _size) {
	uint32_t position = 0;
	while (position + 4 < _size) {
		client_id id;
		std::memcpy(&id, &_data[position], sizeof(id));
		position += 4;

		VSOMEIP_DEBUG << "Client-ID (provided, accepted): " << id;

		if (position + 4 < _size) {
			uint32_t queue_name_size;
			std::memcpy(&queue_name_size, &_data[position], sizeof(queue_name_size));
			position += 4;

			if (position + queue_name_size <= _size) {
				std::string queue_name((char *)&_data[position], queue_name_size);

				if (queue_name == application_queue_name_) {
					if (id_ != 0) {
						if (id_ != id) {
							VSOMEIP_ERROR << "Found my (" << id_ << ") queue ("
								<< queue_name << ") registered for application " << id;
						}
					} else {
						id_ = id;
					}
				} else {
					VSOMEIP_DEBUG << "Found queue " << queue_name << " for application " << id;
					other_queue_names_[id] = queue_name;
				}
				position += queue_name_size;
			}
		} else {
			VSOMEIP_ERROR << "Message contains illegal queue name.";
			return;
		}
	}

	is_registered_ = true;
}

void application_impl::on_application_lost(const uint8_t *_data, uint32_t _size) {
	uint32_t position = 0;
	while (position + sizeof(client_id) <= _size) {
		client_id id;
		std::memcpy(&id, &_data[position], sizeof(id));
		position += 4;

		if (id == id_) {
			VSOMEIP_ERROR << "Daemon reports me as gone lost.";
		} else {
			VSOMEIP_DEBUG << "Client " << id << " got lost. Removing its queue reference.";

			mutex_.lock();
			auto find_queue_name = other_queue_names_.find(id);
			if (find_queue_name != other_queue_names_.end()) {
				auto find_queue = queues_.find(find_queue_name->second);
				if (find_queue != queues_.end()) {
					remove_requested_services(find_queue->second);
					queues_.erase(find_queue);
				}

				other_queue_names_.erase(find_queue_name);
			}
			mutex_.unlock();
		}
	}
}

void application_impl::on_request_service_ack(service_id _service, instance_id _instance, const std::string &_queue_name) {
	auto find_queue = queues_.find(_queue_name);
	if (find_queue != queues_.end()) {
		auto find_service = requested_.find(_service);
		if (find_service != requested_.end()) {
			auto find_instance = find_service->second.find(_instance);
			if (find_instance != find_service->second.end()) {
				find_instance->second.second.reset(find_queue->second);
			}
		}
	} else {
		message_queue *queue = new message_queue(send_service_, this);
		queues_[_queue_name] = queue;
		queue->async_open(
			queue_name_prefix_ + _queue_name,
			boost::bind(
				&application_impl::request_cbk,
				this,
				boost::asio::placeholders::error,
				_service,
				_instance,
				queue,
				_queue_name
			)
		);
	}
}

void application_impl::on_message(client_id _id, const uint8_t *_data, uint32_t _size) {
	if (_size < 8) {
		return;
	}

	// deserialize the message
	uint32_t message_size = VSOMEIP_BYTES_TO_LONG(
								_data[4], _data[5], _data[6], _data[7]);

	deserializer_->set_data(_data, message_size + VSOMEIP_STATIC_HEADER_SIZE);
	message_base *the_message = deserializer_->deserialize_message();
	deserializer_->reset();

	if (0 == the_message) {
		return;
	}

	// deserialize instance
	instance_id its_instance;
	std::memcpy(&its_instance, &_data[message_size + VSOMEIP_STATIC_HEADER_SIZE], sizeof(its_instance));
	the_message->set_instance_id(its_instance);

	// forward to registered callbacks
	service_id service = the_message->get_service_id();
	auto found_service = receive_cbks_.find(service);
	if (found_service != receive_cbks_.end()) {
		auto found_instance = found_service->second.find(its_instance);
		if (found_instance != found_service->second.end()) {
			method_id method = the_message->get_method_id();
			auto found_method = found_instance->second.find(method);
			if (found_method == found_instance->second.end()) {
				found_method = found_instance->second.find(VSOMEIP_ANY_METHOD);
			}

			if (found_method != found_instance->second.end()) {
				std::for_each(
					found_method->second.begin(),
					found_method->second.end(),
					[the_message](receive_cbk_t _func) {
						_func(the_message);
					}
				);
			} else {
				VSOMEIP_DEBUG << "Method not found";
				message_type_enum message_type = the_message->get_message_type();
				if (message_type < message_type_enum::RESPONSE) {
					//send_error_message(the_message, return_code_enum::UNKNOWN_METHOD);
				}
			}
		}
	} else {
		VSOMEIP_DEBUG << "Service not found";
		message_type_enum message_type = the_message->get_message_type();
		if (message_type < message_type_enum::RESPONSE) {
			//send_error_message(the_message, return_code_enum::UNKNOWN_SERVICE);
		}
	}
}

void application_impl::send_pong() {
#ifdef WATCHDOG_TEST
	static int pong_counter = 0;
	if (pong_counter > 20 && 0 == id_ % 3) {
		return;
	}

	pong_counter ++;
#endif

	uint8_t pong_message[] = {
		0x67, 0x37, 0x6D, 0x07,
		0x00, 0x00,
		static_cast<uint8_t>(command_enum::PONG),
		0x00, 0x00, 0x00, 0x00,
		0x07, 0x6D, 0x37, 0x67
	};

	std::memcpy(&pong_message[VSOMEIP_PROTOCOL_ID], &id_, sizeof(id_));

	std::vector< uint8_t > pong_buffer(
		pong_message,
		pong_message + sizeof(pong_message)
	);

	do_send(pong_buffer);
}

void application_impl::process_message(std::size_t _bytes) {
	if (_bytes < VSOMEIP_PROTOCOL_OVERHEAD) {
		VSOMEIP_ERROR << "Message too short (< " << VSOMEIP_PROTOCOL_OVERHEAD << " bytes)";
		return;
	}

	uint32_t start_tag, end_tag, payload_size;
	client_id a_client;
	service_id a_service;
	instance_id an_instance;
	command_enum command;

#ifdef VSOMEIP_DEVEL
		for (std::size_t i = 0; i < _bytes; ++i) {
			std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)receive_buffer_[i] << " ";
		}
		std::cout << std::endl;
#endif

	std::memcpy(&start_tag, &receive_buffer_[0], sizeof(start_tag));
	std::memcpy(&end_tag, &receive_buffer_[_bytes-4], sizeof(end_tag));

	std::memcpy(&payload_size, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], sizeof(payload_size));
	std::memcpy(&a_client, &receive_buffer_[VSOMEIP_PROTOCOL_ID], sizeof(a_client));
	command = static_cast<command_enum>(receive_buffer_[VSOMEIP_PROTOCOL_COMMAND]);

	if (start_tag == VSOMEIP_PROTOCOL_START_TAG && end_tag == VSOMEIP_PROTOCOL_END_TAG) {
		if (_bytes == payload_size + VSOMEIP_PROTOCOL_OVERHEAD) {

			switch (command) {
			case command_enum::APPLICATION_INFO:
				on_application_info(&receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], payload_size);
				process_early_registrations();
				break;

			case command_enum::APPLICATION_LOST:
				on_application_lost(&receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], payload_size);
				break;

			case command_enum::REQUEST_SERVICE_ACK:
				{
					std::memcpy(&a_service, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], sizeof(a_service));
					std::memcpy(&an_instance, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+2], sizeof(an_instance));
					std::string queue_name((char*) &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+4], payload_size - 4);
					on_request_service_ack(a_service, an_instance, queue_name);
				}
				break;

			case command_enum::PING:
				send_pong();
				break;

			case command_enum::SOMEIP_MESSAGE:
				on_message(a_client, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], payload_size);
				break;

			default:
				VSOMEIP_ERROR << "Message contains illegal command " << (int)command;
				break;
			}

		} else {
			VSOMEIP_ERROR
				<< "Message has incorrect size ("
				<< _bytes << "/" << payload_size + VSOMEIP_PROTOCOL_OVERHEAD << ")";
		}
	} else {
		VSOMEIP_ERROR << "Message is not correctly tagged";
	}
}

void application_impl::remove_queue(const std::string &_name) {
	VSOMEIP_ERROR << "Removing queue " << _name;
	queues_.erase(_name);
}

///////////////////////////////////////////////////////////////////////////////
// Callbacks
///////////////////////////////////////////////////////////////////////////////
void application_impl::open_cbk(boost::system::error_code const &_error) {
	if (!_error) {
		is_open_ = true;
		retry_timeout_ = 20;
		send_register_application();
		do_receive();
	} else {
		VSOMEIP_DEBUG << "Trying to open daemon queue in " << retry_timeout_ << "ms";
		retry_timer_.expires_from_now(
			std::chrono::milliseconds(retry_timeout_));
		retry_timer_.async_wait(
			boost::bind(
				&application_impl::retry_open_cbk,
				this,
				boost::asio::placeholders::error
			)
		);

		// next time we wait longer
		retry_timeout_ <<= 1;
	}
}

void application_impl::retry_open_cbk(boost::system::error_code const &_error) {

	if (!_error) {
		daemon_queue_->async_open(
			queue_name_prefix_ + daemon_queue_name_,
			boost::bind(
				&application_impl::open_cbk,
				this,
				boost::asio::placeholders::error
			)
		);
	}
}

void application_impl::create_cbk(
		boost::system::error_code const &_error) {
	if (_error) {
		// Try destroying before creating
		application_queue_->async_close(
			boost::bind(
				&application_impl::close_cbk,
				this,
				boost::asio::placeholders::error
			)
		);

		// TODO: define maximum number of retries
		application_queue_->async_create(
			queue_name_prefix_ + application_queue_name_,
			receiver_slots_,
			VSOMEIP_QUEUE_SIZE,
			boost::bind(
				&application_impl::create_cbk,
				this,
				boost::asio::placeholders::error
			)
		);
	}
}

void application_impl::close_cbk(
		boost::system::error_code const &_error) {
	if (!_error) {
		id_ = 0;
	}
}

void application_impl::send_cbk(
		boost::system::error_code const &_error) {
	if (_error) {
		VSOMEIP_ERROR << "Application " << application_queue_name_
					  << ": Error while sending (" << _error.message() << ")";
	}

	send_buffers_.pop_front();
}

void application_impl::retry_send_cbk(
		boost::system::error_code const &_error) {
	if (!_error) {
		do_send_buffer(send_buffers_.front());
	}
}

void application_impl::receive_cbk(
		boost::system::error_code const &_error,
		std::size_t _bytes, unsigned int _priority) {
	if (!_error && _bytes) {
		process_message(_bytes);
	}

	do_receive();
}

void application_impl::request_cbk(
		boost::system::error_code const &_error,
		service_id _service, instance_id _instance,
		message_queue *_queue, const std::string &_queue_name) {

	if (!_error) {
		VSOMEIP_DEBUG
			<< "APPLICATION " << id_ << " opened queue for service ("
			<< std::hex << std::setw(4) << std::setfill('0')
			<< _service << " " << _instance << ")";

		auto find_service = requested_.find(_service);
		if (find_service != requested_.end()) {
			auto find_instance = find_service->second.find(_instance);
			if (find_instance != find_service->second.end()) {
				find_instance->second.second.reset(_queue);
			} else {
				VSOMEIP_ERROR << "Application " << application_queue_name_
						      << ": Requested service instance ["
						      << std::hex << _service << "." << _instance
						      << "] is unknown.";
			}
		} else {
			VSOMEIP_ERROR << "Application " << application_queue_name_
					      << ": Requested service ["
					      << std::hex << _service
					      << "] is unknown.";
		}
	} else {
		VSOMEIP_ERROR << "Application " << application_queue_name_
				      << ": Error while opening queue ["
				      << _queue_name << "]";
	}
}

void application_impl::response_cbk(
		boost::system::error_code const &_error,
		message_queue *_queue) {
}

///////////////////////////////////////////////////////////////////////////////
// Helper function to automate queue handling
///////////////////////////////////////////////////////////////////////////////
void intrusive_ptr_add_ref(vsomeip::message_queue *_queue) {
	if (_queue) {
		_queue->add_ref();
	}
}

void intrusive_ptr_release(vsomeip::message_queue *_queue) {
	if (_queue) {
		_queue->release();
		if (0 == _queue->get_ref() && 0 != _queue->get_application()) {
			_queue->get_application()->remove_queue(_queue->get_name());
			delete _queue;
		}
	}
}

} // namespace vsomeip
