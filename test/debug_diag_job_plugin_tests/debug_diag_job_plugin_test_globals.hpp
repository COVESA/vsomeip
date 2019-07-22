// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef DEBUG_DIAG_JOB_PLUGIN_TEST_GLOBALS_HPP_
#define DEBUG_DIAG_JOB_PLUGIN_TEST_GLOBALS_HPP_

namespace debug_diag_job_plugin_test {

struct service_info {
    vsomeip::service_t service_id;
    vsomeip::instance_t instance_id;
    vsomeip::method_t method_id;
    vsomeip::event_t event_id;
    vsomeip::eventgroup_t eventgroup_id;
};

static constexpr std::array<service_info, 3> service_infos_remote = {{
        // placeholder to be consistent w/ client ids, service ids, app names
        { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
        { 0x1010, 0x1, 0x1010, 0x8111, 0x1 },
        { 0x2020, 0x1, 0x2020, 0x8222, 0x2 }
}};

static constexpr std::array<service_info, 3> service_infos_local = {{
        // placeholder to be consistent w/ client ids, service ids, app names
        { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
        { 0x3030, 0x1, 0x3030, 0x8333, 0x1 },
        { 0x4040, 0x1, 0x4040, 0x8444, 0x2 }
}};

static constexpr service_info debug_diag_job_serviceinfo = { 0xfea3, 0x80, 0x1, 0x0, 0x0 };
static constexpr service_info debug_diag_job_serviceinfo_reset = { 0xfea3, 0x80, 0x2, 0x0, 0x0 };

static constexpr int notifications_to_send = 1;
}

#endif /* DEBUG_DIAG_JOB_PLUGIN_TEST_GLOBALS_HPP_ */
