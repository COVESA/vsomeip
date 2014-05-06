//
// daemon_proxy_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_DAEMON_DAEMON_PROXY_IMPL_HPP
#define VSOMEIP_DAEMON_DAEMON_PROXY_IMPL_HPP

#include <vsomeip_internal/managing_proxy_impl.hpp>

namespace vsomeip {

class daemon_impl;

class daemon_proxy_impl : public managing_proxy_impl {
public:
	daemon_proxy_impl(daemon_impl &_owner);
	virtual ~daemon_proxy_impl();

	void receive(const uint8_t *_data, uint32_t _size, const endpoint *_source, const endpoint *_target);

private:
	daemon_impl &daemon_;
};

} // namespace vsomeip

#endif // VSOMEIP_DAEMON_DAEMON_PROXY_IMPL_HPP
