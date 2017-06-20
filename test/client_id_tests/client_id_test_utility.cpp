// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <unistd.h> // for access()
#include <sstream>

#include <vsomeip/constants.hpp>

#include "../../implementation/utility/include/utility.hpp"
#include "../../implementation/configuration/include/configuration.hpp"
#include "../../implementation/plugin/include/plugin_manager.hpp"

using namespace vsomeip;

static const std::string APPLICATION_NAME_ROUTING_MANAGER = "vsomeipd";

static const std::string APPLICATION_NAME_NOT_PREDEFINED = "test-application-name";

static const std::string APPLICATION_IN_NAME = "client_id_test_utility_service_in";
static const vsomeip::client_t APPLICATION_IN_CLIENT_ID = 0x6311;

static const std::string APPLICATION_IN_NAME_TWO = "client_id_test_utility_service_in_two";
static const vsomeip::client_t APPLICATION_IN_CLIENT_ID_TWO = 0x6312;

static const std::string APPLICATION_OUT_LOW_NAME = "client_id_test_utility_service_out_low";
static const vsomeip::client_t APPLICATION_OUT_LOW_CLIENT_ID = 0x6011;

static const std::string APPLICATION_OUT_HIGH_NAME = "client_id_test_utility_service_out_high";
static const vsomeip::client_t APPLICATION_OUT_HIGH_CLIENT_ID = 0x6411;

class client_id_utility_test: public ::testing::Test {
public:
    client_id_utility_test() :
            client_id_routing_manager_(0x0) {

        std::shared_ptr<vsomeip::configuration> its_configuration;
        auto its_plugin = vsomeip::plugin_manager::get()->get_plugin(
                vsomeip::plugin_type_e::CONFIGURATION_PLUGIN);
        if (its_plugin) {
            configuration_ = std::dynamic_pointer_cast<vsomeip::configuration>(its_plugin);
        }
    }
protected:
    virtual void SetUp() {
        ASSERT_FALSE(file_exist(std::string("/dev/shm").append(utility::get_shm_name(configuration_))));
        ASSERT_TRUE(static_cast<bool>(configuration_));
        configuration_->load(APPLICATION_NAME_ROUTING_MANAGER);

        utility::auto_configuration_init(configuration_);
        EXPECT_TRUE(file_exist(std::string("/dev/shm").append(utility::get_shm_name(configuration_))));

        client_id_routing_manager_ = utility::request_client_id(
                configuration_, APPLICATION_NAME_ROUTING_MANAGER, 0x0);
        EXPECT_EQ(0x6301, client_id_routing_manager_);
        EXPECT_TRUE(utility::is_routing_manager_host(client_id_routing_manager_));
    }

    virtual void TearDown() {
        utility::auto_configuration_exit(client_id_routing_manager_, configuration_);
        EXPECT_FALSE(file_exist(std::string("/dev/shm").append(utility::get_shm_name(configuration_))));
    }

    bool file_exist(const std::string &_path) {
        const int ret = ::access(_path.c_str(), F_OK);
        if (ret == -1 && errno == ENOENT) {
            return false;
        } else if (ret == -1) {
            std::stringstream its_stream;
            its_stream << "file_exists (" << _path << "): ";
            std::perror(its_stream.str().c_str());
            return false;
        } else {
            return true;
        }
    }

protected:
    std::shared_ptr<configuration> configuration_;
    vsomeip::client_t client_id_routing_manager_;
};

TEST_F(client_id_utility_test, request_release_client_id) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(0x6302, its_client_id);

    utility::release_client_id(its_client_id);
}

TEST_F(client_id_utility_test, request_client_id_twice) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(0x6302, its_client_id);

    client_t its_client_id_2 = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(0x6303, its_client_id_2);

    utility::release_client_id(its_client_id);
    utility::release_client_id(its_client_id_2);
}

TEST_F(client_id_utility_test, release_unknown_client_id) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(0x6302, its_client_id);

    utility::release_client_id(0x4711);
    utility::release_client_id(its_client_id);

    client_t its_client_id_2 = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(0x6303, its_client_id_2);
    utility::release_client_id(its_client_id_2);
}

TEST_F(client_id_utility_test, release_client_id_twice)
{
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(0x6302, its_client_id);

    utility::release_client_id(its_client_id);
    utility::release_client_id(its_client_id);

    client_t its_client_id_2 = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(0x6303, its_client_id_2);
    utility::release_client_id(its_client_id_2);
}

TEST_F(client_id_utility_test, ensure_preconfigured_client_ids_not_used_for_autoconfig)
{
    const std::uint8_t limit = APPLICATION_IN_CLIENT_ID & 0xFF;
    std::vector<client_t> its_client_ids;
    for (int i = 0; i < limit + 10; i++ ) {
        client_t its_client_id = utility::request_client_id(configuration_,
                APPLICATION_NAME_NOT_PREDEFINED, 0x0);
        EXPECT_NE(ILLEGAL_CLIENT, its_client_id);
        if (its_client_id != ILLEGAL_CLIENT) {
            its_client_ids.push_back(its_client_id);
            EXPECT_NE(APPLICATION_IN_CLIENT_ID, its_client_id);
        } else {
            ADD_FAILURE() << "Received ILLEGAL_CLIENT "
                    << static_cast<std::uint32_t>(i);
        }
    }

    // release all
    for (const client_t c : its_client_ids) {
        utility::release_client_id(c);
    }
}

TEST_F(client_id_utility_test,
       ensure_preconfigured_client_ids_in_diagnosis_range_dont_influence_autoconfig_client_ids)
{
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(0x6302, its_client_id);

    client_t its_client_id2 = utility::request_client_id(configuration_,
            APPLICATION_IN_NAME, APPLICATION_IN_CLIENT_ID);
    EXPECT_EQ(APPLICATION_IN_CLIENT_ID, its_client_id2);

    client_t its_client_id3 = utility::request_client_id(configuration_,
            APPLICATION_IN_NAME_TWO, APPLICATION_IN_CLIENT_ID_TWO);
    EXPECT_EQ(APPLICATION_IN_CLIENT_ID_TWO, its_client_id3);


    client_t its_client_id4 = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(0x6303, its_client_id4);

    client_t its_client_id5 = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(0x6304, its_client_id5);

    utility::release_client_id(its_client_id);
    utility::release_client_id(its_client_id2);
    utility::release_client_id(its_client_id3);
    utility::release_client_id(its_client_id4);
    utility::release_client_id(its_client_id5);
}

TEST_F(client_id_utility_test,
        request_predefined_client_id_in_diagnosis_range) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_IN_NAME, APPLICATION_IN_CLIENT_ID);
    EXPECT_EQ(APPLICATION_IN_CLIENT_ID, its_client_id);

    utility::release_client_id(its_client_id);
}

TEST_F(client_id_utility_test,
        request_predefined_client_id_in_diagnosis_range_twice) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_IN_NAME, APPLICATION_IN_CLIENT_ID);
    EXPECT_EQ(APPLICATION_IN_CLIENT_ID, its_client_id);

    client_t its_client_id_2 = utility::request_client_id(configuration_,
            APPLICATION_IN_NAME, APPLICATION_IN_CLIENT_ID);
    EXPECT_EQ(0x6302, its_client_id_2);

    utility::release_client_id(its_client_id);
    utility::release_client_id(its_client_id_2);
}

TEST_F(client_id_utility_test,
        request_different_client_id_with_predefined_app_name_in_diagnosis_range) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_IN_NAME, APPLICATION_IN_CLIENT_ID + 1u);
    // has to get predefined client id although other was requested
    EXPECT_EQ(APPLICATION_IN_CLIENT_ID, its_client_id);

    // predefined in json is now already used and requested should be assigned
    client_t its_client_id_2 = utility::request_client_id(configuration_,
            APPLICATION_IN_NAME, APPLICATION_IN_CLIENT_ID + 1u);
    EXPECT_EQ(APPLICATION_IN_CLIENT_ID + 1u, its_client_id_2);

    client_t its_client_id_3 = utility::request_client_id(configuration_,
            APPLICATION_IN_NAME, APPLICATION_IN_CLIENT_ID);
    EXPECT_EQ(0x6302, its_client_id_3);

    utility::release_client_id(its_client_id);
    utility::release_client_id(its_client_id_2);
    utility::release_client_id(its_client_id_3);
}

TEST_F(client_id_utility_test,
        request_predefined_client_id_outside_diagnosis_range_low) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_OUT_LOW_NAME, APPLICATION_OUT_LOW_CLIENT_ID);
    EXPECT_EQ(APPLICATION_OUT_LOW_CLIENT_ID, its_client_id);

    utility::release_client_id(its_client_id);
}

TEST_F(client_id_utility_test,
        request_predefined_client_id_outside_diagnosis_range_low_twice) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_OUT_LOW_NAME, APPLICATION_OUT_LOW_CLIENT_ID);
    EXPECT_EQ(APPLICATION_OUT_LOW_CLIENT_ID, its_client_id);

    client_t its_client_id_2 = utility::request_client_id(configuration_,
            APPLICATION_OUT_LOW_NAME, APPLICATION_OUT_LOW_CLIENT_ID);
    EXPECT_EQ(0x6302, its_client_id_2);

    utility::release_client_id(its_client_id);
    utility::release_client_id(its_client_id_2);
}

TEST_F(client_id_utility_test,
        request_different_client_id_with_predefined_app_name_outside_diagnosis_range_low) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_OUT_LOW_NAME, APPLICATION_OUT_LOW_CLIENT_ID + 1u);
    // has to get predefined client id although other was requested
    EXPECT_EQ(APPLICATION_OUT_LOW_CLIENT_ID, its_client_id);

    // predefined in json is now already used and requested should be assigned
    client_t its_client_id_2 = utility::request_client_id(configuration_,
            APPLICATION_OUT_LOW_NAME, APPLICATION_OUT_LOW_CLIENT_ID + 1u);
    EXPECT_EQ(APPLICATION_OUT_LOW_CLIENT_ID + 1u, its_client_id_2);

    client_t its_client_id_3 = utility::request_client_id(configuration_,
            APPLICATION_OUT_LOW_NAME, APPLICATION_OUT_LOW_CLIENT_ID);
    EXPECT_EQ(0x6302, its_client_id_3);

    utility::release_client_id(its_client_id);
    utility::release_client_id(its_client_id_2);
    utility::release_client_id(its_client_id_3);
}

TEST_F(client_id_utility_test,
        request_predefined_client_id_outside_diagnosis_range_high) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_OUT_HIGH_NAME, APPLICATION_OUT_HIGH_CLIENT_ID);
    EXPECT_EQ(APPLICATION_OUT_HIGH_CLIENT_ID, its_client_id);

    utility::release_client_id(its_client_id);
}

TEST_F(client_id_utility_test,
        request_predefined_client_id_outside_diagnosis_range_high_twice) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_OUT_HIGH_NAME, APPLICATION_OUT_HIGH_CLIENT_ID);
    EXPECT_EQ(APPLICATION_OUT_HIGH_CLIENT_ID, its_client_id);

    client_t its_client_id_2 = utility::request_client_id(configuration_,
            APPLICATION_OUT_HIGH_NAME, APPLICATION_OUT_HIGH_CLIENT_ID);
    EXPECT_EQ(0x6302, its_client_id_2);

    utility::release_client_id(its_client_id);
    utility::release_client_id(its_client_id_2);
}

TEST_F(client_id_utility_test,
        request_different_client_id_with_predefined_app_name_outside_diagnosis_range_high) {
    client_t its_client_id = utility::request_client_id(configuration_,
            APPLICATION_OUT_HIGH_NAME, APPLICATION_OUT_HIGH_CLIENT_ID + 1u);
    // has to get predefined client id although other was requested
    EXPECT_EQ(APPLICATION_OUT_HIGH_CLIENT_ID, its_client_id);

    // predefined in json is now already used and requested should be assigned
    client_t its_client_id_2 = utility::request_client_id(configuration_,
            APPLICATION_OUT_HIGH_NAME, APPLICATION_OUT_HIGH_CLIENT_ID + 1u);
    EXPECT_EQ(APPLICATION_OUT_HIGH_CLIENT_ID + 1u, its_client_id_2);

    client_t its_client_id_3 = utility::request_client_id(configuration_,
            APPLICATION_OUT_HIGH_NAME, APPLICATION_OUT_HIGH_CLIENT_ID);
    EXPECT_EQ(0x6302, its_client_id_3);

    utility::release_client_id(its_client_id);
    utility::release_client_id(its_client_id_2);
    utility::release_client_id(its_client_id_3);
}


TEST_F(client_id_utility_test, exhaust_client_id_range_sequential) {
     std::vector<client_t> its_client_ids;

     // -1 for the routing manager, -2 as two predefined client IDs are present
     // in the json file which aren't assigned via autoconfiguration
     std::uint8_t max_allowed_clients = 0xFF - 3;
     // acquire maximum amount of client IDs
     for (std::uint8_t i = 0; i < max_allowed_clients; i++) {
         client_t its_client_id = utility::request_client_id(configuration_,
                 APPLICATION_NAME_NOT_PREDEFINED, 0x0);
         EXPECT_NE(ILLEGAL_CLIENT, its_client_id);
         if (its_client_id != ILLEGAL_CLIENT) {
             its_client_ids.push_back(its_client_id);
         } else {
             ADD_FAILURE() << "Received ILLEGAL_CLIENT "
                     << static_cast<std::uint32_t>(i);
         }
     }

     // check limit is reached
     client_t its_illegal_client_id = utility::request_client_id(configuration_,
             APPLICATION_NAME_NOT_PREDEFINED, 0x0);
     EXPECT_EQ(ILLEGAL_CLIENT, its_illegal_client_id);

    // release all
    for (const client_t c : its_client_ids) {
        utility::release_client_id(c);
     }
    its_client_ids.clear();
    its_illegal_client_id = 0xFFFF;

    // One more time!

    // acquire maximum amount of client IDs
    for (std::uint8_t i = 0; i < max_allowed_clients; i++) {
         client_t its_client_id = utility::request_client_id(configuration_,
                 APPLICATION_NAME_NOT_PREDEFINED, 0x0);
         EXPECT_NE(ILLEGAL_CLIENT, its_client_id);
         if (its_client_id != ILLEGAL_CLIENT) {
             its_client_ids.push_back(its_client_id);
         } else {
             ADD_FAILURE() << "Received ILLEGAL_CLIENT "
                     << static_cast<std::uint32_t>(i);
         }
     }

     // check limit is reached
     its_illegal_client_id = utility::request_client_id(configuration_,
             APPLICATION_NAME_NOT_PREDEFINED, 0x0);
     EXPECT_EQ(ILLEGAL_CLIENT, its_illegal_client_id);

    // release all
    for (const client_t c : its_client_ids) {
        utility::release_client_id(c);
    }
 }

TEST_F(client_id_utility_test, exhaust_client_id_range_fragmented) {
    std::vector<client_t> its_client_ids;

    // -1 for the routing manager, -2 as two predefined client IDs are present
    // in the json file which aren't assigned via autoconfiguration
    std::uint8_t max_allowed_clients = 0xFF - 3;
    // acquire maximum amount of client IDs
    for (std::uint8_t i = 0; i < max_allowed_clients; i++) {
        client_t its_client_id = utility::request_client_id(configuration_,
                APPLICATION_NAME_NOT_PREDEFINED, 0x0);
        EXPECT_NE(ILLEGAL_CLIENT, its_client_id);
        if (its_client_id != ILLEGAL_CLIENT) {
            its_client_ids.push_back(its_client_id);
        } else {
            ADD_FAILURE() << "Received ILLEGAL_CLIENT "
                    << static_cast<std::uint32_t>(i);
        }
    }

    // check limit is reached
    client_t its_illegal_client_id = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(ILLEGAL_CLIENT, its_illegal_client_id);

    // release every second requested client ID
    std::vector<client_t> its_released_client_ids;
    for (size_t i = 0; i < its_client_ids.size(); i++ ) {
        if (i % 2) {
            its_released_client_ids.push_back(its_client_ids[i]);
            utility::release_client_id(its_client_ids[i]);
        }
    }
    for (const client_t c : its_released_client_ids) {
        for (auto it = its_client_ids.begin(); it != its_client_ids.end(); ) {
            if (*it == c) {
                it = its_client_ids.erase(it);
            } else {
                ++it;
            }
        }
    }

    // acquire client IDs up to the maximum allowed amount again
    for (std::uint8_t i = 0; i < its_released_client_ids.size(); i++) {
        client_t its_client_id = utility::request_client_id(configuration_,
                APPLICATION_NAME_NOT_PREDEFINED, 0x0);
        EXPECT_NE(ILLEGAL_CLIENT, its_client_id);
        if (its_client_id != ILLEGAL_CLIENT) {
            its_client_ids.push_back(its_client_id);
        } else {
            ADD_FAILURE() << "Received ILLEGAL_CLIENT "
                    << static_cast<std::uint32_t>(i);
        }
    }

    // check limit is reached
    its_illegal_client_id = 0xFFFF;
    its_illegal_client_id = utility::request_client_id(configuration_,
            APPLICATION_NAME_NOT_PREDEFINED, 0x0);
    EXPECT_EQ(ILLEGAL_CLIENT, its_illegal_client_id);

    // release all
    for (const client_t c : its_client_ids) {
        utility::release_client_id(c);
    }
}

#ifndef _WIN32
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
