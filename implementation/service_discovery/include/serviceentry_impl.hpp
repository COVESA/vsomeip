// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_SERVICEENTRY_IMPL_HPP
#define VSOMEIP_SD_SERVICEENTRY_IMPL_HPP

#include "entry_impl.hpp"

namespace vsomeip {
namespace sd {

class serviceentry_impl: public entry_impl {
public:
    serviceentry_impl();
    virtual ~serviceentry_impl();

    minor_version_t get_minor_version() const;
    void set_minor_version(minor_version_t _version);

    bool serialize(vsomeip::serializer *_to) const;
    bool deserialize(vsomeip::deserializer *_from);

private:
    minor_version_t minor_version_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_SERVICEENTRY_IMPL_HPP
