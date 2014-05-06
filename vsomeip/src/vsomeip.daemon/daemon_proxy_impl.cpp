//
// daemon_proxy_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include "daemon_impl.hpp"
#include "daemon_proxy_impl.hpp"

namespace vsomeip {

daemon_proxy_impl::daemon_proxy_impl(daemon_impl &_owner)
	: managing_proxy_impl(_owner), proxy_base_impl(_owner), log_user(_owner), daemon_(_owner) {
};

daemon_proxy_impl::~daemon_proxy_impl() {
};

void daemon_proxy_impl::receive(const uint8_t *_data, uint32_t _size, const endpoint *_source, const endpoint *_target) {
	daemon_.receive(_data, _size, _source, _target);
}

} // namespace vsomeip

