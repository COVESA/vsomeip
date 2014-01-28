//
// connection_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_CONNECTION_IMPL_HPP
#define VSOMEIP_INTERNAL_CONNECTION_IMPL_HPP

#include <boost/asio.hpp>

namespace vsomeip {

class client_impl;

class connection_impl {
public:
	connection_impl(const boost::asio::ip::tcp::endpoint &_endpoint);

private:
	client * owner_;

	boost::asio::ip::tcp::socket socket_;

private:
	connection_impl();
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_CONNECTION_IMPL_HPP
