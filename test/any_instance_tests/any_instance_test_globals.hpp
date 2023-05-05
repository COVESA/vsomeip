// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef ANY_INSTANCE_GLOBALS_HPP_
#define ANY_INSTANCE_GLOBALS_HPP_

namespace any_instance_test {

#define NUMBER_SERVICES 3

struct service_info {
    vsomeip::service_t service_id;
    vsomeip::instance_t instance_id;
    vsomeip::method_t method_id;
};

const vsomeip::service_t service_id = 0x1111;

struct service_info client = { 0x9999, 0x1, 0x9999 };

static constexpr std::array<service_info, NUMBER_SERVICES> service_infos = {{
    { 0x1111, 0x1, 0x1111},
    { 0x1111, 0x2, 0x1111},
    { 0x1111, 0x3, 0x1111}
}};
}

#endif /* ANY_INSTANCE_GLOBALS_HPP_ */
