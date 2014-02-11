//
// participant_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_PARTICIPANT_IMPL_HPP
#define VSOMEIP_INTERNAL_PARTICIPANT_IMPL_HPP

#include <map>
#include <set>

#include <boost/asio/io_service.hpp>

#include <vsomeip/participant.hpp>
#include <vsomeip/internal/statistics_owner_impl.hpp>

namespace vsomeip {

class factory;
class deserializer;
class receiver;
class serializer;

class participant_impl
	: virtual public participant
#ifdef USE_VSOMEIP_STATISTICS
	, virtual public statistics_owner_impl
#endif
{
public:
	participant_impl(const factory *_factory, uint32_t _max_message_size,
					   boost::asio::io_service &_is);
	virtual ~participant_impl();

	std::size_t poll_one();
	std::size_t poll();
	std::size_t run();

	virtual void register_for(receiver *_receiver,
								 service_id _service_id,
								 method_id _method_id);
	virtual void unregister_for(receiver * receiver,
			 	 	 	 	 	   service_id _service_id,
			 	 	 	 	 	   method_id _method_id);

	virtual void connect() = 0;
	virtual void receive() = 0;

	void received(boost::system::error_code const &_error_code,
				  std::size_t _transferred_bytes);

	void enable_magic_cookies();
	void disable_magic_cookies();

private:
	void receive(const message_base *_message) const;

	virtual bool is_magic_cookie(message_base *_message) const;
	virtual bool is_magic_cookie(message_id _message_id,
									length _length,
									request_id _request_id,
									protocol_version _protocol_version,
									interface_version _interface_version,
									message_type _message_type,
									return_code _return_code) const;
	bool resync_on_magic_cookie();

protected:
	virtual void send_queued() = 0;
	virtual void restart() = 0;

private:
	virtual ip_address get_remote_address() const = 0;
	virtual ip_port get_remote_port() const = 0;
	virtual ip_protocol get_protocol() const = 0;
	virtual ip_version get_version() const = 0;

	virtual const uint8_t * get_received() const = 0;

protected:
	boost::asio::io_service &is_;

	std::map< service_id,
			  std::map< method_id,
			  	  	    std::set< receiver * > > > receiver_registry_;

	serializer * serializer_;
	deserializer * deserializer_;

	uint32_t max_message_size_;
	bool has_magic_cookies_;
	bool has_enabled_magic_cookies_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_PARTICIPANT_IMPL_HPP
