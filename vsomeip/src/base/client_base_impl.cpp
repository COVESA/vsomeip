//
// client_base_impl.cpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/internal/client_base_impl.hpp>

namespace vsomeip {

client_base_impl::~client_base_impl() {
}

std::size_t client_base_impl::poll_one() {
	return is_.poll_one();
}

std::size_t client_base_impl::poll() {
	return is_.poll();
}

std::size_t client_base_impl::run() {
	return is_.run();
}

} // namespace vsomeip

