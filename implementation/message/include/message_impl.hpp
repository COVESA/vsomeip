// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

#include <vsomeip/primitive_types.hpp>
#include "message_base_impl.hpp"

#if _MSC_VER >= 1300
/*
 * Diamond inheritance is used for the vsomeip::message_base base class.
 * The Microsoft compiler put warning (C4250) using a desired c++ feature: "Delegating to a sister
 * class" A powerful technique that arises from using virtual inheritance is to delegate a method
 * from a class in another class by using a common abstract base class. This is also called cross
 * delegation.
 */
#pragma warning(disable : 4250)
#endif

namespace vsomeip_v3 {

class payload;

class message_impl : virtual public message, virtual public message_base_impl {
public:
    message_impl();
    virtual ~message_impl();

    length_t get_length() const override;

    std::shared_ptr<payload> get_payload() const override;
    void set_payload(std::shared_ptr<payload> _payload) override;

    bool serialize(serializer* _to) const override;
    bool deserialize(deserializer* _from) override;

    uint8_t get_check_result() const override;
    void set_check_result(uint8_t _check_result) override;
    bool is_valid_crc() const override;

    uid_t get_uid() const override;

    gid_t get_gid() const override;

    vsomeip_sec_client_t get_sec_client() const override;
    std::string get_env() const override;

public:
    // not exported, these are internal to the codebase
    message_impl(const message_header_impl& _header, bool _reliable, uint8_t _check_result);

    void set_sec_client(const vsomeip_sec_client_t& _sec_client);
    void set_env(const std::string& _env);

protected: // members
    std::shared_ptr<payload> payload_;
    uint8_t check_result_;
    vsomeip_sec_client_t sec_client_;
    std::string env_;
};

} // namespace vsomeip_v3
