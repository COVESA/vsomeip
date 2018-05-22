// Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstdlib>
#include <iostream>

#include <gtest/gtest.h>

#include <vsomeip/constants.hpp>
#include <vsomeip/plugins/application_plugin.hpp>

#include "../implementation/configuration/include/configuration.hpp"
#include "../implementation/configuration/include/configuration_impl.hpp"
#include "../implementation/logging/include/logger.hpp"

#include "../implementation/plugin/include/plugin_manager.hpp"

#define CONFIGURATION_FILE              "configuration-test.json"
#define DEPRECATED_CONFIGURATION_FILE   "configuration-test-deprecated.json"

#define EXPECTED_UNICAST_ADDRESS        "10.0.2.15"

#define EXPECTED_HAS_CONSOLE            true
#define EXPECTED_HAS_FILE                true
#define EXPECTED_HAS_DLT                false
#define EXPECTED_LOGLEVEL                "debug"
#define EXPECTED_LOGFILE                "/home/someip/another-file.log"

#define EXPECTED_ROUTING_MANAGER_HOST    "my_application"

// Logging
#define EXPECTED_VERSION_LOGGING_ENABLED                                    false
#define EXPECTED_VERSION_LOGGING_INTERVAL                                   15

// Application
#define EXPECTED_APPLICATION_MAX_DISPATCHERS                                25
#define EXPECTED_APPLICATION_MAX_DISPATCH_TIME                              1234
#define EXPECTED_APPLICATION_THREADS                                        12
#define EXPECTED_APPLICATION_REQUEST_DEBOUNCE_TIME                          5000

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
::testing::AssertionResult check(const T &_is, const T &_expected, const std::string &_test) {
    if (_is == _expected) {
        return ::testing::AssertionSuccess() << "Test \"" << _test << "\" succeeded.";
    } else {
        return ::testing::AssertionFailure() << "Test \"" << _test << "\" failed! ("
                      << _is << " != " << _expected << ")";
    }
}

void check_file(const std::string &_config_file,
                const std::string &_expected_unicast_address,
                bool _expected_has_console,
                bool _expected_has_file,
                bool _expected_has_dlt,
                bool _expected_version_logging_enabled,
                uint32_t _expected_version_logging_interval,
                uint32_t _expected_application_max_dispatcher,
                uint32_t _expected_application_max_dispatch_time,
                uint32_t _expected_application_threads,
                uint32_t _expected_application_request_debounce_time,
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
    std::shared_ptr<vsomeip::configuration> its_configuration;
    auto its_plugin = vsomeip::plugin_manager::get()->get_plugin(
            vsomeip::plugin_type_e::CONFIGURATION_PLUGIN, VSOMEIP_CFG_LIBRARY);
    if (its_plugin) {
        its_configuration = std::dynamic_pointer_cast<vsomeip::configuration>(its_plugin);
    }

    // 1. Did we get a configuration object?
    if (0 == its_configuration) {
        ADD_FAILURE() << "No configuration object. "
                "Either memory overflow or loading error detected!";
        return;
    }

    vsomeip::cfg::configuration_impl its_copied_config(
            static_cast<vsomeip::cfg::configuration_impl&>(*its_configuration));
    vsomeip::cfg::configuration_impl* its_new_config =
            new vsomeip::cfg::configuration_impl(its_copied_config);
    delete its_new_config;

    // 2. Set environment variable to config file and load it
#ifndef _WIN32
    setenv("VSOMEIP_CONFIGURATION", _config_file.c_str(), 1);
#else
    _putenv_s("VSOMEIP_CONFIGURATION", _config_file.c_str()
#endif
    its_configuration->load(EXPECTED_ROUTING_MANAGER_HOST);

    its_configuration->set_configuration_path("/my/test/path");

    // 3. Check host address
    boost::asio::ip::address its_host_unicast_address
        = its_configuration->get_unicast_address();
    EXPECT_TRUE(check<std::string>(its_host_unicast_address.to_string(),
                       _expected_unicast_address, "UNICAST ADDRESS"));
    EXPECT_TRUE(its_configuration->is_v4());
    EXPECT_FALSE(its_configuration->is_v6());

    // check diagnosis prefix
    EXPECT_NE(0x54, its_configuration->get_diagnosis_address());
    EXPECT_EQ(0x55, its_configuration->get_diagnosis_address());
    EXPECT_NE(0x56, its_configuration->get_diagnosis_address());

    // 4. Check logging
    bool has_console = its_configuration->has_console_log();
    bool has_file = its_configuration->has_file_log();
    bool has_dlt = its_configuration->has_dlt_log();
    std::string logfile = its_configuration->get_logfile();
    boost::log::trivial::severity_level loglevel
        = its_configuration->get_loglevel();
    bool has_version_logging = its_configuration->log_version();
    std::uint32_t version_logging_interval = its_configuration->get_log_version_interval();

    EXPECT_TRUE(check<bool>(has_console, _expected_has_console, "HAS CONSOLE"));
    EXPECT_TRUE(check<bool>(has_file, _expected_has_file, "HAS FILE"));
    EXPECT_TRUE(check<bool>(has_dlt, _expected_has_dlt, "HAS DLT"));
    EXPECT_TRUE(check<std::string>(logfile, _expected_logfile, "LOGFILE"));
    EXPECT_TRUE(check<std::string>(boost::log::trivial::to_string(loglevel),
                       _expected_loglevel, "LOGLEVEL"));
    EXPECT_TRUE(check<bool>(has_version_logging, _expected_version_logging_enabled,
                    "VERSION LOGGING"));
    EXPECT_TRUE(check<uint32_t>(version_logging_interval,
                    _expected_version_logging_interval,
                    "VERSION LOGGING INTERVAL"));

    // watchdog
    EXPECT_TRUE(its_configuration->is_watchdog_enabled());
    EXPECT_EQ(1234u, its_configuration->get_watchdog_timeout());
    EXPECT_EQ(7u, its_configuration->get_allowed_missing_pongs());

    // file permissions
    EXPECT_EQ(0444u, its_configuration->get_permissions_shm());
    EXPECT_EQ(0222u, its_configuration->get_umask());

    // selective broadcasts
    EXPECT_TRUE(its_configuration->supports_selective_broadcasts(
            boost::asio::ip::address::from_string("160.160.160.160")));

    // tracing
    std::shared_ptr<vsomeip::cfg::trace> its_trace = its_configuration->get_trace();
    EXPECT_TRUE(its_trace->is_enabled_);
    EXPECT_TRUE(its_trace->is_sd_enabled_);
    EXPECT_EQ(2u, its_trace->channels_.size());
    EXPECT_EQ(2u, its_trace->filter_rules_.size());
    for (const auto &c : its_trace->channels_) {
        EXPECT_TRUE(c->name_ == std::string("testname") || c->name_ == std::string("testname2"));
        if (c->name_ == std::string("testname")) {
            EXPECT_EQ(std::string("testid"), c->id_);
        } else if (c->name_ == std::string("testname2")) {
            EXPECT_EQ(std::string("testid2"), c->id_);
        }
    }
    for (const auto &f : its_trace->filter_rules_) {
        EXPECT_TRUE(f->channel_ == std::string("testname") || f->channel_ == std::string("testname2"));
        if (f->channel_ == std::string("testname")) {
            EXPECT_EQ(2u, f->services_.size());
            EXPECT_EQ(2u, f->methods_.size());
            EXPECT_EQ(2u, f->clients_.size());
            for (const vsomeip::service_t s : f->services_) {
                EXPECT_TRUE(s == vsomeip::service_t(0x1111) || s == vsomeip::service_t(2222));
            }
            for (const vsomeip::method_t s : f->methods_) {
                EXPECT_TRUE(s == vsomeip::method_t(0x1111) || s == vsomeip::method_t(2222));
            }
            for (const vsomeip::client_t s : f->clients_) {
                EXPECT_TRUE(s == vsomeip::client_t(0x1111) || s == vsomeip::client_t(2222));
            }
        } else if (f->channel_ == std::string("testname2")) {
            EXPECT_EQ(2u, f->services_.size());
            EXPECT_EQ(2u, f->methods_.size());
            EXPECT_EQ(2u, f->clients_.size());
            for (const vsomeip::service_t s : f->services_) {
                EXPECT_TRUE(s == vsomeip::service_t(0x3333) || s == vsomeip::service_t(4444));
            }
            for (const vsomeip::method_t s : f->methods_) {
                EXPECT_TRUE(s == vsomeip::method_t(0x3333) || s == vsomeip::method_t(4444));
            }
            for (const vsomeip::client_t s : f->clients_) {
                EXPECT_TRUE(s == vsomeip::client_t(0x3333) || s == vsomeip::client_t(4444));
            }
        }
    }

    // Applications
    std::size_t max_dispatchers = its_configuration->get_max_dispatchers(
            EXPECTED_ROUTING_MANAGER_HOST);
    std::size_t max_dispatch_time = its_configuration->get_max_dispatch_time(
            EXPECTED_ROUTING_MANAGER_HOST);
    std::size_t io_threads = its_configuration->get_io_thread_count(
            EXPECTED_ROUTING_MANAGER_HOST);
    std::size_t request_time = its_configuration->get_request_debouncing(
            EXPECTED_ROUTING_MANAGER_HOST);

    EXPECT_TRUE(check<std::size_t>(max_dispatchers,
            _expected_application_max_dispatcher, "MAX DISPATCHERS"));
    EXPECT_TRUE(check<std::size_t>(max_dispatch_time,
            _expected_application_max_dispatch_time, "MAX DISPATCH TIME"));
    EXPECT_TRUE(check<std::size_t>(io_threads, _expected_application_threads,
            "IO THREADS"));
    EXPECT_TRUE(check<std::size_t>(request_time,
            _expected_application_request_debounce_time, "REQUEST DEBOUNCE TIME"));

    EXPECT_EQ(0x9933, its_configuration->get_id("other_application"));

    std::map<vsomeip::plugin_type_e, std::set<std::string>> its_plugins =
            its_configuration->get_plugins(EXPECTED_ROUTING_MANAGER_HOST);
    EXPECT_EQ(1u, its_plugins.size());
    for (const auto plugin : its_plugins) {
        EXPECT_EQ(vsomeip::plugin_type_e::APPLICATION_PLUGIN, plugin.first);
        for (auto its_library : plugin.second)
            EXPECT_EQ(std::string("libtestlibraryname.so." + std::to_string(VSOMEIP_APPLICATION_PLUGIN_VERSION)), its_library);
    }
    EXPECT_EQ(vsomeip::plugin_type_e::CONFIGURATION_PLUGIN, its_plugin->get_plugin_type());
    EXPECT_EQ("vsomeip cfg plugin", its_plugin->get_plugin_name());
    EXPECT_EQ(1u, its_plugin->get_plugin_version());


    // 5. Services
    std::string its_unicast_address
        = its_configuration->get_unicast_address(0x1234, 0x0022);
    uint16_t its_reliable_port
        = its_configuration->get_reliable_port(0x1234, 0x0022);
    uint16_t its_unreliable_port
        = its_configuration->get_unreliable_port(0x1234, 0x0022);

    EXPECT_TRUE(check<std::string>(its_unicast_address,
            _expected_unicast_address_1234_0022,
            "UNICAST_ADDRESS_1234_0022"));
    EXPECT_TRUE(check<uint16_t>(its_reliable_port,
            _expected_reliable_port_1234_0022,
            "RELIABLE_PORT_1234_0022"));
    EXPECT_TRUE(check<uint16_t>(its_unreliable_port,
            _expected_unreliable_port_1234_0022,
            "UNRELIABLE_PORT_1234_0022"));

    its_unicast_address
        = its_configuration->get_unicast_address(0x1234, 0x0023);
    its_reliable_port
        = its_configuration->get_reliable_port(0x1234, 0x0023);
    its_unreliable_port
        = its_configuration->get_unreliable_port(0x1234, 0x0023);

    EXPECT_TRUE(check<std::string>(its_unicast_address,
            _expected_unicast_address_1234_0023,
            "UNICAST_ADDRESS_1234_0023"));
    EXPECT_TRUE(check<uint16_t>(its_reliable_port,
            _expected_reliable_port_1234_0023,
            "RELIABLE_PORT_1234_0023"));
    EXPECT_TRUE(check<uint16_t>(its_unreliable_port,
            _expected_unreliable_port_1234_0023,
            "UNRELIABLE_PORT_1234_0023"));

    its_unicast_address
        = its_configuration->get_unicast_address(0x2277, 0x0022);
    its_reliable_port
        = its_configuration->get_reliable_port(0x2277, 0x0022);
    its_unreliable_port
        = its_configuration->get_unreliable_port(0x2277, 0x0022);

    EXPECT_TRUE(check<std::string>(its_unicast_address,
            _expected_unicast_address_2277_0022,
            "UNICAST_ADDRESS_2277_0022"));
    EXPECT_TRUE(check<uint16_t>(its_reliable_port,
            _expected_reliable_port_2277_0022,
            "RELIABLE_PORT_2277_0022"));
    EXPECT_TRUE(check<uint16_t>(its_unreliable_port,
            _expected_unreliable_port_2277_0022,
            "UNRELIABLE_PORT_2277_0022"));

    its_unicast_address
        = its_configuration->get_unicast_address(0x2266, 0x0022);
    its_reliable_port
        = its_configuration->get_reliable_port(0x2266, 0x0022);
    its_unreliable_port
        = its_configuration->get_unreliable_port(0x2266, 0x0022);

    EXPECT_TRUE(check<std::string>(its_unicast_address,
            _expected_unicast_address_2266_0022,
            "UNICAST_ADDRESS_2266_0022"));
    EXPECT_TRUE(check<uint16_t>(its_reliable_port,
            _expected_reliable_port_2266_0022,
            "RELIABLE_PORT_2266_0022"));
    EXPECT_TRUE(check<uint16_t>(its_unreliable_port,
            _expected_unreliable_port_2266_0022,
            "UNRELIABLE_PORT_2266_0022"));

    its_unicast_address
        = its_configuration->get_unicast_address(0x4466, 0x0321);
    its_reliable_port
        = its_configuration->get_reliable_port(0x4466, 0x0321);
    its_unreliable_port
        = its_configuration->get_unreliable_port(0x4466, 0x0321);

    EXPECT_TRUE(check<std::string>(its_unicast_address,
            _expected_unicast_address_4466_0321,
            "UNICAST_ADDRESS_4466_0321"));
    EXPECT_TRUE(check<uint16_t>(its_reliable_port,
            _expected_reliable_port_4466_0321,
            "RELIABLE_PORT_4466_0321"));
    EXPECT_TRUE(check<uint16_t>(its_unreliable_port,
            _expected_unreliable_port_4466_0321,
            "UNRELIABLE_PORT_4466_0321"));

    std::string its_multicast_address;
    std::uint16_t its_multicast_port;
    its_configuration->get_multicast(0x7809, 0x1, 0x1111,
            its_multicast_address, its_multicast_port);
    EXPECT_EQ(1234u, its_multicast_port);
    EXPECT_EQ(std::string("224.212.244.225"), its_multicast_address);
    EXPECT_EQ(8u, its_configuration->get_threshold(0x7809, 0x1, 0x1111));

    EXPECT_TRUE(its_configuration->is_offered_remote(0x1234,0x0022));
    EXPECT_FALSE(its_configuration->is_offered_remote(0x3333,0x1));

    EXPECT_TRUE(its_configuration->has_enabled_magic_cookies("10.0.2.15", 30506));
    EXPECT_FALSE(its_configuration->has_enabled_magic_cookies("10.0.2.15", 30503));

    std::set<std::pair<vsomeip::service_t, vsomeip::instance_t>> its_remote_services =
            its_configuration->get_remote_services();
    EXPECT_EQ(1u, its_remote_services.size());
    for (const auto &p : its_remote_services) {
        EXPECT_EQ(0x4466, p.first);
        EXPECT_EQ(0x321, p.second);
    }

    EXPECT_TRUE(its_configuration->is_someip(0x3333,0x1));
    EXPECT_FALSE(its_configuration->is_someip(0x3555,0x1));

    // Internal services
    EXPECT_TRUE(its_configuration->is_local_service(0x1234, 0x0022));
    EXPECT_TRUE(its_configuration->is_local_service(0x3333,0x1));
    // defined range, service level only
    EXPECT_FALSE(its_configuration->is_local_service(0xF0FF,0x1));
    EXPECT_TRUE(its_configuration->is_local_service(0xF100,0x1));
    EXPECT_TRUE(its_configuration->is_local_service(0xF101,0x23));
    EXPECT_TRUE(its_configuration->is_local_service(0xF109,0xFFFF));
    EXPECT_FALSE(its_configuration->is_local_service(0xF10a,0x1));
    // defined range, service and instance level
    EXPECT_FALSE(its_configuration->is_local_service(0xF2FF,0xFFFF));
    EXPECT_TRUE(its_configuration->is_local_service(0xF300,0x1));
    EXPECT_TRUE(its_configuration->is_local_service(0xF300,0x5));
    EXPECT_TRUE(its_configuration->is_local_service(0xF300,0x10));
    EXPECT_FALSE(its_configuration->is_local_service(0xF300,0x11));
    EXPECT_FALSE(its_configuration->is_local_service(0xF301,0x11));

    // clients
    std::map<bool, std::set<uint16_t>> used_ports;
    used_ports[true].insert(0x11);
    used_ports[false].insert(0x10);
    std::uint16_t port_to_use(0x0);
    EXPECT_TRUE(its_configuration->get_client_port(0x8888, 0x1, vsomeip::ILLEGAL_PORT, true, used_ports, port_to_use));
    EXPECT_EQ(0x10, port_to_use);
    EXPECT_TRUE(its_configuration->get_client_port(0x8888, 0x1, vsomeip::ILLEGAL_PORT, false, used_ports, port_to_use));
    EXPECT_EQ(0x11, port_to_use);

    used_ports[true].insert(0x10);
    used_ports[false].insert(0x11);
    EXPECT_FALSE(its_configuration->get_client_port(0x8888, 0x1, vsomeip::ILLEGAL_PORT, true, used_ports, port_to_use));
    EXPECT_EQ(vsomeip::ILLEGAL_PORT, port_to_use);
    EXPECT_FALSE(its_configuration->get_client_port(0x8888, 0x1, vsomeip::ILLEGAL_PORT, false, used_ports, port_to_use));
    EXPECT_EQ(vsomeip::ILLEGAL_PORT, port_to_use);


    //check for correct client port assignment if service / instance was not configured but a remote port range
    used_ports.clear();
    EXPECT_TRUE(its_configuration->get_client_port(0x8888, 0x12, 0x7725, true, used_ports, port_to_use));
    EXPECT_EQ(0x771B, port_to_use);
    used_ports[true].insert(0x771B);
    EXPECT_TRUE(its_configuration->get_client_port(0x8888, 0x12, 0x7725, true, used_ports, port_to_use));
    EXPECT_EQ(0x771C, port_to_use);
    used_ports[true].insert(0x771C);
    EXPECT_TRUE(its_configuration->get_client_port(0x8888, 0x12, 0x7B0D, true, used_ports, port_to_use));
    EXPECT_EQ(0x7B03, port_to_use);
    used_ports[true].insert(0x7B03);
    EXPECT_TRUE(its_configuration->get_client_port(0x8888, 0x12, 0x7B0D, true, used_ports, port_to_use));
    EXPECT_EQ(0x7B04, port_to_use);
    used_ports[true].insert(0x7B04);
    EXPECT_TRUE(its_configuration->get_client_port(0x8888, 0x12, 0x7EF4, true, used_ports, port_to_use));
    EXPECT_EQ(0x7EEB, port_to_use);
    used_ports[true].insert(0x7EEB);
    EXPECT_TRUE(its_configuration->get_client_port(0x8888, 0x12, 0x7EF4, true, used_ports, port_to_use));
    EXPECT_EQ(0x7EEC, port_to_use);
    used_ports[true].insert(0x7EEC);
    used_ports.clear();


    // payload sizes
    // use 17000 instead of 1500 as configured max-local-payload size will be
    // increased to bigger max-reliable-payload-size
    std::uint32_t max_local_message_size(
            17000u + 16u + + VSOMEIP_SEND_COMMAND_SIZE);
    EXPECT_EQ(max_local_message_size, its_configuration->get_max_message_size_local());
    EXPECT_EQ(11u, its_configuration->get_buffer_shrink_threshold());
    EXPECT_EQ(14999u + 16u, its_configuration->get_max_message_size_reliable("10.10.10.10", 7777));
    EXPECT_EQ(17000u + 16, its_configuration->get_max_message_size_reliable("11.11.11.11", 4711));
    EXPECT_EQ(15001u + 16, its_configuration->get_max_message_size_reliable("10.10.10.11", 7778));

    // security
    EXPECT_TRUE(its_configuration->is_security_enabled());
    EXPECT_TRUE(its_configuration->is_offer_allowed(0x1277, 0x1234, 0x5678));
    EXPECT_FALSE(its_configuration->is_offer_allowed(0x1277, 0x1234, 0x5679));
    EXPECT_FALSE(its_configuration->is_offer_allowed(0x1277, 0x1233, 0x5679));
    EXPECT_FALSE(its_configuration->is_offer_allowed(0x1266, 0x1233, 0x5679));
    // explicitly denied offers
    EXPECT_FALSE(its_configuration->is_offer_allowed(0x1443, 0x1234, 0x5678));
    EXPECT_FALSE(its_configuration->is_offer_allowed(0x1443, 0x1235, 0x5678));
    EXPECT_TRUE(its_configuration->is_offer_allowed(0x1443, 0x1234, 0x5679));
    EXPECT_TRUE(its_configuration->is_offer_allowed(0x1443, 0x1300, 0x1));
    EXPECT_TRUE(its_configuration->is_offer_allowed(0x1443, 0x1300, 0x2));

    EXPECT_TRUE(its_configuration->is_client_allowed(0x1343, 0x1234, 0x5678));
    EXPECT_TRUE(its_configuration->is_client_allowed(0x1346, 0x1234, 0x5678));
    EXPECT_FALSE(its_configuration->is_client_allowed(0x1347, 0x1234, 0x5678));
    EXPECT_FALSE(its_configuration->is_client_allowed(0x1342, 0x1234, 0x5678));
    EXPECT_FALSE(its_configuration->is_client_allowed(0x1343, 0x1234, 0x5679));
    EXPECT_FALSE(its_configuration->is_client_allowed(0x1343, 0x1230, 0x5678));
    // explicitly denied requests
    EXPECT_FALSE(its_configuration->is_client_allowed(0x1443, 0x1234, 0x5678));
    EXPECT_FALSE(its_configuration->is_client_allowed(0x1446, 0x1234, 0x5678));
    EXPECT_TRUE(its_configuration->is_client_allowed(0x1443, 0x1234, 0x5679));
    EXPECT_TRUE(its_configuration->is_client_allowed(0x1443, 0x1234, 0x5679));
    EXPECT_FALSE(its_configuration->is_client_allowed(0x1442, 0x1234, 0x5678));
    EXPECT_FALSE(its_configuration->is_client_allowed(0x1447, 0x1234, 0x5678));

    EXPECT_TRUE(its_configuration->check_credentials(0x1277, 1000, 1000));
    EXPECT_FALSE(its_configuration->check_credentials(0x1277, 1001, 1001));
    EXPECT_FALSE(its_configuration->check_credentials(0x1278, 1000, 1000));

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

    EXPECT_TRUE(check<bool>(enabled, _expected_enabled, "SD ENABLED"));
    EXPECT_TRUE(check<std::string>(protocol, _expected_protocol, "SD PROTOCOL"));
    EXPECT_TRUE(check<std::string>(multicast, _expected_multicast, "SD MULTICAST"));
    EXPECT_TRUE(check<uint16_t>(port, _expected_port, "SD PORT"));

    EXPECT_TRUE(check<int32_t>(initial_delay_min, _expected_initial_delay_min, "SD INITIAL DELAY MIN"));
    EXPECT_TRUE(check<int32_t>(initial_delay_max, _expected_initial_delay_max, "SD INITIAL DELAY MAX"));
    EXPECT_TRUE(check<int32_t>(repetitions_base_delay, _expected_repetitions_base_delay, "SD REPETITION BASE DELAY"));
    EXPECT_TRUE(check<uint8_t>(repetitions_max,_expected_repetitions_max, "SD REPETITION MAX"));
    EXPECT_TRUE(check<vsomeip::ttl_t>(ttl, _expected_ttl, "SD TTL"));
    EXPECT_TRUE(check<int32_t>(cyclic_offer_delay, _expected_cyclic_offer_delay, "SD CYCLIC OFFER DELAY"));
    EXPECT_TRUE(check<int32_t>(request_response_delay, _expected_request_response_delay, "SD RESPONSE REQUEST DELAY"));
    EXPECT_EQ(1000u, its_configuration->get_sd_offer_debounce_time());

    ASSERT_TRUE(vsomeip::plugin_manager::get()->unload_plugin(vsomeip::plugin_type_e::CONFIGURATION_PLUGIN));
}

TEST(configuration_test, check_config_file) {
    // Check current configuration file format
    check_file(CONFIGURATION_FILE,
               EXPECTED_UNICAST_ADDRESS,
               EXPECTED_HAS_CONSOLE,
               EXPECTED_HAS_FILE,
               EXPECTED_HAS_DLT,
               EXPECTED_VERSION_LOGGING_ENABLED,
               EXPECTED_VERSION_LOGGING_INTERVAL,
               EXPECTED_APPLICATION_MAX_DISPATCHERS,
               EXPECTED_APPLICATION_MAX_DISPATCH_TIME,
               EXPECTED_APPLICATION_THREADS,
               EXPECTED_APPLICATION_REQUEST_DEBOUNCE_TIME,
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
}

TEST(configuration_test, check_deprecated_config_file) {
    // Check deprecated configuration file format
    check_file(DEPRECATED_CONFIGURATION_FILE,
               EXPECTED_UNICAST_ADDRESS,
               EXPECTED_HAS_CONSOLE,
               EXPECTED_HAS_FILE,
               EXPECTED_HAS_DLT,
               EXPECTED_VERSION_LOGGING_ENABLED,
               EXPECTED_VERSION_LOGGING_INTERVAL,
               EXPECTED_APPLICATION_MAX_DISPATCHERS,
               EXPECTED_APPLICATION_MAX_DISPATCH_TIME,
               EXPECTED_APPLICATION_THREADS,
               EXPECTED_APPLICATION_REQUEST_DEBOUNCE_TIME,
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
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
