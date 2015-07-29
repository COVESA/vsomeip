// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_REQUEST_HPP
#define VSOMEIP_SD_REQUEST_HPP

#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;

namespace sd {

class request {
public:
    request(major_version_t _major, minor_version_t _minor, ttl_t _ttl);

    major_version_t get_major() const;
    void set_major(major_version_t _major);

    minor_version_t get_minor() const;
    void set_minor(minor_version_t _minor);

    ttl_t get_ttl() const;
    void set_ttl(ttl_t _ttl);

private:
    major_version_t major_;
    minor_version_t minor_;
    ttl_t ttl_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_SUBSCRIPTION_HPP
