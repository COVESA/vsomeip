// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef APPLICATION_TEST_GLOBALS_HPP_
#define APPLICATION_TEST_GLOBALS_HPP_

namespace application_test {

struct service_info {
    vsomeip::service_t service_id;
    vsomeip::instance_t instance_id;
    vsomeip::method_t method_id;
    vsomeip::event_t event_id;
    vsomeip::eventgroup_t eventgroup_id;
    vsomeip::method_t shutdown_method_id;
    vsomeip::major_version_t major_version;
    vsomeip::minor_version_t minor_version;
};


struct service_info service = { 0x1111, 0x1, 0x1111, 0x1111, 0x1000, 0x1404, 0x2, 0x4711 };

static constexpr int number_of_messages_to_send = 150;
}

#endif /* APPLICATION_TEST_GLOBALS_HPP_ */
