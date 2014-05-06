//
// proxy_base_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_PROXY_BASE_IMPL_HPP
#define VSOMEIP_PROXY_BASE_IMPL_HPP

#include <vsomeip_internal/proxy.hpp>
#include <vsomeip_internal/log_user.hpp>

namespace vsomeip {

class application_base_impl;

class proxy_base_impl
		: virtual public proxy,
		  virtual public log_user {
public:
	proxy_base_impl(application_base_impl &_owner);
	virtual ~proxy_base_impl();

protected:
	application_base_impl &owner_;
};

} // namespace vsomeip

#endif // VSOMEIP_PROXY_BASE_IMPL_HPP
