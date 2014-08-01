// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ENDPOINT_DEFINITION_HPP
#define VSOMEIP_ENDPOINT_DEFINITION_HPP

#include <boost/asio/ip/address.hpp>

namespace vsomeip {

class endpoint_definition {
public:
	endpoint_definition();
	endpoint_definition(const boost::asio::ip::address &_address, uint16_t _port, bool _is_reliable);

	const boost::asio::ip::address & get_address() const;
	void set_address(const boost::asio::ip::address &_address);

	uint16_t get_port() const;
	void set_port(uint16_t _port);

	bool is_reliable() const;
	void set_reliable(bool _is_reliable);

private:
	boost::asio::ip::address address_;
	uint16_t port_;
	bool is_reliable_;
};

} // namespace vsomeip

#endif // VSOMEIP_ENDPOINT_DEFINITION_HPP
