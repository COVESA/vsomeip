//
// client_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_CLIENT_IMPL_HPP
#define VSOMEIP_INTERNAL_CLIENT_IMPL_HPP

#include <deque>
#include <vector>

#include <boost/array.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/utility.hpp>

#include <vsomeip_internal/client.hpp>
#include <vsomeip_internal/participant_impl.hpp>

namespace vsomeip {

class endpoint;

template < typename Protocol, int MaxBufferSize >
class client_impl
		: virtual public client,
		  public participant_impl< MaxBufferSize > {
public:
	client_impl(managing_proxy_impl *_owner, const endpoint *_location);
	virtual ~client_impl();

	bool send(const uint8_t *_data, uint32_t _size, bool _flush);
	bool flush();

	void stop();
	void restart();

	bool is_client() const;
	const uint8_t * get_buffer() const;

public:
	void connect_cbk(boost::system::error_code const &_error);
	void wait_connect_cbk(boost::system::error_code const &_error);
	void send_cbk(
			boost::system::error_code const &_error, std::size_t _bytes);
	void flush_cbk(boost::system::error_code const &_error);

public:
	virtual void connect() = 0;
	virtual void receive() = 0;

protected:
	typedef typename Protocol::socket socket_type;
	typedef typename Protocol::endpoint endpoint_type;
	typedef boost::array<uint8_t, MaxBufferSize> buffer_type;

	socket_type socket_;
	endpoint_type local_;
	buffer_type buffer_;

	boost::asio::system_timer flush_timer_;
	boost::asio::system_timer connect_timer_;
	uint32_t connect_timeout_;
	bool is_connected_;

	std::deque<std::vector< uint8_t > > packet_queue_;
	std::vector< uint8_t > packetizer_;

	virtual void send_queued() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_CLIENT_IMPL_HPP
