// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/constants.hpp>

#include "../include/endpoint_definition.hpp"

namespace vsomeip {

std::map<boost::asio::ip::address,
    std::map<uint16_t,
        std::map<bool, std::shared_ptr<endpoint_definition> > > >
endpoint_definition::definitions_;

std::mutex endpoint_definition::definitions_mutex_;

std::shared_ptr<endpoint_definition>
endpoint_definition::get(const boost::asio::ip::address &_address,
                         uint16_t _port, bool _is_reliable) {
    std::lock_guard<std::mutex> its_lock(definitions_mutex_);
    std::shared_ptr<endpoint_definition> its_result;

    auto find_address = definitions_.find(_address);
    if (find_address != definitions_.end()) {
        auto find_port = find_address->second.find(_port);
        if (find_port != find_address->second.end()) {
            auto found_reliable = find_port->second.find(_is_reliable);
            if (found_reliable != find_port->second.end()) {
                its_result = found_reliable->second;
            }
        }
    }

    if (!its_result) {
        its_result = std::make_shared<endpoint_definition>(
                         _address, _port, _is_reliable);
        definitions_[_address][_port][_is_reliable] = its_result;
    }
    return its_result;
}

endpoint_definition::endpoint_definition(
        const boost::asio::ip::address &_address, uint16_t _port,
        bool _is_reliable)
        : address_(_address), port_(_port), remote_port_(_port),
          is_reliable_(_is_reliable) {
}

const boost::asio::ip::address & endpoint_definition::get_address() const {
    return address_;
}

uint16_t endpoint_definition::get_port() const {
    return port_;
}

bool endpoint_definition::is_reliable() const {
    return is_reliable_;
}

uint16_t endpoint_definition::get_remote_port() const {
    return remote_port_;
}

void endpoint_definition::set_remote_port(uint16_t _port) {
    remote_port_ = _port;
}


} // namespace vsomeip
