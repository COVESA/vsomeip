// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/serviceinfo.hpp"

namespace vsomeip {

serviceinfo::serviceinfo(major_version_t _major, minor_version_t _minor,
                         ttl_t _ttl)
    : group_(0),
      major_(_major),
      minor_(_minor),
      ttl_(_ttl),
      reliable_(nullptr),
      unreliable_(nullptr),
      multicast_group_(0xFFFF) {
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

std::shared_ptr<endpoint> serviceinfo::get_endpoint(bool _reliable) const {
  return (_reliable ? reliable_ : unreliable_);
}

void serviceinfo::set_endpoint(std::shared_ptr<endpoint> _endpoint,
                               bool _reliable) {
  if (_reliable) {
    reliable_ = _endpoint;
  } else {
    unreliable_ = _endpoint;
  }
}

const std::string & serviceinfo::get_multicast_address() const {
  return multicast_address_;
}

void serviceinfo::set_multicast_address(const std::string &_multicast_address) {
  multicast_address_ = _multicast_address;
}

uint16_t serviceinfo::get_multicast_port() const {
  return multicast_port_;
}

void serviceinfo::set_multicast_port(uint16_t _multicast_port) {
  multicast_port_ = _multicast_port;
}

eventgroup_t serviceinfo::get_multicast_group() const {
  return multicast_group_;
}

void serviceinfo::set_multicast_group(eventgroup_t _multicast_group) {
  multicast_group_ = _multicast_group;
}

void serviceinfo::add_client(client_t _client) {
  requesters_.insert(_client);
}

void serviceinfo::remove_client(client_t _client) {
  requesters_.erase(_client);
}

}  // namespace vsomeip

