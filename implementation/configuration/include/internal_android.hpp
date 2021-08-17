// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_INTERNAL_HPP_
#define VSOMEIP_V3_INTERNAL_HPP_

#include <cstdint>
#include <limits>
#include <vsomeip/primitive_types.hpp>

#define VSOMEIP_ENV_APPLICATION_NAME            "VSOMEIP_APPLICATION_NAME"
#define VSOMEIP_ENV_CONFIGURATION               "VSOMEIP_CONFIGURATION"
#define VSOMEIP_ENV_CONFIGURATION_MODULE        "VSOMEIP_CONFIGURATION_MODULE"
#define VSOMEIP_ENV_E2E_PROTECTION_MODULE       "VSOMEIP_E2E_PROTECTION_MODULE"
#define VSOMEIP_ENV_MANDATORY_CONFIGURATION_FILES "VSOMEIP_MANDATORY_CONFIGURATION_FILES"
#define VSOMEIP_ENV_LOAD_PLUGINS                "VSOMEIP_LOAD_PLUGINS"
#define VSOMEIP_ENV_CLIENTSIDELOGGING           "VSOMEIP_CLIENTSIDELOGGING"
#define VSOMEIP_ENV_BASE_PATH                   "VSOMEIP_BASE_PATH"

#define VSOMEIP_DEFAULT_CONFIGURATION_FILE      "/vendor/etc/vsomeip.json"
#define VSOMEIP_LOCAL_CONFIGURATION_FILE        "./vsomeip.json"
#define VSOMEIP_MANDATORY_CONFIGURATION_FILES   "vsomeip_std.json,vsomeip_app.json,vsomeip_plc.json,vsomeip_log.json,vsomeip_security.json,vsomeip_whitelist.json"

#define VSOMEIP_DEFAULT_CONFIGURATION_FOLDER    "/vendor/etc/vsomeip"
#define VSOMEIP_DEBUG_CONFIGURATION_FOLDER      "/var/opt/public/sin/vsomeip/"
#define VSOMEIP_LOCAL_CONFIGURATION_FOLDER      "./vsomeip"

#define VSOMEIP_BASE_PATH                       "/storage/"

#define VSOMEIP_CFG_LIBRARY                     "libvsomeip3-cfg.so"

#define VSOMEIP_SD_LIBRARY                      "libvsomeip3-sd.so"

#define VSOMEIP_E2E_LIBRARY                     "libvsomeip3-e2e.so"

#define VSOMEIP_ROUTING_CLIENT                  0

#define VSOMEIP_CLIENT_UNSET                    0xFFFF

#define VSOMEIP_UNICAST_ADDRESS                 "127.0.0.1"
#define VSOMEIP_NETMASK                         "255.255.255.0"

#define VSOMEIP_DEFAULT_CONNECT_TIMEOUT         100
#define VSOMEIP_MAX_CONNECT_TIMEOUT             1600
#define VSOMEIP_DEFAULT_FLUSH_TIMEOUT           1000

#define VSOMEIP_DEFAULT_SHUTDOWN_TIMEOUT        5000

#define VSOMEIP_DEFAULT_QUEUE_WARN_SIZE         102400

#define VSOMEIP_MAX_TCP_CONNECT_TIME            5000
#define VSOMEIP_MAX_TCP_RESTART_ABORTS          5
#define VSOMEIP_MAX_TCP_SENT_WAIT_TIME          10000

#define VSOMEIP_DEFAULT_BUFFER_SHRINK_THRESHOLD 5

#define VSOMEIP_DEFAULT_WATCHDOG_TIMEOUT        5000
#define VSOMEIP_DEFAULT_MAX_MISSING_PONGS       3

#define VSOMEIP_DEFAULT_UDP_RCV_BUFFER_SIZE     1703936

#define VSOMEIP_IO_THREAD_COUNT                 2
#define VSOMEIP_IO_THREAD_NICE_LEVEL            255

#define VSOMEIP_MAX_DISPATCHERS                 10
#define VSOMEIP_MAX_DISPATCH_TIME               100

#define VSOMEIP_REQUEST_DEBOUNCE_TIME           10
#define VSOMEIP_DEFAULT_STATISTICS_MAX_MSG      50
#define VSOMEIP_DEFAULT_STATISTICS_MIN_FREQ     50
#define VSOMEIP_DEFAULT_STATISTICS_INTERVAL     10000

#define VSOMEIP_MAX_WAIT_SENT                   5

#define VSOMEIP_COMMAND_HEADER_SIZE             7

#define VSOMEIP_COMMAND_TYPE_POS                0
#define VSOMEIP_COMMAND_CLIENT_POS              1
#define VSOMEIP_COMMAND_SIZE_POS_MIN            3
#define VSOMEIP_COMMAND_SIZE_POS_MAX            6
#define VSOMEIP_COMMAND_PAYLOAD_POS             7

#define VSOMEIP_ASSIGN_CLIENT                   0x00
#define VSOMEIP_ASSIGN_CLIENT_ACK               0x01
#define VSOMEIP_REGISTER_APPLICATION            0x02
#define VSOMEIP_DEREGISTER_APPLICATION          0x03
#define VSOMEIP_APPLICATION_LOST                0x04
#define VSOMEIP_ROUTING_INFO                    0x05
#define VSOMEIP_REGISTERED_ACK                  0x06

#define VSOMEIP_PING                            0x0E
#define VSOMEIP_PONG                            0x0F

#define VSOMEIP_OFFER_SERVICE                   0x10
#define VSOMEIP_STOP_OFFER_SERVICE              0x11
#define VSOMEIP_SUBSCRIBE                       0x12
#define VSOMEIP_UNSUBSCRIBE                     0x13
#define VSOMEIP_REQUEST_SERVICE                 0x14
#define VSOMEIP_RELEASE_SERVICE                 0x15
#define VSOMEIP_SUBSCRIBE_NACK                  0x16
#define VSOMEIP_SUBSCRIBE_ACK                   0x17

#define VSOMEIP_SEND                            0x18
#define VSOMEIP_NOTIFY                          0x19
#define VSOMEIP_NOTIFY_ONE                      0x1A

#define VSOMEIP_REGISTER_EVENT                  0x1B
#define VSOMEIP_UNREGISTER_EVENT                0x1C
#define VSOMEIP_ID_RESPONSE                     0x1D
#define VSOMEIP_ID_REQUEST                      0x1E
#define VSOMEIP_OFFERED_SERVICES_REQUEST        0x1F
#define VSOMEIP_OFFERED_SERVICES_RESPONSE       0x20
#define VSOMEIP_UNSUBSCRIBE_ACK                 0x21
#define VSOMEIP_RESEND_PROVIDED_EVENTS          0x22

#define VSOMEIP_UPDATE_SECURITY_POLICY          0x23
#define VSOMEIP_UPDATE_SECURITY_POLICY_RESPONSE 0x24
#define VSOMEIP_REMOVE_SECURITY_POLICY          0x25
#define VSOMEIP_REMOVE_SECURITY_POLICY_RESPONSE 0x26
#define VSOMEIP_UPDATE_SECURITY_CREDENTIALS     0x27
#define VSOMEIP_DISTRIBUTE_SECURITY_POLICIES    0x28
#define VSOMEIP_UPDATE_SECURITY_POLICY_INT      0x29

#define VSOMEIP_SEND_COMMAND_SIZE               13
#define VSOMEIP_SEND_COMMAND_INSTANCE_POS_MIN   7
#define VSOMEIP_SEND_COMMAND_INSTANCE_POS_MAX   8
#define VSOMEIP_SEND_COMMAND_RELIABLE_POS       9
#define VSOMEIP_SEND_COMMAND_CHECK_STATUS_POS   10
#define VSOMEIP_SEND_COMMAND_DST_CLIENT_POS_MIN 11
#define VSOMEIP_SEND_COMMAND_DST_CLIENT_POS_MAX 12
#define VSOMEIP_SEND_COMMAND_PAYLOAD_POS        13

#define VSOMEIP_ASSIGN_CLIENT_ACK_COMMAND_SIZE  9
#define VSOMEIP_OFFER_SERVICE_COMMAND_SIZE      16
#define VSOMEIP_REQUEST_SERVICE_COMMAND_SIZE    16
#define VSOMEIP_RELEASE_SERVICE_COMMAND_SIZE    11
#define VSOMEIP_STOP_OFFER_SERVICE_COMMAND_SIZE 16
#define VSOMEIP_SUBSCRIBE_COMMAND_SIZE          18
#define VSOMEIP_SUBSCRIBE_ACK_COMMAND_SIZE      19
#define VSOMEIP_SUBSCRIBE_NACK_COMMAND_SIZE     19
#define VSOMEIP_UNSUBSCRIBE_COMMAND_SIZE        17
#define VSOMEIP_UNSUBSCRIBE_ACK_COMMAND_SIZE    15
#define VSOMEIP_REGISTER_EVENT_COMMAND_SIZE     16
#define VSOMEIP_UNREGISTER_EVENT_COMMAND_SIZE   14
#define VSOMEIP_OFFERED_SERVICES_COMMAND_SIZE    8
#define VSOMEIP_RESEND_PROVIDED_EVENTS_COMMAND_SIZE 11
#define VSOMEIP_REMOVE_SECURITY_POLICY_COMMAND_SIZE 19
#define VSOMEIP_UPDATE_SECURITY_POLICY_RESPONSE_COMMAND_SIZE 11
#define VSOMEIP_REMOVE_SECURITY_POLICY_RESPONSE_COMMAND_SIZE 11
#define VSOMEIP_PING_COMMAND_SIZE                7
#define VSOMEIP_PONG_COMMAND_SIZE                7
#define VSOMEIP_REGISTER_APPLICATION_COMMAND_SIZE 7
#define VSOMEIP_DEREGISTER_APPLICATION_COMMAND_SIZE 7
#define VSOMEIP_REGISTERED_ACK_COMMAND_SIZE      7

#include <pthread.h>

#define VSOMEIP_DATA_ID                         0x677D
#define VSOMEIP_DIAGNOSIS_ADDRESS               0x01

#define VSOMEIP_DEFAULT_SHM_PERMISSION          0666
#define VSOMEIP_DEFAULT_UDS_PERMISSIONS         0666

#define VSOMEIP_ROUTING_READY_MESSAGE           "SOME/IP routing ready."

namespace vsomeip_v3 {

typedef enum {
    RIE_ADD_CLIENT = 0x0,
    RIE_ADD_SERVICE_INSTANCE = 0x1,
    RIE_DEL_SERVICE_INSTANCE = 0x2,
    RIE_DEL_CLIENT = 0x3,
} routing_info_entry_e;

struct service_data_t {
    service_t service_;
    instance_t instance_;
    major_version_t major_;
    minor_version_t minor_;

    bool operator<(const service_data_t &_other) const {
        return (service_ < _other.service_
                || (service_ == _other.service_
                    && instance_ < _other.instance_));
    }
};

typedef enum {
    SUBSCRIPTION_ACKNOWLEDGED,
    SUBSCRIPTION_NOT_ACKNOWLEDGED,
    IS_SUBSCRIBING
} subscription_state_e;

const std::uint32_t MESSAGE_SIZE_UNLIMITED = (std::numeric_limits<std::uint32_t>::max)();

const std::uint32_t QUEUE_SIZE_UNLIMITED = (std::numeric_limits<std::uint32_t>::max)();

#define VSOMEIP_DEFAULT_NPDU_DEBOUNCING_NANO         2 * 1000 * 1000
#define VSOMEIP_DEFAULT_NPDU_MAXIMUM_RETENTION_NANO  5 * 1000 * 1000

const std::uint32_t MAX_RECONNECTS_UNLIMITED = (std::numeric_limits<std::uint32_t>::max)();

const std::uint32_t ANY_UID = 0xFFFFFFFF;
const std::uint32_t ANY_GID = 0xFFFFFFFF;

typedef std::pair<std::uint32_t, std::uint32_t> credentials_t;

enum class port_type_e {
    PT_OPTIONAL,
    PT_SECURE,
    PT_UNSECURE,
    PT_UNKNOWN
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_INTERNAL_HPP_
