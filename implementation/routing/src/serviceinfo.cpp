// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/serviceinfo.hpp"

namespace vsomeip {

serviceinfo::serviceinfo(major_version_t _major, minor_version_t _minor, ttl_t _ttl)
	: group_(0), major_(_major), minor_(_minor), ttl_(_ttl) {
}

serviceinfo::~serviceinfo() {
}

servicegroup * serviceinfo::get_group() const {
	return group_;
}

void serviceinfo::set_group(servicegroup *_group) {
	group_ = _group;
}

major_version_t serviceinfo::get_major() const {
	return major_;
}

minor_version_t serviceinfo::get_minor() const {
	return minor_;
}

ttl_t serviceinfo::get_ttl() const {
	return ttl_;
}

void serviceinfo::set_ttl(ttl_t _ttl) {
	ttl_ = _ttl;
}

std::shared_ptr< endpoint > & serviceinfo::get_endpoint(bool _reliable) {
	if (_reliable)
		return reliable_;

	return unreliable_;
}

void serviceinfo::set_endpoint(std::shared_ptr< endpoint > &_endpoint, bool _reliable) {
	if (_reliable) {
		reliable_ = _endpoint;
	} else {
		unreliable_ = _endpoint;
	}
}

void serviceinfo::add_client(client_t _client) {
	requesters_.insert(_client);
}

void serviceinfo::remove_client(client_t _client) {
	requesters_.erase(_client);
}


} // namespace vsomeip



