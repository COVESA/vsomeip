//
// client_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_CLIENT_IMPL_HPP
#define VSOMEIP_CLIENT_IMPL_HPP

#include <map>

#include <boost/asio/io_service.hpp>

#include <vsomeip/client.hpp>
#include <vsomeip/internal/client_base_impl.hpp>

namespace vsomeip {

class client_impl
			: virtual public client,
			  virtual public client_base_impl {
public:
	virtual ~client_impl();

	consumer * create_consumer(const endpoint *_target);
	provider * create_provider(const endpoint *_source);

private:
	std::map<const endpoint *, consumer *> consumers_;
	std::map<const endpoint *, provider *> providers_;
};

} // namespace vsomeip

#endif // VSOMEIP_CLIENT_IMPL_HPP
