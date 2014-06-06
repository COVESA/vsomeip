//
// managed_proxy_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_MANAGED_PROXY_IMPL_HPP
#define VSOMEIP_INTERNAL_MANAGED_PROXY_IMPL_HPP

#include <deque>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread/mutex.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip_internal/administration_proxy_impl.hpp>
#include <vsomeip_internal/message_queue.hpp>
#include <vsomeip_internal/proxy.hpp>

namespace vsomeip {

class deserializer;
class serializer;

class managed_proxy_impl
		: public administration_proxy_impl {
public:
	managed_proxy_impl(application_base_impl &_owner);
	virtual ~managed_proxy_impl();

	void init();
	void start();
	void stop();

	bool send(message_base *_message, bool _reliable, bool _flush);

	bool enable_magic_cookies(service_id _service, instance_id _instance);
	bool disable_magic_cookies(service_id _service, instance_id _instance);

private: // internal
	virtual void catch_up_registrations();

	void process_command(command_enum _command, client_id _client, const uint8_t *_payload, uint32_t payload_size);

	void on_message(client_id _id, const uint8_t *_data, uint32_t _size);

	message_queue * find_target_queue(service_id, instance_id) const;
	message_queue * find_target_queue(client_id);

private:
	void send_cbk(boost::system::error_code const &);
	void response_cbk(boost::system::error_code const &_error, message_queue *_queue);

private:
	// Serialization & deserialization
	boost::shared_ptr< serializer > serializer_;
	boost::shared_ptr< deserializer > deserializer_;

	// buffers
	std::deque< std::vector< uint8_t > > send_buffers_;

	// Mutex to lock while manipulating sender queue
	boost::mutex mutex_;
};

} // namespace vsomeip

#endif // VSOMEIP_MANAGED_PROXY_IMPL_HPP
