// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>

#include "../include/eventgroupinfo.hpp"

namespace vsomeip {

eventgroupinfo::eventgroupinfo()
    : major_(DEFAULT_MAJOR),
      ttl_(DEFAULT_TTL),
      is_multicast_(false) {
}

eventgroupinfo::~eventgroupinfo() {
}

major_version_t eventgroupinfo::get_major() const {
  return major_;
}

void eventgroupinfo::set_major(major_version_t _major) {
  major_ = _major;
}

ttl_t eventgroupinfo::get_ttl() const {
  return ttl_;
}

void eventgroupinfo::set_ttl(ttl_t _ttl) {
  ttl_ = _ttl;
}

bool eventgroupinfo::get_multicast(boost::asio::ip::address &_address, uint16_t &_port) {
  if (is_multicast_) {
    _address = address_;
    _port = port_;
  }
  return is_multicast_;
}

void eventgroupinfo::set_multicast(const boost::asio::ip::address &_address, uint16_t _port) {
  address_ = _address;
  port_ = _port;
  is_multicast_ = true;
}

const std::set<std::shared_ptr<event> > eventgroupinfo::get_events() const {
  return events_;
}

void eventgroupinfo::add_event(std::shared_ptr<event> _event) {
  events_.insert(_event);
}

}  // namespace vsomeip
