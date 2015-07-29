// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ENDPOINT_HOST_HPP
#define VSOMEIP_ENDPOINT_HOST_HPP

#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;

class endpoint_host {
public:
    virtual ~endpoint_host() {}

    virtual void on_connect(std::shared_ptr<endpoint> _endpoint) = 0;
    virtual void on_disconnect(std::shared_ptr<endpoint> _endpoint) = 0;
    virtual void on_message(const byte_t *_data, length_t _length,
        endpoint *_receiver) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_ENDPOINT_HOST_HPP
