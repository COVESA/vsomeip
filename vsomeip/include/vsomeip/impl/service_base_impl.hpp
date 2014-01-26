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
#include <vector>

#include <vsomeip/service.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/impl/participant_impl.hpp>

namespace vsomeip {

class message_base;

class service_base_impl
		: virtual public service,
		  public participant_impl {
public:
	service_base_impl(uint32_t _max_message_size);
	~service_base_impl();

	virtual bool send(const message_base *_message, bool _flush);

protected:
	std::map< endpoint *,
			  std::deque< std::vector< uint8_t > > > packet_queue_;
	std::map< endpoint *,
	  	  	  std::deque< std::vector< uint8_t > > >::iterator current_queue_;

	std::map< endpoint *,
			  std::vector< uint8_t > > packetizer_;

public:
	void connected(boost::system::error_code const &_error_code);

	void sent(boost::system::error_code const &_error_code,
			   std::size_t _sent_bytes);

	void set_next_queue();
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_SERVICE_BASE_IMPL_HPP
