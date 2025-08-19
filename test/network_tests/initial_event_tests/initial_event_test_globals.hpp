// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef SUBSCRIBE_NOTIFY_TEST_GLOBALS_HPP_
#define SUBSCRIBE_NOTIFY_TEST_GLOBALS_HPP_

namespace initial_event_test {

struct service_info {
    vsomeip::service_t service_id;
    vsomeip::instance_t instance_id;
    vsomeip::method_t method_id;
    vsomeip::event_t event_id;
    vsomeip::eventgroup_t eventgroup_id;
};

static constexpr std::array<service_info, 7> service_infos = {{// placeholder to be consistent w/ client ids, service ids, app names
                                                               {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
                                                               // node 1
                                                               {0x1111, 0x1, 0x1111, 0x1111, 0x1000},
                                                               {0x2222, 0x1, 0x2222, 0x2222, 0x2000},
                                                               {0x3333, 0x1, 0x3333, 0x3333, 0x3000},
                                                               // node 2
                                                               {0x4444, 0x1, 0x4444, 0x4444, 0x4000},
                                                               {0x5555, 0x1, 0x5555, 0x5555, 0x5000},
                                                               {0x6666, 0x1, 0x6666, 0x6666, 0x6000}}};

static constexpr std::array<service_info, 7> service_infos_same_service_id = {
        {// placeholder to be consistent w/ client ids, service ids, app names
         {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF},
         // node 1
         {0x1111, 0x1, 0x1111, 0x1111, 0x1000},
         {0x1111, 0x2, 0x2222, 0x2222, 0x2000},
         {0x1111, 0x3, 0x3333, 0x3333, 0x3000},
         // node 2
         {0x2222, 0x1, 0x4444, 0x4444, 0x4000},
         {0x2222, 0x2, 0x5555, 0x5555, 0x5000},
         {0x2222, 0x3, 0x6666, 0x6666, 0x6000}}};

static constexpr service_info stop_service_master = {0x8888, 0x1, 0x8888, 0x0, 0x0};
static constexpr service_info stop_service_slave = {0x9999, 0x1, 0x9999, 0x0, 0x0};

static constexpr int notifications_to_send = 1;
}

#endif /* SUBSCRIBE_NOTIFY_TEST_GLOBALS_HPP_ */
