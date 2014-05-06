//
// proxy_base_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������������������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/application_base_impl.hpp>
#include <vsomeip_internal/proxy_base_impl.hpp>

namespace vsomeip {

proxy_base_impl::proxy_base_impl(application_base_impl &_owner)
	: log_user(_owner),
	  owner_(_owner) {
}

proxy_base_impl::~proxy_base_impl() {
}

} // namespace vsomeip

