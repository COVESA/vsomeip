// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_RUNTIME_HPP
#define VSOMEIP_RUNTIME_HPP

#include <memory>
#include <string>
#include <vector>

#include <vsomeip/export.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class application;
class message;
class payload;

class VSOMEIP_EXPORT runtime {
public:
    static std::shared_ptr<runtime> get();

    virtual ~runtime() {
    }

    virtual std::shared_ptr<application> create_application(
            const std::string &_name = "") const = 0;

    virtual std::shared_ptr<message> create_message(
            bool _reliable = false) const = 0;
    virtual std::shared_ptr<message> create_request(
            bool _reliable = false) const = 0;
    virtual std::shared_ptr<message> create_response(
            const std::shared_ptr<message> &_request) const = 0;
    virtual std::shared_ptr<message> create_notification(
            bool _reliable = false) const = 0;

    virtual std::shared_ptr<payload> create_payload() const = 0;
    virtual std::shared_ptr<payload> create_payload(
            const byte_t *_data, uint32_t _size) const = 0;
    virtual std::shared_ptr<payload> create_payload(
            const std::vector<byte_t> &_data) const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_RUNTIME_HPP
