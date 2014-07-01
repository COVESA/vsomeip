// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/routing_info.hpp"

namespace vsomeip {

routing_info::routing_info(major_version_t _major, minor_version_t _minor, ttl_t _ttl)
	: major_(_major), minor_(_minor), ttl_(_ttl) {
}

routing_info::~routing_info() {
}

major_version_t routing_info::get_major() const {
	return major_;
}

minor_version_t routing_info::get_minor() const {
	return minor_;
}

ttl_t routing_info::get_ttl() const {
	return ttl_;
}

void routing_info::set_ttl(ttl_t _ttl) {
	ttl_ = _ttl;
}

std::shared_ptr< endpoint > & routing_info::get_reliable_endpoint() {
	return reliable_endpoint_;
}

void routing_info::set_reliable_endpoint(std::shared_ptr< endpoint > &_endpoint) {
	reliable_endpoint_ = _endpoint;
}

std::shared_ptr< endpoint > & routing_info::get_unreliable_endpoint() {
	return unreliable_endpoint_;
}

void routing_info::set_unreliable_endpoint(std::shared_ptr< endpoint > &_endpoint) {
	unreliable_endpoint_ = _endpoint;
}

} // namespace vsomeip



