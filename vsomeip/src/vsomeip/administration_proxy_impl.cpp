//
// administration_proxy_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <cstring>

#include <boost/asio/placeholders.hpp>

#include <boost_ext/asio/placeholders.hpp>
#include <boost_ext/process.hpp>

#include <vsomeip_internal/administration_proxy_impl.hpp>
#include <vsomeip_internal/application_impl.hpp>
#include <vsomeip_internal/configuration.hpp>
#include <vsomeip_internal/enumeration_types.hpp>
#include <vsomeip_internal/log_macros.hpp>

namespace vsomeip {

administration_proxy_impl::administration_proxy_impl(application_base_impl &_owner)
	: proxy_base_impl(_owner),
	  log_user(_owner),
	  daemon_queue_(new message_queue(_owner.get_sender_service(), this)),
	  application_queue_(new message_queue(_owner.get_receiver_service(), this)),
	  queue_name_prefix_("/vsomeip-"),
	  daemon_queue_name_("0"),
	  retry_timer_(_owner.get_sender_service()),
	  retry_timeout_(VSOMEIP_DEFAULT_RETRY_TIMEOUT),
	  is_registered_(false),
	  is_open_(false),
	  is_created_(false) {
}

administration_proxy_impl::~administration_proxy_impl() {

}

void administration_proxy_impl::init() {
	VSOMEIP_TRACE << "administration_proxy_impl::init";
	static boost::atomic< uint32_t  > queue_id(1);

	// configure application message queue name (use thread id)
	std::stringstream message_queue_id_stream;
	message_queue_id_stream
		<< (int)boost_ext::process::process_id()
		<< "."
		<< queue_id;

	// Message queue name
	application_queue_name_ = message_queue_id_stream.str();

	VSOMEIP_DEBUG << "Client-ID (configured): " << owner_.get_id();
	VSOMEIP_DEBUG << "Using queue \"" << queue_name_prefix_ << application_queue_name_ << "\"";

	queue_id++;
}

void administration_proxy_impl::start() {
	VSOMEIP_TRACE << "administration_proxy_impl::start";
	configuration *vsomeip_configuration = configuration::request(owner_.get_name());

	// Number of slots the message queue provides
	int slots = vsomeip_configuration->get_slots();

	VSOMEIP_DEBUG
		<< owner_.get_name() << " provides "
		<< slots << " message slots";

	daemon_queue_->async_open(
		queue_name_prefix_ + daemon_queue_name_,
		boost::bind(
			&administration_proxy_impl::open_cbk,
			this,
			boost::asio::placeholders::error
		)
	);

	application_queue_->async_create(
		queue_name_prefix_ + application_queue_name_,
		slots,
		VSOMEIP_DEFAULT_QUEUE_SIZE,
		boost::bind(
			&administration_proxy_impl::create_cbk,
			this,
			boost::asio::placeholders::error
		)
	);
}

void administration_proxy_impl::stop() {
}

bool administration_proxy_impl::request_service(
			service_id _service, instance_id _instance,
			const endpoint *_location) {
	VSOMEIP_TRACE << "administration_proxy_impl::request_service";

	bool is_requested = true;

	if (_location) {
		auto find_service = requested_.find(_service);
		if (find_service != requested_.end()) {
			auto find_location = find_service->second.find(_instance);
			if (find_location != find_service->second.end()) {
				if (find_location->second.first != _location) {
					if (owner_.get_id() != 0) {
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

		if (is_registered_) {
			send_service_command(
				command_enum::REQUEST_SERVICE,
				_service,
				_instance,
				_location
			);
		}

	} else {
		VSOMEIP_ERROR << "Request for service ["
					  << std::hex << _service << "." << _instance
					  << "] is missing location (and SD is inactive)";
		is_requested = false;
	}

	return is_requested;
}

bool administration_proxy_impl::release_service(
			service_id _service, instance_id _instance) {

	VSOMEIP_TRACE << "administration_proxy_impl::release_service";

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

bool administration_proxy_impl::provide_service(
	service_id _service, instance_id _instance,
	const endpoint *_location) {

	VSOMEIP_TRACE << "administration_proxy_impl::provide_service";

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

bool administration_proxy_impl::withdraw_service(
	service_id _service, instance_id _instance,
	const endpoint *_location) {

	VSOMEIP_TRACE << "administration_proxy_impl::withdraw_service";

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

bool administration_proxy_impl::start_service(
			service_id _service, instance_id _instance) {

	VSOMEIP_TRACE << "administration_proxy_impl::start_service";

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

bool administration_proxy_impl::stop_service(
			service_id _service, instance_id _instance) {

	VSOMEIP_TRACE << "administration_proxy_impl::stop_service";

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

void administration_proxy_impl::catch_up_registrations() {
	VSOMEIP_TRACE << "administration_proxy_impl::catch_up_registrations";

	for (auto s : provided_) {
		for (auto i : s.second) {
			for (auto l : i.second.second) {
				provide_service(s.first, i.first, l);
			}
		}
	}

	for (auto s : provided_) {
		for (auto i : s.second) {
			if (i.second.first) {
				start_service(s.first, i.first);
			}
		}
	}

	for (auto s : requested_) {
		for (auto i : s.second) {
			request_service(s.first, i.first, i.second.first);
		}
	}
}

void administration_proxy_impl::do_send(const std::vector< uint8_t > &_buffer) {
	VSOMEIP_TRACE << "administration_proxy_impl::do_send";
	send_buffers_.push_back(_buffer);
	if (is_open_)
		do_send_buffer(send_buffers_.back());
}

void administration_proxy_impl::do_send_buffer(const std::vector< uint8_t > &_buffer) {
	VSOMEIP_TRACE << "administration_proxy_impl::do_send_buffer";
	daemon_queue_->async_send(
		_buffer.data(),
		_buffer.size(),
		0,
		boost::bind(
			&administration_proxy_impl::send_cbk,
			this,
			boost::asio::placeholders::error
		)
	);
}

void administration_proxy_impl::do_receive() {
	VSOMEIP_TRACE << "administration_proxy_impl::do_receive";
	application_queue_->async_receive(
		receive_buffer_,
		sizeof(receive_buffer_),
		boost::bind(
			&administration_proxy_impl::receive_cbk,
			this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred,
			boost_ext::asio::placeholders::priority
		)
	);
}

void administration_proxy_impl::send_register_application() {
	VSOMEIP_TRACE << "administration_proxy_impl::send_register_application";
	std::vector< uint8_t > registration;

	// Resize to the needed sizeid
	uint32_t payload_size = application_queue_name_.size() + 1; // "+ 1" for "is_managed"
	registration.resize(payload_size + VSOMEIP_PROTOCOL_OVERHEAD);

	client_id id = owner_.get_id();

	std::memcpy(&registration[0], &VSOMEIP_PROTOCOL_START_TAG, sizeof(VSOMEIP_PROTOCOL_START_TAG));
	std::memcpy(&registration[VSOMEIP_PROTOCOL_ID], &id, sizeof(id));
	registration[VSOMEIP_PROTOCOL_COMMAND] = static_cast< uint8_t >(command_enum::REGISTER_APPLICATION);
	std::memcpy(&registration[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], &payload_size, sizeof(payload_size));
	std::copy(application_queue_name_.begin(), application_queue_name_.end(), registration.begin() + VSOMEIP_PROTOCOL_PAYLOAD);
	registration[VSOMEIP_PROTOCOL_PAYLOAD + payload_size - 1] = static_cast< uint8_t >(owner_.is_managing());
	std::memcpy(&registration[payload_size + VSOMEIP_PROTOCOL_PAYLOAD], &VSOMEIP_PROTOCOL_END_TAG, sizeof(VSOMEIP_PROTOCOL_END_TAG));

	// send
	do_send(registration);
}

void administration_proxy_impl::send_deregister_application() {
	VSOMEIP_TRACE << "administration_proxy_impl::send_deregister_application";
	uint8_t deregistration_message[] = {
		0x67, 0x37, 0x6D, 0x07,
		0x00, 0x00,
		static_cast< uint8_t >(command_enum::DEREGISTER_APPLICATION),
		0x00, 0x00, 0x00, 0x00,
		0x07, 0x6D, 0x37, 0x67
	};

	client_id id = owner_.get_id();
	std::memcpy(&deregistration_message[VSOMEIP_PROTOCOL_ID], &id, sizeof(id));

	std::vector< uint8_t > deregistration_buffer(
		deregistration_message,
		deregistration_message + sizeof(deregistration_message)
	);

	// send
	do_send(deregistration_buffer);
}

void administration_proxy_impl::send_service_command(
		command_enum _command,  service_id _service, instance_id _instance, const endpoint * _location) {
	VSOMEIP_TRACE << "administration_proxy_impl::send_service_command";

	std::vector< uint8_t > command;

	client_id id = owner_.get_id();
	boost::shared_ptr< serializer > its_serializer(owner_.get_serializer());
	uint32_t payload_size = 4;
	if (_location) {
		its_serializer->serialize(reinterpret_cast<const serializable *>(_location));
		payload_size += its_serializer->get_size();
	}

	command.resize(payload_size + VSOMEIP_PROTOCOL_OVERHEAD);

	std::memcpy(&command[0], &VSOMEIP_PROTOCOL_START_TAG, sizeof(VSOMEIP_PROTOCOL_START_TAG));
	std::memcpy(&command[VSOMEIP_PROTOCOL_ID], &id, sizeof(id));
	command[VSOMEIP_PROTOCOL_COMMAND] = static_cast<uint8_t>(_command);
	std::memcpy(&command[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], &payload_size, sizeof(payload_size));
	std::memcpy(&command[VSOMEIP_PROTOCOL_PAYLOAD], &_service, sizeof(_service));
	std::memcpy(&command[VSOMEIP_PROTOCOL_PAYLOAD+2], &_instance, sizeof(_instance));
	if (_location)
		std::memcpy(&command[VSOMEIP_PROTOCOL_PAYLOAD+4], its_serializer->get_data(), its_serializer->get_size());
	std::memcpy(&command[payload_size + VSOMEIP_PROTOCOL_PAYLOAD], &VSOMEIP_PROTOCOL_END_TAG, sizeof(VSOMEIP_PROTOCOL_END_TAG));

	its_serializer->reset();

	do_send(command);
}

void administration_proxy_impl::send_pong() {
	VSOMEIP_TRACE << "administration_proxy_impl::send_pong";
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

	client_id id = owner_.get_id();
	std::memcpy(&pong_message[VSOMEIP_PROTOCOL_ID], &id, sizeof(id));

	std::vector< uint8_t > pong_buffer(
		pong_message,
		pong_message + sizeof(pong_message)
	);

	do_send(pong_buffer);
}

void administration_proxy_impl::process_message(std::size_t _bytes) {
	VSOMEIP_TRACE << "administration_proxy_impl::process_message";
#ifdef VSOMEIP_DEVEL
		for (std::size_t i = 0; i < _bytes; ++i) {
			std::cout << std::setw(2) << std::setfill('0') << std::hex << (int)receive_buffer_[i] << " ";
		}
		std::cout << std::endl;
#endif

	if (_bytes < VSOMEIP_PROTOCOL_OVERHEAD) {
		VSOMEIP_ERROR << "Message too short (< " << VSOMEIP_PROTOCOL_OVERHEAD << " bytes)";
		return;
	}

	uint32_t start_tag, end_tag, payload_size;
	command_enum command;
	client_id its_client;

	std::memcpy(&start_tag, &receive_buffer_[0], sizeof(start_tag));
	std::memcpy(&end_tag, &receive_buffer_[_bytes-4], sizeof(end_tag));
	command = static_cast< command_enum >(receive_buffer_[VSOMEIP_PROTOCOL_COMMAND]);
	std::memcpy(&payload_size, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD_SIZE], sizeof(payload_size));

	if (start_tag == VSOMEIP_PROTOCOL_START_TAG && end_tag == VSOMEIP_PROTOCOL_END_TAG) {
		if (_bytes == payload_size + VSOMEIP_PROTOCOL_OVERHEAD) {

			std::memcpy(&its_client, &receive_buffer_[VSOMEIP_PROTOCOL_ID], sizeof(its_client));
			process_command(command, its_client, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], payload_size);

		} else {
			VSOMEIP_ERROR
				<< "Message has incorrect size ("
				<< _bytes << "/" << payload_size + VSOMEIP_PROTOCOL_OVERHEAD << ")";
		}
	} else {
		VSOMEIP_ERROR << "Message is not correctly tagged";
	}
}

void administration_proxy_impl::process_command(command_enum _command, client_id _client, const uint8_t *_payload, uint32_t payload_size) {
	service_id its_service;
	instance_id its_instance;

	switch (_command) {
	case command_enum::APPLICATION_INFO:
		on_application_info(&receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], payload_size);
		catch_up_registrations();
		break;

	case command_enum::APPLICATION_LOST:
		on_application_lost(&receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], payload_size);
		break;

	case command_enum::REQUEST_SERVICE_ACK:
		std::memcpy(&its_service, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD], sizeof(its_service));
		std::memcpy(&its_instance, &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+2], sizeof(its_instance));
		on_request_service_ack(its_service, its_instance,
			std::string((char*) &receive_buffer_[VSOMEIP_PROTOCOL_PAYLOAD+4], payload_size - 4));
		break;

	case command_enum::PING:
		send_pong();
		break;

	default:
		VSOMEIP_ERROR << "Message contains illegal command " << (int)_command;
		break;
	}
}

void administration_proxy_impl::on_application_info(const uint8_t *_data, uint32_t _size) {
	VSOMEIP_TRACE << "administration_proxy_impl::on_application_info";
	uint32_t position = 0;
	while (position + 4 < _size) {
		client_id configured_id = owner_.get_id();
		client_id provided_id;
		std::memcpy(&provided_id, &_data[position], sizeof(provided_id));
		position += 4;

		VSOMEIP_DEBUG << "Client-ID (from daemon): " << provided_id;

		if (position + 4 < _size) {
			uint32_t queue_name_size;
			std::memcpy(&queue_name_size, &_data[position], sizeof(queue_name_size));
			position += 4;

			if (position + queue_name_size <= _size) {
				std::string queue_name((char *)&_data[position], queue_name_size);

				if (queue_name == application_queue_name_) {
					if (configured_id != 0) {
						if (configured_id != provided_id) {
							VSOMEIP_ERROR << "Found my (" << configured_id << ") queue ("
								<< queue_name << ") registered for application " << provided_id;
						}
					} else {
						owner_.set_id(provided_id);
					}
				} else {
					VSOMEIP_DEBUG << "Found queue " << queue_name << " for application " << provided_id;
					other_queue_names_[provided_id] = queue_name;
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

void administration_proxy_impl::on_application_lost(const uint8_t *_data, uint32_t _size) {
	VSOMEIP_TRACE << "administration_proxy_impl::on_application_lost";
	uint32_t position = 0;
	while (position + sizeof(client_id) <= _size) {
		client_id id;
		std::memcpy(&id, &_data[position], sizeof(id));
		position += 4;

		if (id == owner_.get_id()) {
			VSOMEIP_FATAL << "Daemon reported me as gone lost!";
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

void administration_proxy_impl::on_request_service_ack(service_id _service, instance_id _instance, const std::string &_queue_name) {
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
		message_queue *queue = new message_queue(owner_.get_sender_service(), this);
		queues_[_queue_name] = queue;
		queue->async_open(
			queue_name_prefix_ + _queue_name,
			boost::bind(
				&administration_proxy_impl::request_cbk,
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

void administration_proxy_impl::remove_requested_services(message_queue *_queue) {
	VSOMEIP_TRACE << "administration_proxy_impl::remove_requested_services";
	std::map< service_id, std::set< instance_id > > served_by_queue;

	for (auto& i : requested_) {
		for (auto& j : i.second) {
			if (j.second.second == _queue) {
				j.second.second = 0;
			}
		}
	}
}

void administration_proxy_impl::open_cbk(boost::system::error_code const &_error) {
	VSOMEIP_TRACE << "administration_proxy_impl::open_cbk";
	if (!_error) {
		is_open_ = true;
		retry_timeout_ = 20;
		if (is_created_) {
			send_register_application();
			do_receive();
		}
	} else {
		VSOMEIP_DEBUG << "Trying to open daemon queue in " << retry_timeout_ << "ms";
		retry_timer_.expires_from_now(
			std::chrono::milliseconds(retry_timeout_));
		retry_timer_.async_wait(
			boost::bind(
				&administration_proxy_impl::retry_open_cbk,
				this,
				boost::asio::placeholders::error
			)
		);

		// next time we wait longer
		retry_timeout_ <<= 1;
	}
}

void administration_proxy_impl::retry_open_cbk(boost::system::error_code const &_error) {
	VSOMEIP_TRACE << "administration_proxy_impl::retry_open_cbk";
	if (!_error) {
		daemon_queue_->async_open(
			queue_name_prefix_ + daemon_queue_name_,
			boost::bind(
				&administration_proxy_impl::open_cbk,
				this,
				boost::asio::placeholders::error
			)
		);
	}
}

void administration_proxy_impl::create_cbk(boost::system::error_code const &_error) {
	VSOMEIP_TRACE << "administration_proxy_impl::create_cbk";
	if (_error) {
		// Try destroying before creating
		application_queue_->async_close(
			boost::bind(
				&administration_proxy_impl::close_cbk,
				this,
				boost::asio::placeholders::error
			)
		);

		// TODO: define maximum number of retries
		application_queue_->async_create(
			queue_name_prefix_ + application_queue_name_,
			slots_,
			VSOMEIP_DEFAULT_QUEUE_SIZE,
			boost::bind(
				&administration_proxy_impl::create_cbk,
				this,
				boost::asio::placeholders::error
			)
		);
	} else {
		is_created_ = true;
		if (is_open_) {
			send_register_application();
			do_receive();
		}
	}
}

void administration_proxy_impl::close_cbk(
		boost::system::error_code const &_error) {
	VSOMEIP_TRACE << "administration_proxy_impl::close_cbk";
	if (!_error) {
		owner_.set_id(0);
	}
}

void administration_proxy_impl::send_cbk(
		boost::system::error_code const &_error) {
	VSOMEIP_TRACE << "administration_proxy_impl::send_cbk";
	if (_error) {
		VSOMEIP_ERROR << "Applicatiom " << owner_.get_name()
					  << ": Error while sending (" << _error.message() << ")";
	}

	send_buffers_.pop_front();
}

void administration_proxy_impl::receive_cbk(
		boost::system::error_code const &_error,
		std::size_t _bytes, unsigned int _priority) {
	VSOMEIP_TRACE << "administration_proxy_impl::receive_cbk";
	if (!_error && _bytes) {
		process_message(_bytes);
	}

	do_receive();
}

void administration_proxy_impl::request_cbk(
		boost::system::error_code const &_error,
		service_id _service, instance_id _instance,
		message_queue *_queue, const std::string &_queue_name) {

	if (!_error) {
		VSOMEIP_DEBUG
			<< "APPLICATION " << owner_.get_id() << " opened queue for service ("
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

///////////////////////////////////////////////////////////////////////////////
// Helper function to automate queue handling
///////////////////////////////////////////////////////////////////////////////
void administration_proxy_impl::remove_queue(const std::string &_name) {
	VSOMEIP_TRACE << "Removing queue " << _name;
	queues_.erase(_name);
}

void intrusive_ptr_add_ref(vsomeip::message_queue *_queue) {
	if (_queue) {
		_queue->add_ref();
	}
}

void intrusive_ptr_release(vsomeip::message_queue *_queue) {
	if (_queue) {
		_queue->release();
		if (0 == _queue->get_ref() && 0 != _queue->get_owner()) {
			_queue->get_owner()->remove_queue(_queue->get_name());
			delete _queue;
		}
	}
}

} // namespace vsomeip
