//
// service_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SERVICE_IMPL_HPP
#define VSOMEIP_INTERNAL_SERVICE_IMPL_HPP

#include <deque>
#include <map>
#include <vector>

#include <boost/array.hpp>
#include <boost/asio/io_service.hpp>

#include <vsomeip_internal/service.hpp>
#include <vsomeip_internal/participant_impl.hpp>

namespace vsomeip {

template < typename Protocol, int MaxBufferSize >
class service_impl
		: virtual public service,
		  public participant_impl< MaxBufferSize > {
public:
	service_impl(managing_application *_owner, const endpoint *_location);

	bool is_client() const;

	bool send(const uint8_t *_data, uint32_t _size, const endpoint *_target, bool _flush);
	bool flush(const endpoint *_target);

	typedef typename Protocol::socket socket_type;
	typedef typename Protocol::endpoint endpoint_type;
	typedef boost::array< uint8_t, MaxBufferSize > buffer_type;

public:
	void connect_cbk(boost::system::error_code const &_error);
	void send_cbk(
			boost::system::error_code const &_error, std::size_t _bytes);
	void flush_cbk(
			const endpoint *_target, const boost::system::error_code &_error);

public:
	virtual void send_queued() = 0;

protected:
	std::map< const endpoint *,
			  std::deque< std::vector< uint8_t > > > packet_queues_;

	std::map< const endpoint *,
			  std::deque< std::vector< uint8_t > > >::iterator current_queue_;

	std::map< const endpoint *,
			  std::vector< uint8_t > > packetizer_;

	boost::asio::system_timer flush_timer_;

private:
	bool set_next_queue();
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SERVICE_IMPL_HPP
