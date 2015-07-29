// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_MESSAGE_HPP
#define VSOMEIP_MESSAGE_HPP

#include <memory>

#include <vsomeip/message_base.hpp>

namespace vsomeip {

class payload;

class message: virtual public message_base {
public:
    virtual ~message() {
    }

    virtual std::shared_ptr<payload> get_payload() const = 0;
    virtual void set_payload(std::shared_ptr<payload> _payload) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_MESSAGE_HPP
