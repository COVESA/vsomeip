//
// service_base_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_SERVICE_BASE_IMPL_HPP
#define VSOMEIP_IMPL_SERVICE_BASE_IMPL_HPP

#include <deque>
#include <map>
#include <set>
#include <vector>

#include <boost/asio/io_service.hpp>

#include <vsomeip/service.hpp>
#include <vsomeip/serializer.hpp>
#include <vsomeip/deserializer.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/impl/statistics_owner_impl.hpp>

namespace vsomeip {

class message_base;
class receiver;

class service_base_impl
		: virtual public service,
		  virtual public statistics_owner_impl {
public: // client interface methods
	service_base_impl();
	~service_base_impl();

	virtual void start() = 0;
	virtual void stop() = 0;

	std::size_t poll_one();
	std::size_t poll();
	std::size_t run();

	virtual bool send(const message_base *_message, bool _flush);
	virtual void register_for(receiver *_receiver,
								 service_id _service_id,
								 method_id _method_id);
	virtual void unregister_for(receiver * receiver,
			 	 	 	 	 	   service_id _service_id,
			 	 	 	 	 	   method_id _method_id);

protected:
	std::map< endpoint *,
			  std::deque< std::vector< uint8_t > > > packet_queue_;
	std::map< endpoint *,
	  	  	  std::deque< std::vector< uint8_t > > >::iterator current_queue_;

	std::map< endpoint *,
			  std::vector< uint8_t > > packetizer_;

	std::map< service_id,
			  std::map< method_id,
			  	  	    std::set< receiver * > > > receiver_registry_;

	uint32_t max_message_size_;

	serializer *serializer_;
	deserializer *deserializer_;

	boost::asio::io_service is_;

private: // methods
	void receive(const message_base *_message) const;
	virtual void send_queued() = 0;

	// Methods to build an endpoint for replying
	virtual std::string get_remote_address() const = 0;
	virtual uint16_t get_remote_port() const = 0;
	virtual ip_protocol get_protocol() const = 0;
	virtual ip_version get_version() const = 0;

	virtual const uint8_t * get_received() const = 0;

	virtual void restart() = 0;

public:
	void connected(boost::system::error_code const &_error_code);

	void sent(boost::system::error_code const &_error_code,
			   std::size_t _sent_bytes);

	void received(boost::system::error_code const &_error_code,
					std::size_t _sent_bytes);

	void set_next_queue();
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_SERVICE_BASE_IMPL_HPP
