// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef UNKNOWN_SD_OPTION_TYPE_TEST_GLOBALS_HPP_
#define UNKNOWN_SD_OPTION_TYPE_TEST_GLOBALS_HPP_

namespace unknown_sd_option_type_test {

// SOME/IP MESSAGE MACROS
std::uint16_t SD_DEFAULT_SERVICE_ID {0xFFFF};
std::uint16_t SD_DEFAULT_METHOD_ID {0x8100};
std::uint16_t SD_DEFAULT_CLIENT_ID {0x0000};
std::uint16_t SD_CLIENT_ID {0x2222};
std::uint16_t NOTIF_METHOD_ID {0x4242};
std::uint16_t SHUTDOWN_METHOD_ID {0x1404};
std::uint16_t SERVER_PORT {30001};
std::uint8_t SOMEIP_VERSION {0x01};
std::uint8_t SOMEIP_INTERFACE_VERSION {0x00};
std::uint8_t SOMEIP_SD_INTERFACE_VERSION {0x01};
std::uint8_t UDP_PROTOCOL {0x11};

// SD SUBSCRIPTION MESSAGE MACROS
std::uint16_t EVENTGROUP_ID {0x1000};
std::uint16_t SD_SESSION_ID {0x0001};
std::uint8_t INTERFACE_VERSION {0x01};
std::uint16_t SD_SERVICE_ID {0x1122};
std::uint16_t SD_INSTANCE_ID {0x0001};
std::uint8_t SD_MAJOR_VERSION {0x00};
std::uint8_t SD_COUNTER {0x00};
std::uint32_t SD_TTL {0x00000003};
std::uint16_t SD_PORT = 0x771A;
std::uint8_t SUBSCRIBE_EVENTGROUP_ACK {0x07};

std::uint8_t INDEX_1_OF_ENTRY_1_OPTION_COUNT {27};
std::uint8_t OPTION_COUNT {0x10};

struct service_info {
    vsomeip::service_t service_id;
    vsomeip::instance_t instance_id;
    vsomeip::method_t method_id;
    vsomeip::event_t event_id;
    vsomeip::eventgroup_t eventgroup_id;
    vsomeip::method_t shutdown_method_id;
    vsomeip::method_t notify_method_id;
};

struct service_info service = { 0x1122, 0x1, 0x1111, 0x1111, 0x1000, 0x1404, 0x4242 };

}

#endif /* UNKNOWN_SD_OPTION_TYPE_TEST_GLOBALS_HPP_ */
