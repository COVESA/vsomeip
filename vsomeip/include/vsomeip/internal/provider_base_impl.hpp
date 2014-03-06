//
// provider_base_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_PROVIDER_BASE_IMPL_HPP
#define VSOMEIP_INTERNAL_PROVIDER_BASE_IMPL_HPP

#include <deque>
#include <map>
#include <vector>

#include <boost/asio/system_timer.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/provider.hpp>
#include <vsomeip/internal/participant_impl.hpp>

namespace vsomeip {

class endpoint;
class message_base;

class provider_base_impl
		: virtual public provider,
		  public participant_impl {
public:
	provider_base_impl(
			factory *_factory,
			uint32_t _max_message_size,
			boost::asio::io_service &_is);
	~provider_base_impl();

	bool send(const message_base *_message, bool _flush);
	bool send(const uint8_t *_data, uint32_t _size,
			   endpoint *_target, bool _flush);
	bool flush(endpoint *_target);

protected:
	std::map< endpoint *,
			  std::deque< std::vector< uint8_t > > > packet_queues_;

	std::map< endpoint *,
	  	  	  std::deque< std::vector< uint8_t > > >::iterator current_queue_;

	std::map< endpoint *,
			  std::vector< uint8_t > > packetizer_;

private:
	void flush(endpoint *_endpoint,
				const boost::system::error_code &_error_code);
	boost::asio::system_timer flush_timer_;

public:
	void connected(boost::system::error_code const &_error_code);

	void sent(boost::system::error_code const &_error_code,
			   std::size_t _sent_bytes);

	bool set_next_queue();
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_PROVIDER_BASE_IMPL_HPP
