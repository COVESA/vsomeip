// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef SECURITY_CONFIG_PLUGIN_TEST_GLOBALS_HPP_
#define SECURITY_CONFIG_PLUGIN_TEST_GLOBALS_HPP_

namespace security_config_plugin_test {

struct service_info {
    vsomeip::service_t service_id;
    vsomeip::instance_t instance_id;
    vsomeip::method_t method_id;
    vsomeip::event_t event_id;
    vsomeip::eventgroup_t eventgroup_id;
};

// ACL interface of plugin (this service is allowed to be offered in global security config)
static constexpr service_info security_config_plugin_serviceinfo =       { 0xF90F, 0x01, 0x1, 0x0, 0x0 };
static constexpr vsomeip::major_version_t security_config_plugin_major_version_ = 0x01;
static constexpr vsomeip::minor_version_t security_config_plugin_minor_version_ = 0x00;

static constexpr service_info security_config_plugin_serviceinfo_reset = { 0xF90F, 0x01, 0x2, 0x0, 0x0 };

// services to test policy (these services are denied to be offered in global security config and will be allowed via updateAcl)
static constexpr service_info security_config_test_serviceinfo_1 =       { 0x0101, 0x63, 0x1, 0x8001, 0x1 };
static constexpr service_info security_config_test_serviceinfo_2 =       { 0x0102, 0x63, 0x2, 0x8002, 0x1 };

// service to control offering of above service instances via client method call (this service is allowed to be offered in global security config)
static constexpr service_info security_config_test_serviceinfo_3 =       { 0x0103, 0x63, 0x3, 0x0, 0x0 };


static constexpr int notifications_to_send = 1;
}

#endif /* SECURITY_CONFIG_PLUGIN_TEST_GLOBALS_HPP_ */
