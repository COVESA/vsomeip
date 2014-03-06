//
// client_base_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_CLIENT_BASE_IMPL_HPP
#define VSOMEIP_CLIENT_BASE_IMPL_HPP

#include <boost/asio/io_service.hpp>

#include <vsomeip/client_base.hpp>

namespace vsomeip {

class client_base_impl
			: virtual public client_base {
public:
	virtual ~client_base_impl();

	std::size_t poll_one();
	std::size_t poll();
	std::size_t run();

protected:
	boost::asio::io_service is_;
};

} // namespace vsomeip

#endif // VSOMEIP_CLIENT_IMPL_HPP
