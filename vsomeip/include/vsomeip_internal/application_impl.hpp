//
// application_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_APPLICATION_IMPL_HPP
#define VSOMEIP_INTERNAL_APPLICATION_IMPL_HPP

#include <deque>
#include <vector>

#include <boost/asio/system_timer.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/utility.hpp>

#include <boost_ext/asio/mq.hpp>

#include <vsomeip/application.hpp>
#include <vsomeip_internal/config.hpp>
#include <vsomeip_internal/log_owner.hpp>

namespace vsomeip {

class deserializer;
class endpoint;
class message_base;
class serializer;

class application_impl
	: virtual public application,
	  public log_owner,
	  boost::noncopyable {

public:
	application_impl(const std::string &_name);
	virtual ~application_impl();

	void init();
	void start();
	void stop();

	std::size_t poll_one();
	std::size_t poll();
	std::size_t run();

	bool request_service(service_id _service, instance_id _instance,
							const endpoint *_location) const;
	bool release_service(service_id _service, instance_id _instance) const;

	bool provide_service(service_id _service, instance_id _instance,
							const endpoint *_location) const;
	bool start_service(service_id _service, instance_id _instance) const;
	bool stop_service(service_id _service, instance_id _instance) const;

	bool send(message_base *_message, bool _flush) const;

	void enable_magic_cookies(service_id _service, instance_id _instance) const;
	void disable_magic_cookies(service_id _service, instance_id _instance) const;

	void enable_watchdog();
	void disable_watchdog();

private:
	void do_send(const std::vector<uint8_t> &_data);
	void do_receive();

	void process_message(std::size_t _bytes);

	void send_register_application();
	void send_deregister_application();

	void send_pong();

private: // callbacks
	void open_cbk(boost::system::error_code const &_error);
	void create_cbk(boost::system::error_code const &_error);
	void destroy_cbk(boost::system::error_code const &_error);
	void send_cbk(boost::system::error_code const &_error);
	void receive_cbk(boost::system::error_code const &_error,
					   std::size_t _bytes, unsigned int _priority);

private: // object members
	boost::asio::io_service service_;
	boost::asio::system_timer watchdog_timer_;

	uint32_t id_; // will be received from the daemon

	// Message queue to communicate to the vsomeip-daemon
	boost_ext::asio::message_queue daemon_queue_;

	std::string application_queue_name_;
	boost_ext::asio::message_queue application_queue_;

	std::deque<std::vector<uint8_t>> send_buffers_;
	uint8_t receive_buffer_[VSOMEIP_MAX_TCP_MESSAGE_SIZE +
	                        VSOMEIP_PROTOCOL_OVERHEAD];

	serializer *serializer_;
	deserializer *_deserializer_;

	// Name of this application (object)
	std::string name_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_APPLICATION_IMPL_HPP
