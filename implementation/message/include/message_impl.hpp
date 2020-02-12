// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_MESSAGE_IMPL_HPP
#define VSOMEIP_V3_MESSAGE_IMPL_HPP

#include <memory>

#include <vsomeip/export.hpp>
#include <vsomeip/primitive_types.hpp>
#include "message_base_impl.hpp"

#  if _MSC_VER >= 1300
/*
* Diamond inheritance is used for the vsomeip::message_base base class.
* The Microsoft compiler put warning (C4250) using a desired c++ feature: "Delegating to a sister class"
* A powerful technique that arises from using virtual inheritance is to delegate a method from a class in another class
* by using a common abstract base class. This is also called cross delegation.
*/
#    pragma warning( disable : 4250 )
#  endif

namespace vsomeip_v3 {

class payload;

class message_impl
        : virtual public message,
          virtual public message_base_impl {
public:
    VSOMEIP_EXPORT message_impl();
    VSOMEIP_EXPORT virtual ~message_impl();

    VSOMEIP_EXPORT length_t get_length() const;
    VSOMEIP_EXPORT void set_length(length_t _length);

    VSOMEIP_EXPORT std::shared_ptr< payload > get_payload() const;
    VSOMEIP_EXPORT void set_payload(std::shared_ptr< payload > _payload);

    VSOMEIP_EXPORT bool serialize(serializer *_to) const;
    VSOMEIP_EXPORT bool deserialize(deserializer *_from);

    VSOMEIP_EXPORT uint8_t get_check_result() const;
    VSOMEIP_EXPORT void set_check_result(uint8_t _check_result);
    VSOMEIP_EXPORT bool is_valid_crc() const;

    VSOMEIP_EXPORT uid_t get_uid() const;
    VSOMEIP_EXPORT void set_uid(uid_t _uid);

    VSOMEIP_EXPORT gid_t get_gid() const;
    VSOMEIP_EXPORT void set_gid(gid_t _gid);

protected: // members
    std::shared_ptr< payload > payload_;
    uint8_t check_result_;
    uid_t uid_;
    gid_t gid_;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_MESSAGE_IMPL_HPP
