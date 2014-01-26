/*
 * participant_impl.cpp
 *
 *  Created on: Jan 25, 2014
 *      Author: lutz
 */

#include <vsomeip/factory.hpp>
#include <vsomeip/deserializer.hpp>
#include <vsomeip/receiver.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/impl/participant_impl.hpp>

namespace vsomeip {

participant_impl::participant_impl(uint32_t _max_message_size) {
	serializer_ = factory::get_default_factory()->create_serializer();
	deserializer_ = factory::get_default_factory()->create_deserializer();
	max_message_size_ = _max_message_size;
	if (serializer_)
		serializer_->create_data(max_message_size_);
}

participant_impl::~participant_impl() {
}

std::size_t participant_impl::poll_one() {
	return is_.poll_one();
}

std::size_t participant_impl::poll() {
	return is_.poll();
}

std::size_t participant_impl::run() {
	return is_.run();
}

void participant_impl::register_for(
		receiver *_receiver,
		service_id _service_id, method_id _method_id) {

	std::map< service_id, std::set< receiver * > >& service_registry
		= receiver_registry_[_service_id];
	service_registry[_method_id].insert(_receiver);
}

void participant_impl::unregister_for(
			receiver * _receiver,
			service_id _service_id, method_id _method_id) {

	auto i = receiver_registry_.find(_service_id);
	if (i != receiver_registry_.end()) {
		auto j = i->second.find(_method_id);
		if (j != i->second.end()) {
			j->second.erase(_receiver);
		}
	}
}

//
// Internal part
//
void participant_impl::receive(const message_base *_message) const {
	service_id requested_service = _message->get_service_id();
	auto i = receiver_registry_.find(requested_service);
	if (i != receiver_registry_.end()) {
		method_id requested_method = _message->get_message_id();
		auto j = i->second.find(requested_method);
		if (j != i->second.end()) {
			std::for_each(j->second.begin(), j->second.end(),
					[_message](receiver* const& _receiver) {
						_receiver->receive(_message); });
		}
	}
}

void participant_impl::received(
		boost::system::error_code const &_error_code,
		std::size_t _transferred_bytes) {

	if (!_error_code) {
#ifdef USE_VSOMEIP_STATISTICS
		statistics_.received_bytes_ += _transferred_bytes;
#endif
		deserializer_->append_data(get_received(), _transferred_bytes);

		bool has_deserialized;
		do {
			has_deserialized = false;

			uint32_t message_length = 0;
			deserializer_->look_ahead(VSOMEIP_LENGTH_POSITION, message_length);

			if (message_length + VSOMEIP_STATIC_HEADER_LENGTH
					<= deserializer_->get_available()) {
				deserializer_->set_remaining(message_length
											 + VSOMEIP_STATIC_HEADER_LENGTH);
				message_base* received_message
					= deserializer_->deserialize_message();
				if (0 != received_message) {
					endpoint *sender
						= factory::get_default_factory()->create_endpoint(
								get_remote_address(), get_remote_port(),
								get_protocol(), get_version());

					received_message->set_endpoint(sender);
#ifdef USE_VSOMEIP_STATISTICS
					statistics_.received_messages_++;
#endif
					receive(received_message);
					has_deserialized = true;
				}
				deserializer_->reset();
			}
		} while (has_deserialized);

		restart();
	} else {
		std::cerr << _error_code.message() << std::endl;
	}
}


} // namespace vsomeip


