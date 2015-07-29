// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_RUNTIME_IMPL_HPP
#define VSOMEIP_RUNTIME_IMPL_HPP

#include <vsomeip/runtime.hpp>

namespace vsomeip {

class runtime_impl: public runtime {
public:
    static std::shared_ptr<runtime> get();

    virtual ~runtime_impl();

    std::shared_ptr<application> create_application(
            const std::string &_name) const;

    std::shared_ptr<message> create_message(bool _reliable) const;
    std::shared_ptr<message> create_request(bool _reliable) const;
    std::shared_ptr<message> create_response(
            const std::shared_ptr<message> &_request) const;
    std::shared_ptr<message> create_notification(bool _reliable) const;

    std::shared_ptr<payload> create_payload() const;
    std::shared_ptr<payload> create_payload(const byte_t *_data,
            uint32_t _size) const;
    std::shared_ptr<payload> create_payload(
            const std::vector<byte_t> &_data) const;
};

} // namespace vsomeip

#endif // VSOMEIP_RUNTIME_IMPL_HPP
