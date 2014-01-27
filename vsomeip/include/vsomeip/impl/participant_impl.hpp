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

#ifndef VSOMEIP_IMPL_PARTICIPANT_IMPL_HPP
#define VSOMEIP_IMPL_PARTICIPANT_IMPL_HPP

#include <map>
#include <set>

#include <boost/asio/io_service.hpp>

#include <vsomeip/participant.hpp>
#include <vsomeip/impl/statistics_owner_impl.hpp>

namespace vsomeip {

class deserializer;
class receiver;

class participant_impl
	: virtual public participant
#ifdef USE_VSOMEIP_STATISTICS
	, virtual public statistics_owner_impl
#endif
{
public:
	participant_impl(uint32_t _max_message_size, bool _is_supporting_resync);
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

	void received(boost::system::error_code const &_error_code,
				  std::size_t _transferred_bytes);

	bool is_sending_magic_cookies() const;
	void set_sending_magic_cookies(bool _is_sending_magic_cookies);

private:
	void receive(const message_base *_message) const;
	bool resync_on_magic_cookie();

protected:
	virtual void send_queued() = 0;
	virtual void restart() = 0;

private:
	virtual std::string get_remote_address() const = 0;
	virtual uint16_t get_remote_port() const = 0;
	virtual ip_protocol get_protocol() const = 0;
	virtual ip_version get_version() const = 0;

	virtual const uint8_t * get_received() const = 0;
	virtual bool is_magic_cookie(const message_base *_message) const = 0;

protected:
	std::map< service_id,
			  std::map< method_id,
			  	  	    std::set< receiver * > > > receiver_registry_;

	serializer * serializer_;
	deserializer * deserializer_;

	uint32_t max_message_size_;
	bool is_supporting_resync_;
	bool is_sending_magic_cookies_;

	boost::asio::io_service is_;
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_PARTICIPANT_IMPL_HPP
