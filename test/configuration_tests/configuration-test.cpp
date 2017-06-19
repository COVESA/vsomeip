// Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstdlib>
#include <iostream>

#include <vsomeip/constants.hpp>

#include "../implementation/configuration/include/configuration.hpp"
#include "../implementation/logging/include/logger.hpp"

#define CONFIGURATION_FILE              "configuration-test.json"
#define DEPRECATED_CONFIGURATION_FILE   "configuration-test-deprecated.json"

#define EXPECTED_UNICAST_ADDRESS        "10.0.2.15"

#define EXPECTED_HAS_CONSOLE            true
#define EXPECTED_HAS_FILE                true
#define EXPECTED_HAS_DLT                false
#define EXPECTED_LOGLEVEL                "debug"
#define EXPECTED_LOGFILE                "/home/someip/another-file.log"

#define EXPECTED_ROUTING_MANAGER_HOST    "my_application"

// Services
#define EXPECTED_UNICAST_ADDRESS_1234_0022                                  EXPECTED_UNICAST_ADDRESS
#define EXPECTED_RELIABLE_PORT_1234_0022                                    30506
#define EXPECTED_UNRELIABLE_PORT_1234_0022                                  31000

#define EXPECTED_UNICAST_ADDRESS_1234_0023                                  EXPECTED_UNICAST_ADDRESS
#define EXPECTED_RELIABLE_PORT_1234_0023                                    30503
#define EXPECTED_UNRELIABLE_PORT_1234_0023                                  vsomeip::ILLEGAL_PORT

#define EXPECTED_UNICAST_ADDRESS_2277_0022                                  EXPECTED_UNICAST_ADDRESS
#define EXPECTED_RELIABLE_PORT_2277_0022                                    30505
#define EXPECTED_UNRELIABLE_PORT_2277_0022                                  31001

#define EXPECTED_UNICAST_ADDRESS_2266_0022                                  EXPECTED_UNICAST_ADDRESS
#define EXPECTED_RELIABLE_PORT_2266_0022                                    30505
#define EXPECTED_UNRELIABLE_PORT_2266_0022                                  30507

#define EXPECTED_UNICAST_ADDRESS_4466_0321                                  "10.0.2.23"
#define EXPECTED_RELIABLE_PORT_4466_0321                                    30506
#define EXPECTED_UNRELIABLE_PORT_4466_0321                                  30444

// Service Discovery
#define EXPECTED_SD_ENABLED                                                 true
#define EXPECTED_SD_PROTOCOL                                                "udp"
#define EXPECTED_SD_MULTICAST                                               "224.212.244.223"
#define EXPECTED_SD_PORT                                                    30666

#define EXPECTED_INITIAL_DELAY_MIN                                          1234
#define EXPECTED_INITIAL_DELAY_MAX                                          2345
#define EXPECTED_REPETITIONS_BASE_DELAY                                     4242
#define EXPECTED_REPETITIONS_MAX                                            4
#define EXPECTED_TTL                                                        13
#define EXPECTED_CYCLIC_OFFER_DELAY                                         2132
#define EXPECTED_REQUEST_RESPONSE_DELAY                                     1111

#define EXPECTED_DEPRECATED_INITIAL_DELAY_MIN                               10
#define EXPECTED_DEPRECATED_INITIAL_DELAY_MAX                               100
#define EXPECTED_DEPRECATED_REPETITIONS_BASE_DELAY                          200
#define EXPECTED_DEPRECATED_REPETITIONS_MAX                                 7
#define EXPECTED_DEPRECATED_TTL                                             5
#define EXPECTED_DEPRECATED_REQUEST_RESPONSE_DELAY                          2001

template<class T>
void check(const T &_is, const T &_expected, const std::string &_test) {
    if (_is == _expected) {
        VSOMEIP_INFO << "Test \"" << _test << "\" succeeded.";
    } else {
        VSOMEIP_ERROR << "Test \"" << _test << "\" failed! ("
                      << _is << " != " << _expected << ")";
    }
}

void check_file(const std::string &_config_file,
                const std::string &_expected_unicast_address,
                bool _expected_has_console,
                bool _expected_has_file,
                bool _expected_has_dlt,
                const std::string &_expected_logfile,
                const std::string &_expected_loglevel,
                const std::string &_expected_unicast_address_1234_0022,
                uint16_t _expected_reliable_port_1234_0022,
                uint16_t _expected_unreliable_port_1234_0022,
                const std::string &_expected_unicast_address_1234_0023,
                uint16_t _expected_reliable_port_1234_0023,
                uint16_t _expected_unreliable_port_1234_0023,
                const std::string &_expected_unicast_address_2277_0022,
                uint16_t _expected_reliable_port_2277_0022,
                uint16_t _expected_unreliable_port_2277_0022,
                const std::string &_expected_unicast_address_2266_0022,
                uint16_t _expected_reliable_port_2266_0022,
                uint16_t _expected_unreliable_port_2266_0022,
                const std::string &_expected_unicast_address_4466_0321,
                uint16_t _expected_reliable_port_4466_0321,
                uint16_t _expected_unreliable_port_4466_0321,
                bool _expected_enabled,
                const std::string &_expected_protocol,
                const std::string &_expected_multicast,
                uint16_t _expected_port,
                int32_t _expected_initial_delay_min,
                int32_t _expected_initial_delay_max,
                int32_t _expected_repetitions_base_delay,
                uint8_t _expected_repetitions_max,
                vsomeip::ttl_t _expected_ttl,
                vsomeip::ttl_t _expected_cyclic_offer_delay,
                vsomeip::ttl_t _expected_request_response_delay) {

    // 0. Create configuration object
    std::shared_ptr<vsomeip::configuration> its_configuration
            = vsomeip::configuration::get();

    // 1. Did we get a configuration object?
    if (0 == its_configuration) {
        VSOMEIP_ERROR << "No configuration object. "
                "Either memory overflow or loading error detected!";
        return;
    }

    // 2. Set environment variable to config file and load it
#ifndef _WIN32
    setenv("VSOMEIP_CONFIGURATION", _config_file.c_str(), 1);
#else
    _putenv_s("VSOMEIP_CONFIGURATION", _config_file.c_str()
#endif
    its_configuration->load(EXPECTED_ROUTING_MANAGER_HOST);

    // 3. Check host address
    boost::asio::ip::address its_host_unicast_address
        = its_configuration->get_unicast_address();
    check<std::string>(its_host_unicast_address.to_string(),
                       _expected_unicast_address, "UNICAST ADDRESS");

    // 4. Check logging
    bool has_console = its_configuration->has_console_log();
    bool has_file = its_configuration->has_file_log();
    bool has_dlt = its_configuration->has_dlt_log();
    std::string logfile = its_configuration->get_logfile();
    boost::log::trivial::severity_level loglevel
        = its_configuration->get_loglevel();

    check<bool>(has_console, _expected_has_console, "HAS CONSOLE");
    check<bool>(has_file, _expected_has_file, "HAS FILE");
    check<bool>(has_dlt, _expected_has_dlt, "HAS DLT");
    check<std::string>(logfile, _expected_logfile, "LOGFILE");
    check<std::string>(boost::log::trivial::to_string(loglevel),
                       _expected_loglevel, "LOGLEVEL");

    // 5. Services
    std::string its_unicast_address
        = its_configuration->get_unicast_address(0x1234, 0x0022);
    uint16_t its_reliable_port
        = its_configuration->get_reliable_port(0x1234, 0x0022);
    uint16_t its_unreliable_port
        = its_configuration->get_unreliable_port(0x1234, 0x0022);

    check<std::string>(its_unicast_address,
            _expected_unicast_address_1234_0022,
            "UNICAST_ADDRESS_1234_0022");
    check<uint16_t>(its_reliable_port,
            _expected_reliable_port_1234_0022,
            "RELIABLE_PORT_1234_0022");
    check<uint16_t>(its_unreliable_port,
            _expected_unreliable_port_1234_0022,
            "UNRELIABLE_PORT_1234_0022");

    its_unicast_address
        = its_configuration->get_unicast_address(0x1234, 0x0023);
    its_reliable_port
        = its_configuration->get_reliable_port(0x1234, 0x0023);
    its_unreliable_port
        = its_configuration->get_unreliable_port(0x1234, 0x0023);

    check<std::string>(its_unicast_address,
            _expected_unicast_address_1234_0023,
            "UNICAST_ADDRESS_1234_0023");
    check<uint16_t>(its_reliable_port,
            _expected_reliable_port_1234_0023,
            "RELIABLE_PORT_1234_0023");
    check<uint16_t>(its_unreliable_port,
            _expected_unreliable_port_1234_0023,
            "UNRELIABLE_PORT_1234_0023");

    its_unicast_address
        = its_configuration->get_unicast_address(0x2277, 0x0022);
    its_reliable_port
        = its_configuration->get_reliable_port(0x2277, 0x0022);
    its_unreliable_port
        = its_configuration->get_unreliable_port(0x2277, 0x0022);

    check<std::string>(its_unicast_address,
            _expected_unicast_address_2277_0022,
            "UNICAST_ADDRESS_2277_0022");
    check<uint16_t>(its_reliable_port,
            _expected_reliable_port_2277_0022,
            "RELIABLE_PORT_2277_0022");
    check<uint16_t>(its_unreliable_port,
            _expected_unreliable_port_2277_0022,
            "UNRELIABLE_PORT_2277_0022");

    its_unicast_address
        = its_configuration->get_unicast_address(0x2266, 0x0022);
    its_reliable_port
        = its_configuration->get_reliable_port(0x2266, 0x0022);
    its_unreliable_port
        = its_configuration->get_unreliable_port(0x2266, 0x0022);

    check<std::string>(its_unicast_address,
            _expected_unicast_address_2266_0022,
            "UNICAST_ADDRESS_2266_0022");
    check<uint16_t>(its_reliable_port,
            _expected_reliable_port_2266_0022,
            "RELIABLE_PORT_2266_0022");
    check<uint16_t>(its_unreliable_port,
            _expected_unreliable_port_2266_0022,
            "UNRELIABLE_PORT_2266_0022");

    its_unicast_address
        = its_configuration->get_unicast_address(0x4466, 0x0321);
    its_reliable_port
        = its_configuration->get_reliable_port(0x4466, 0x0321);
    its_unreliable_port
        = its_configuration->get_unreliable_port(0x4466, 0x0321);

    check<std::string>(its_unicast_address,
            _expected_unicast_address_4466_0321,
            "UNICAST_ADDRESS_4466_0321");
    check<uint16_t>(its_reliable_port,
            _expected_reliable_port_4466_0321,
            "RELIABLE_PORT_4466_0321");
    check<uint16_t>(its_unreliable_port,
            _expected_unreliable_port_4466_0321,
            "UNRELIABLE_PORT_4466_0321");

    // payload sizes
    // use 17000 instead of 1500 as configured max-local-payload size will be
    // increased to bigger max-reliable-payload-size
    std::uint32_t max_local_message_size(
            17000u + 16u + + VSOMEIP_COMMAND_HEADER_SIZE
                    + sizeof(vsomeip::instance_t) + sizeof(bool) + sizeof(bool)
                    + sizeof(vsomeip::client_t));
    check<std::uint32_t>(max_local_message_size, its_configuration->get_max_message_size_local(), "max-local-message-size");
    check<std::uint32_t>(11u, its_configuration->get_buffer_shrink_threshold(), "buffer shrink threshold");
    check<std::uint32_t>(14999u + 16u, its_configuration->get_max_message_size_reliable("10.10.10.10", 7777), "max-message-size-reliable1");
    check<std::uint32_t>(17000u + 16, its_configuration->get_max_message_size_reliable("11.11.11.11", 4711), "max-message-size-reliable2");
    check<std::uint32_t>(15001u + 16, its_configuration->get_max_message_size_reliable("10.10.10.11", 7778), "max-message-size-reliable3");

    // 6. Service discovery
    bool enabled = its_configuration->is_sd_enabled();
    std::string protocol = its_configuration->get_sd_protocol();
    uint16_t port = its_configuration->get_sd_port();
    std::string multicast = its_configuration->get_sd_multicast();

    int32_t initial_delay_min = its_configuration->get_sd_initial_delay_min();
    int32_t initial_delay_max = its_configuration->get_sd_initial_delay_max();
    int32_t repetitions_base_delay = its_configuration->get_sd_repetitions_base_delay();
    uint8_t repetitions_max = its_configuration->get_sd_repetitions_max();
    vsomeip::ttl_t ttl = its_configuration->get_sd_ttl();
    int32_t cyclic_offer_delay = its_configuration->get_sd_cyclic_offer_delay();
    int32_t request_response_delay = its_configuration->get_sd_request_response_delay();

    check<bool>(enabled, _expected_enabled, "SD ENABLED");
    check<std::string>(protocol, _expected_protocol, "SD PROTOCOL");
    check<std::string>(multicast, _expected_multicast, "SD MULTICAST");
    check<uint16_t>(port, _expected_port, "SD PORT");

    check<int32_t>(initial_delay_min, _expected_initial_delay_min, "SD INITIAL DELAY MIN");
    check<int32_t>(initial_delay_max, _expected_initial_delay_max, "SD INITIAL DELAY MAX");
    check<int32_t>(repetitions_base_delay, _expected_repetitions_base_delay, "SD REPETITION BASE DELAY");
    check<uint8_t>(repetitions_max,_expected_repetitions_max, "SD REPETITION MAX");
    check<vsomeip::ttl_t>(ttl, _expected_ttl, "SD TTL");
    check<int32_t>(cyclic_offer_delay, _expected_cyclic_offer_delay, "SD CYCLIC OFFER DELAY");
    check<int32_t>(request_response_delay, _expected_request_response_delay, "SD RESPONSE REQUEST DELAY");
}



int main() {
    // Check current configuration file format
    std::cout << "/////////////////////////////////" << std::endl
              << "// CHECKING CONFIGURATION FILE //" << std::endl
              << "/////////////////////////////////" << std::endl;
    check_file(CONFIGURATION_FILE,
               EXPECTED_UNICAST_ADDRESS,
               EXPECTED_HAS_CONSOLE,
               EXPECTED_HAS_FILE,
               EXPECTED_HAS_DLT,
               EXPECTED_LOGFILE,
               EXPECTED_LOGLEVEL,
               EXPECTED_UNICAST_ADDRESS_1234_0022,
               EXPECTED_RELIABLE_PORT_1234_0022,
               EXPECTED_UNRELIABLE_PORT_1234_0022,
               EXPECTED_UNICAST_ADDRESS_1234_0023,
               EXPECTED_RELIABLE_PORT_1234_0023,
               EXPECTED_UNRELIABLE_PORT_1234_0023,
               EXPECTED_UNICAST_ADDRESS_2277_0022,
               EXPECTED_RELIABLE_PORT_2277_0022,
               EXPECTED_UNRELIABLE_PORT_2277_0022,
               EXPECTED_UNICAST_ADDRESS_2266_0022,
               EXPECTED_RELIABLE_PORT_2266_0022,
               EXPECTED_UNRELIABLE_PORT_2266_0022,
               EXPECTED_UNICAST_ADDRESS_4466_0321,
               EXPECTED_RELIABLE_PORT_4466_0321,
               EXPECTED_UNRELIABLE_PORT_4466_0321,
               EXPECTED_SD_ENABLED,
               EXPECTED_SD_PROTOCOL,
               EXPECTED_SD_MULTICAST,
               EXPECTED_SD_PORT,
               EXPECTED_INITIAL_DELAY_MIN,
               EXPECTED_INITIAL_DELAY_MAX,
               EXPECTED_REPETITIONS_BASE_DELAY,
               EXPECTED_REPETITIONS_MAX,
               EXPECTED_TTL,
               EXPECTED_CYCLIC_OFFER_DELAY,
               EXPECTED_REQUEST_RESPONSE_DELAY);

    // Check deprecated configuration file format
    std::cout << "////////////////////////////////////////////" << std::endl
              << "// CHECKING DEPRECATED CONFIGURATION FILE //" << std::endl
              << "////////////////////////////////////////////" << std::endl;
    check_file(DEPRECATED_CONFIGURATION_FILE,
               EXPECTED_UNICAST_ADDRESS,
               EXPECTED_HAS_CONSOLE,
               EXPECTED_HAS_FILE,
               EXPECTED_HAS_DLT,
               EXPECTED_LOGFILE,
               EXPECTED_LOGLEVEL,
               EXPECTED_UNICAST_ADDRESS_1234_0022,
               EXPECTED_RELIABLE_PORT_1234_0022,
               EXPECTED_UNRELIABLE_PORT_1234_0022,
               EXPECTED_UNICAST_ADDRESS_1234_0023,
               EXPECTED_RELIABLE_PORT_1234_0023,
               EXPECTED_UNRELIABLE_PORT_1234_0023,
               EXPECTED_UNICAST_ADDRESS_2277_0022,
               EXPECTED_RELIABLE_PORT_2277_0022,
               EXPECTED_UNRELIABLE_PORT_2277_0022,
               EXPECTED_UNICAST_ADDRESS_2266_0022,
               EXPECTED_RELIABLE_PORT_2266_0022,
               EXPECTED_UNRELIABLE_PORT_2266_0022,
               EXPECTED_UNICAST_ADDRESS_4466_0321,
               EXPECTED_RELIABLE_PORT_4466_0321,
               EXPECTED_UNRELIABLE_PORT_4466_0321,
               EXPECTED_SD_ENABLED,
               EXPECTED_SD_PROTOCOL,
               EXPECTED_SD_MULTICAST,
               EXPECTED_SD_PORT,
               EXPECTED_DEPRECATED_INITIAL_DELAY_MIN,
               EXPECTED_DEPRECATED_INITIAL_DELAY_MAX,
               EXPECTED_DEPRECATED_REPETITIONS_BASE_DELAY,
               EXPECTED_DEPRECATED_REPETITIONS_MAX,
               EXPECTED_DEPRECATED_TTL,
               EXPECTED_CYCLIC_OFFER_DELAY,
               EXPECTED_DEPRECATED_REQUEST_RESPONSE_DELAY);

    return 0;
}



