// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <algorithm>
#include <cstring>

#include <gtest/gtest.h>

#ifndef _WIN32
#include <signal.h>
#endif

#include <vsomeip/vsomeip.hpp>
#include "../../implementation/utility/include/utility.hpp"
#include "../../implementation/logging/include/logger.hpp"
#include "../../implementation/configuration/include/policy.hpp"

#include "../security_config_plugin_tests/security_config_plugin_test_globals.hpp"
class security_config_plugin_test_client;
static security_config_plugin_test_client* the_client;
extern "C" void signal_handler(int _signum);

#define GET_LONG_BYTE0(x) ((x) & 0xFF)
#define GET_LONG_BYTE1(x) (((x) >> 8) & 0xFF)
#define GET_LONG_BYTE2(x) (((x) >> 16) & 0xFF)
#define GET_LONG_BYTE3(x) (((x) >> 24) & 0xFF)

class security_config_plugin_test_client {
public:
    security_config_plugin_test_client(bool _update_only, bool _remove_only, bool _subscribe_only) :
            update_only_(_update_only),
            remove_only_(_remove_only),
            subscribe_only_(_subscribe_only),
            app_(vsomeip::runtime::get()->create_application()),
            wait_until_registered_(true),
            wait_until_security_config_service_available_(true),
            wait_until_control_service_available_(true),
            wait_until_updated_service_101_available_(true),
            wait_until_updated_service_102_available_(true),
            wait_until_method_1_responses_received_(true),
            wait_until_method_2_responses_received_(true),
            number_of_received_responses_method_1(0),
            number_of_received_responses_method_2(0),
            number_of_received_events_1(0),
            number_of_received_events_2(0),
            number_of_received_events_4(0),
            wait_until_notifications_1_received_(true),
            wait_until_notifications_2_received_(true),
            wait_for_stop_(true),
            update_ok_(false),
            removal_ok_(false),
            stop_thread_(std::bind(&security_config_plugin_test_client::wait_for_stop, this)),
            run_thread_(std::bind(&security_config_plugin_test_client::run, this)) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }

        // register signal handler
        the_client = this;
        struct sigaction sa_new, sa_old;
        sa_new.sa_handler = signal_handler;
        sa_new.sa_flags = 0;
        sigemptyset(&sa_new.sa_mask);
        ::sigaction(SIGUSR1, &sa_new, &sa_old);
        ::sigaction(SIGINT, &sa_new, &sa_old);
        ::sigaction(SIGTERM, &sa_new, &sa_old);
        ::sigaction(SIGABRT, &sa_new, &sa_old);

        app_->register_state_handler(
                std::bind(&security_config_plugin_test_client::on_state, this,
                        std::placeholders::_1));

        // handle plugin messages
        app_->register_message_handler(security_config_plugin_test::security_config_plugin_serviceinfo.service_id,
                security_config_plugin_test::security_config_plugin_serviceinfo.instance_id,
                vsomeip::ANY_METHOD,
                std::bind(&security_config_plugin_test_client::on_plugin_message, this,
                        std::placeholders::_1));

        // handle service 0x0101 messages
        app_->register_message_handler(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                vsomeip::ANY_METHOD,
                std::bind(&security_config_plugin_test_client::on_service_1_message, this,
                        std::placeholders::_1));

        // handle service 0x0102 messages
        app_->register_message_handler(security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
                vsomeip::ANY_METHOD,
                std::bind(&security_config_plugin_test_client::on_service_2_message, this,
                        std::placeholders::_1));

        // request acl plugin interface service
        app_->register_availability_handler(
                security_config_plugin_test::security_config_plugin_serviceinfo.service_id,
                security_config_plugin_test::security_config_plugin_serviceinfo.instance_id,
                std::bind(&security_config_plugin_test_client::on_availability, this,
                        std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3),
                security_config_plugin_test::security_config_plugin_major_version_);
        app_->request_service(
                security_config_plugin_test::security_config_plugin_serviceinfo.service_id,
                security_config_plugin_test::security_config_plugin_serviceinfo.instance_id,
                security_config_plugin_test::security_config_plugin_major_version_, vsomeip::ANY_MINOR);

        // request control service which is globally allowed
        app_->register_availability_handler(
                security_config_plugin_test::security_config_test_serviceinfo_3.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_3.instance_id,
                std::bind(&security_config_plugin_test_client::on_availability, this,
                        std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3));
        app_->request_service(
                security_config_plugin_test::security_config_test_serviceinfo_3.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_3.instance_id,
                vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR);

        // request service 0x0101 which is globally blacklisted and shall get available after aclUpdate
        app_->register_availability_handler(
                security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                std::bind(&security_config_plugin_test_client::on_availability, this,
                        std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3));
        app_->request_service(
                security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR);


        // request service 0x0102 which is globally blacklisted and shall get available after aclUpdate
        app_->register_availability_handler(
                security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
                std::bind(&security_config_plugin_test_client::on_availability, this,
                        std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3));
        app_->request_service(
                      security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
                      security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
                      vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR);

        // offer service 0x0100 which is not allowed to be offered at any time
        // (log message "Skip offer!" should be printed by daemon)
        app_->offer_service(0x666, security_config_plugin_test::security_config_test_serviceinfo_1.instance_id);

        app_->start();
    }

    ~security_config_plugin_test_client() {
        run_thread_.join();
        stop_thread_.join();

    }

    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
        << (_state == vsomeip::state_type_e::ST_REGISTERED ?
                "registered." : "deregistered.");

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            std::lock_guard<std::mutex> its_lock(mutex_);
            wait_until_registered_ = false;
            condition_.notify_one();
        }
    }

    void on_availability(vsomeip::service_t _service,
                         vsomeip::instance_t _instance, bool _is_available) {
        if(_is_available) {
            // check plugin interface availability
            if (_service == security_config_plugin_test::security_config_plugin_serviceinfo.service_id &&
                    _instance == security_config_plugin_test::security_config_plugin_serviceinfo.instance_id) {
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_until_security_config_service_available_ = false;
                condition_.notify_one();
            }

            // check control service availability
            if (_service == security_config_plugin_test::security_config_test_serviceinfo_3.service_id &&
                    _instance == security_config_plugin_test::security_config_test_serviceinfo_3.instance_id) {
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_until_control_service_available_ = false;
                condition_.notify_one();
            }

            // check updated policy makes service 0x0101 available
            if (_service == security_config_plugin_test::security_config_test_serviceinfo_1.service_id &&
                    _instance == security_config_plugin_test::security_config_test_serviceinfo_1.instance_id) {
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_until_updated_service_101_available_ = false;
                condition_.notify_one();
            }

            // check updated policy makes service 0x0102 available
            if (_service == security_config_plugin_test::security_config_test_serviceinfo_2.service_id &&
                    _instance == security_config_plugin_test::security_config_test_serviceinfo_2.instance_id) {
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_until_updated_service_102_available_ = false;
                condition_.notify_one();
            }
        } else {

        }
    }

    void on_service_1_message(const std::shared_ptr<vsomeip::message> &_message) {
        if(_message->get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {

            number_of_received_events_1++;

            // event which should never be received
            if (_message->get_method() == 0x8004)
                number_of_received_events_4++;

            other_services_received_notification_[std::make_pair(_message->get_service(),
                                                             _message->get_method())]++;
            VSOMEIP_INFO
            << "on_service_1_message Received a notification with Client/Session [" << std::setw(4)
            << std::setfill('0') << std::hex << _message->get_client() << "/"
            << std::setw(4) << std::setfill('0') << std::hex
            << _message->get_session() << "] from Service/Method ["
            << std::setw(4) << std::setfill('0') << std::hex
            << _message->get_service() << "/" << std::setw(4) << std::setfill('0')
            << std::hex << _message->get_method() <<"] (now have: "
            << std::dec << other_services_received_notification_[std::make_pair(_message->get_service(),
                                                                    _message->get_method())] << ")";

            std::shared_ptr<vsomeip::payload> its_payload(_message->get_payload());
            EXPECT_EQ(2u, its_payload->get_length());
            EXPECT_EQ((_message->get_service() & 0xFF00 ) >> 8, its_payload->get_data()[0]);
            EXPECT_EQ((_message->get_service() & 0xFF), its_payload->get_data()[1]);
            bool notify = all_notifications_received();

            if(notify) {
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_until_notifications_1_received_ = false;
                condition_.notify_one();
            }
        } else if (_message->get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
            if(_message->get_method() == security_config_plugin_test::security_config_test_serviceinfo_1.method_id) {
                std::lock_guard<std::mutex> its_lock(mutex_);
                number_of_received_responses_method_1++;
                wait_until_method_1_responses_received_ = false;
                condition_.notify_one();
            }
        }
    }

    void on_service_2_message(const std::shared_ptr<vsomeip::message> &_message) {
        if(_message->get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {

            number_of_received_events_2++;
            other_services_received_notification_[std::make_pair(_message->get_service(),
                                                             _message->get_method())]++;
            VSOMEIP_INFO
            << "on_service_2_message Received a notification with Client/Session [" << std::setw(4)
            << std::setfill('0') << std::hex << _message->get_client() << "/"
            << std::setw(4) << std::setfill('0') << std::hex
            << _message->get_session() << "] from Service/Method ["
            << std::setw(4) << std::setfill('0') << std::hex
            << _message->get_service() << "/" << std::setw(4) << std::setfill('0')
            << std::hex << _message->get_method() <<"] (now have: "
            << std::dec << other_services_received_notification_[std::make_pair(_message->get_service(),
                                                                    _message->get_method())] << ")";

            std::shared_ptr<vsomeip::payload> its_payload(_message->get_payload());
            EXPECT_EQ(2u, its_payload->get_length());
            EXPECT_EQ((_message->get_service() & 0xFF00 ) >> 8, its_payload->get_data()[0]);
            EXPECT_EQ((_message->get_service() & 0xFF), its_payload->get_data()[1]);
            bool notify = all_notifications_received();

            if(notify) {
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_until_notifications_2_received_ = false;
                condition_.notify_one();
            }
        } else if (_message->get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
            if(_message->get_method() == security_config_plugin_test::security_config_test_serviceinfo_2.method_id) {
                std::lock_guard<std::mutex> its_lock(mutex_);
                number_of_received_responses_method_2++;
                wait_until_method_2_responses_received_ = false;
                condition_.notify_one();
            }
        }
    }

    void on_plugin_message(const std::shared_ptr<vsomeip::message> &_message) {
        if (_message->get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
            VSOMEIP_INFO
            << "Received a response with Client/Session [" << std::setw(4)
            << std::setfill('0') << std::hex << _message->get_client() << "/"
            << std::setw(4) << std::setfill('0') << std::hex
            << _message->get_session() << "] from Service/Method ["
            << std::setw(4) << std::setfill('0') << std::hex
            << _message->get_service() << "/" << std::setw(4) << std::setfill('0')
            << std::hex << _message->get_method();

            std::shared_ptr<vsomeip::payload> pl = _message->get_payload();
            EXPECT_EQ( vsomeip::return_code_e::E_OK, _message->get_return_code());

            uint32_t length = pl->get_length();
            EXPECT_EQ(length, (uint32_t) 0x01);

            // expect status_success_ returned by plugin interface
            bool success(false);
            uint8_t* dataptr = pl->get_data();
            for(uint32_t i = 0; i< pl->get_length(); i++) {
                std::cout << "payload data: " << std::hex << (uint32_t) dataptr[i] << std::endl;
                EXPECT_EQ(dataptr[i], 0);
                if (dataptr[i] == 0x00) {
                    success = true;
                }
            }
            if (success) {
                // updateAcl answer
                if (_message->get_service() == security_config_plugin_test::security_config_plugin_serviceinfo.service_id
                        &&_message->get_method() == security_config_plugin_test::security_config_plugin_serviceinfo.method_id) {
                    update_ok_ = true;
                }

                // removeAcl answer
                if (_message->get_service() == security_config_plugin_test::security_config_plugin_serviceinfo.service_id
                        &&_message->get_method() == security_config_plugin_test::security_config_plugin_serviceinfo_reset.method_id) {
                    removal_ok_ = true;
                }
            }
        }
    }

    bool all_notifications_received() {
        return std::all_of(
                other_services_received_notification_.cbegin(),
                other_services_received_notification_.cend(),
                [&](const std::map<std::pair<vsomeip::service_t,
                        vsomeip::method_t>, std::uint32_t>::value_type& v)
                {
                    if (v.second == security_config_plugin_test::notifications_to_send) {
                        return true;
                    } else {
                        if (v.second >= security_config_plugin_test::notifications_to_send) {
                            VSOMEIP_WARNING
                                    << " Received multiple initial events from service/instance: "
                                    << std::setw(4) << std::setfill('0') << std::hex << v.first.first
                                    << "."
                                    << std::setw(4) << std::setfill('0') << std::hex << v.first.second
                                    << " number of received events: " << v.second
                                    << ". This is caused by StopSubscribe/Subscribe messages and/or"
                                    << " service offered via UDP and TCP";
                            return false;
                        } else {
                            return false;
                        }
                    }
                }
        );
    }

    void handle_signal(int _signum) {
        VSOMEIP_DEBUG << "Catched signal, going down ("
                << std::dec <<_signum << ")";
        std::lock_guard<std::mutex> its_lock(stop_mutex_);
        wait_for_stop_ = false;
        stop_condition_.notify_one();
    }

    void wait_for_stop() {
        {
            std::unique_lock<std::mutex> its_lock(stop_mutex_);
            while (wait_for_stop_) {
                stop_condition_.wait(its_lock);
            }

            EXPECT_TRUE(update_ok_);

            // Todo if policy was removed, the plugin answer is not allowed to be sent back as plugin service is blacklisted again
            //EXPECT_TRUE(removal_ok_);

            VSOMEIP_INFO << "Received all responses from plugin, going down";
        }
        app_->clear_all_handler();
        app_->stop();
    }

    void call_acl_deployment_interface(bool _updateAcl) {

        uint32_t user_id = (uint32_t) getuid();
        uint32_t group_id = (uint32_t) getgid();

        if(update_only_ || remove_only_) {
            user_id = 1001;
            group_id = 1001;
        }

        uint8_t uid_byte_0 = (uint8_t) GET_LONG_BYTE3(user_id);
        uint8_t uid_byte_1 = GET_LONG_BYTE2(user_id);
        uint8_t uid_byte_2 = GET_LONG_BYTE1(user_id);
        uint8_t uid_byte_3 = GET_LONG_BYTE0(user_id);

        uint8_t gid_byte_0 = (uint8_t) GET_LONG_BYTE3(group_id);
        uint8_t gid_byte_1 = GET_LONG_BYTE2(group_id);
        uint8_t gid_byte_2 = GET_LONG_BYTE1(group_id);
        uint8_t gid_byte_3 = GET_LONG_BYTE0(group_id);

        // send policy payload which also contains part of global security json to allow answer for removeAcl calls
        // updateAcl payload:
        /*
            Security configuration: UID: 0x3e8
            Security configuration: GID: 0x3e8
            Security configuration: RQUESTS POLICY SIZE: 4
            ALLOWED REQUESTS Service: 0x101
                Instances:
                     first: 0x63 last: 0x63
                Methods:
                     first: 0x1 last: 0x5
                     first: 0x8001 last: 0x8003
            ALLOWED REQUESTS Service: 0x102
                Instances:
                     first: 0x63 last: 0x63
                Methods:
                     first: 0x1 last: 0x5
                     first: 0x8001 last: 0x8003
            ALLOWED REQUESTS Service: 0x103
                Instances:
                     first: 0x63 last: 0x63
                Methods:
                     first: 0x3 last: 0x3
                     first: 0x7777 last: 0x7777
            ALLOWED REQUESTS Service: 0xF90F
                Instances:
                     first: 0x1 last: 0x1
                Methods:
                     first: 0x1 last: 0x1
                     first: 0x2 last: 0x2
            Security configuration: OFFER POLICY SIZE: 4
            ALLOWED OFFERS Service: 0x101
                Instances:
                     first: 0x63 last: 0x63
            ALLOWED OFFERS Service: 0x102
                Instances:
                     first: 0x63 last: 0x63
            ALLOWED OFFERS Service: 0x103
                Instances:
                     first: 0x63 last: 0x63
            ALLOWED OFFERS Service: 0xF90F
                Instances:
                     first: 0x1 last: 0x1
         */
        std::array<unsigned char, 280> update_payload =
                {{  uid_byte_0, uid_byte_1, uid_byte_2, uid_byte_3, gid_byte_0, gid_byte_1, gid_byte_2, gid_byte_3,
                    0x00, 0x00, 0x00, 0xb8, 0xF9, 0x0F, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
                    0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00,
                    0x00, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
                    0x00, 0x01, 0x77, 0x77, 0x01, 0x01, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x04,
                    0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x80, 0x01, 0x80, 0x03, 0x01, 0x02, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00,
                    0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02,
                    0x80, 0x01, 0x80, 0x03, 0x00, 0x00, 0x00, 0x40, 0xF9, 0x0F, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x03, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00,
                    0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x01, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x01, 0x02, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00,
                    0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63}};

        // removeAcl payload for UID 12345 GID 4567 or UID: 1000 -> 0x03, 0xe8,
        std::array<unsigned char, 8> remove_payload = {{uid_byte_0, uid_byte_1, uid_byte_2, uid_byte_3, gid_byte_0, gid_byte_1, gid_byte_2, gid_byte_3}};

        std::shared_ptr<vsomeip::message> its_request = vsomeip::runtime::get()->create_request();
        its_request->set_service(security_config_plugin_test::security_config_plugin_serviceinfo.service_id);
        its_request->set_instance(security_config_plugin_test::security_config_plugin_serviceinfo.instance_id);
        std::vector<vsomeip::byte_t> its_payload;

        if (_updateAcl) {
            its_request->set_method(security_config_plugin_test::security_config_plugin_serviceinfo.method_id);
            for (uint32_t i=0; i < update_payload.size(); i++) {
                its_payload.push_back(update_payload[i]);
            }
        } else {
            its_request->set_method(security_config_plugin_test::security_config_plugin_serviceinfo_reset.method_id);
            for (uint32_t i=0; i < remove_payload.size(); i++) {
                its_payload.push_back(remove_payload[i]);
            }
        }
        its_request->set_payload(vsomeip::runtime::get()->create_payload(its_payload));
        app_->send(its_request);
    }

    void call_try_offer() {
        std::shared_ptr<vsomeip::message> its_request = vsomeip::runtime::get()->create_request();
        its_request->set_service(security_config_plugin_test::security_config_test_serviceinfo_3.service_id);
        its_request->set_instance(security_config_plugin_test::security_config_test_serviceinfo_3.instance_id);
        its_request->set_method(security_config_plugin_test::security_config_test_serviceinfo_3.method_id);
        app_->send(its_request);
    }

    void call_method(vsomeip::service_t _service, vsomeip::instance_t _instance, vsomeip::method_t _method) {
        std::shared_ptr<vsomeip::message> its_request = vsomeip::runtime::get()->create_request();
        its_request->set_service(_service);
        its_request->set_instance(_instance);
        its_request->set_method(_method);
        app_->send(its_request);
    }

    void run() {
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_registered_) {
                condition_.wait(its_lock);
            }
        }

        // payload for testing utility::parse_policy()
        /*
            Security configuration: UID: 0x01
            Security configuration: GID: 0x01
            Security configuration: RQUESTS POLICY SIZE: 4
            ALLOWED REQUESTS Service: 0x101
                Instances:
                     first: 0x63 last: 0x63
                Methods:
                     first: 0x1 last: 0x5
                     first: 0x8001 last: 0x8003
            ALLOWED REQUESTS Service: 0x102
                Instances:
                     first: 0x63 last: 0x63
                Methods:
                     first: 0x1 last: 0x5
                     first: 0x8001 last: 0x8003
            ALLOWED REQUESTS Service: 0x103
                Instances:
                     first: 0x63 last: 0x63
                Methods:
                     first: 0x3 last: 0x3
                     first: 0x7777 last: 0x7777
            ALLOWED REQUESTS Service: 0xF90F
                Instances:
                     first: 0x1 last: 0x1
                Methods:
                     first: 0x1 last: 0x1
                     first: 0x2 last: 0x2
            Security configuration: OFFER POLICY SIZE: 4
            ALLOWED OFFERS Service: 0x101
                Instances:
                     first: 0x63 last: 0x63
            ALLOWED OFFERS Service: 0x102
                Instances:
                     first: 0x63 last: 0x63
            ALLOWED OFFERS Service: 0x103
                Instances:
                     first: 0x63 last: 0x63
            ALLOWED OFFERS Service: 0xF90F
                Instances:
                     first: 0x1 last: 0x1
         */
        std::array<unsigned char, 280> policy_update_payload =
                {{  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, // uid / gid
                    0x00, 0x00, 0x00, 0xb8,
                    0xF9, 0x0F, // 0xf90f -> service ID at array index 12 - 13
                    0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x0a,
                    0x00, 0x00, 0x00, 0x02, // -> union length = 2 byte
                    0x00, 0x00, 0x00, 0x01, // -> union type is single instance ID
                    0x00, 0x01, // -> single instance id 0x1 at array index 30 - 31
                    0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00,
                    0x00, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
                    0x00, 0x01, 0x77, 0x77, 0x01, 0x01, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x00, 0x00, 0x00, 0x18,
                    0x00, 0x00, 0x00, 0x04,
                    0x00, 0x00, 0x00, 0x02, // -> union type is ID range
                    0x00, 0x01, // first 0x01 at array index 132 - 133
                    0x00, 0x05, // last  0x05 at array index 134 - 135
                    0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x80, 0x01, 0x80, 0x03, 0x01, 0x02, 0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00,
                    0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02,
                    0x80, 0x01, 0x80, 0x03, 0x00, 0x00, 0x00, 0x40, 0xF9, 0x0F, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x03, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00,
                    0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x01, 0x01, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x01, 0x02, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00,
                    0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63}};

        // Check if parse_policy() correctly handles invalid service, instance and method ID's / ranges.
        uint32_t its_uid = 0x00;
        uint32_t its_gid = 0x00;
        uint32_t its_remaining_bytes_ = policy_update_payload.size();
        std::shared_ptr<vsomeip::policy> its_policy(std::make_shared<vsomeip::policy>());
        const vsomeip::byte_t* its_buffer_ptr = policy_update_payload.data();

        // valid policy
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), true);

        // set request service ID to invalid value 0x00
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_invalid_service = policy_update_payload;
        // set service ID from 0xf90f to 0x00
        policy_invalid_service[12] = 0x00;
        policy_invalid_service[13] = 0x00;
        its_buffer_ptr = policy_invalid_service.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), false);

        // set request service ID to invalid value 0xFFFF
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_invalid_service_2 = policy_update_payload;
        // set service ID from 0xf90f to 0x00
        policy_invalid_service_2[12] = 0xFF;
        policy_invalid_service_2[13] = 0xFF;
        its_buffer_ptr = policy_invalid_service_2.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), false);

        // set request single instance ID to invalid value 0x00 which shall be ignored
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_invalid_instance = policy_update_payload;
        // set instance ID of service 0xf90f to 0x00
        policy_invalid_instance[30] = 0x00;
        policy_invalid_instance[31] = 0x00;
        its_buffer_ptr = policy_invalid_instance.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), true);

        // set request single instance ID to valid value 0xFFFF
        // meaning ANY_INSTANCE range from 0x01 to 0xFFFE
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_valid_instance = policy_update_payload;
        // set instance ID of service 0xf90f to 0xFFFF
        policy_valid_instance[30] = 0xFF;
        policy_valid_instance[31] = 0xFF;
        its_buffer_ptr = policy_valid_instance.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), true);

        // set first of range to 0x00 and last to 0x05 which is invalid
        // the range shall be adjusted to be 0x01 to 0x05 without giving an error
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_invalid_method = policy_update_payload;
        policy_invalid_method[132] = 0x00;
        policy_invalid_method[133] = 0x00;
        its_buffer_ptr = policy_invalid_method.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), true);

        // set first of range to 0x01 and last to 0x00 which is invalid
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_invalid_method_2 = policy_update_payload;
        policy_invalid_method_2[134] = 0x00;
        policy_invalid_method_2[135] = 0x00;
        its_buffer_ptr = policy_invalid_method_2.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), false);

        // set first of range to 0x06 and last to 0x05 which is invalid
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_invalid_method_3 = policy_update_payload;
        policy_invalid_method_3[132] = 0x00;
        policy_invalid_method_3[133] = 0x06;
        its_buffer_ptr = policy_invalid_method_3.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), false);

        // set first of range to 0x01 and last to 0xFFFF which is invalid as first and last
        // must be set to 0xFFFF if ANY_METHOD / ANY_INSTANCE shall be specified
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_invalid_method_4 = policy_update_payload;
        policy_invalid_method_4[134] = 0xFF;
        policy_invalid_method_4[135] = 0xFF;
        its_buffer_ptr = policy_invalid_method_4.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), false);

        // set first of range to 0xFFFF and last to 0xFFFF which is valid
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_valid_method = policy_update_payload;
        policy_valid_method[132] = 0xFF;
        policy_valid_method[133] = 0xFF;
        policy_valid_method[134] = 0xFF;
        policy_valid_method[135] = 0xFF;
        its_buffer_ptr = policy_valid_method.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), true);

        // set first of range to 0x01 and last to 0x01 which is valid
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_valid_method_2 = policy_update_payload;
        policy_valid_method_2[132] = 0x00;
        policy_valid_method_2[133] = 0x01;
        policy_valid_method_2[134] = 0x00;
        policy_valid_method_2[135] = 0x01;
        its_buffer_ptr = policy_valid_method_2.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), true);

        // set first of range to 0x01 and last to 0xFFFE which is valid
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_valid_method_3 = policy_update_payload;
        policy_valid_method_3[132] = 0x00;
        policy_valid_method_3[133] = 0x01;
        policy_valid_method_3[134] = 0xFF;
        policy_valid_method_3[135] = 0xFE;
        its_buffer_ptr = policy_valid_method_3.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), true);

        // set first of range to 0xFFFF and last to 0x05 which is invalid
        its_remaining_bytes_ = policy_update_payload.size();
        std::array<unsigned char, 280> policy_invalid_method_6 = policy_update_payload;
        policy_invalid_method_6[132] = 0xFF;
        policy_invalid_method_6[133] = 0xFF;
        its_buffer_ptr = policy_invalid_method_6.data();
        EXPECT_EQ(::vsomeip::utility::parse_policy(its_buffer_ptr, its_remaining_bytes_, its_uid, its_gid, its_policy), false);

        VSOMEIP_INFO << "waiting until security_config plugin service is available!";
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_security_config_service_available_) {
                condition_.wait(its_lock);
            }
            wait_until_security_config_service_available_ = true;
            VSOMEIP_INFO << "security config plugin service is available";
        }

        if (update_only_) {
            VSOMEIP_INFO << "ONLY Calling updateAcl with user ID 1001 and exit -> shall allow method calls and events...";
            call_acl_deployment_interface(true);
            sleep(1);
            exit(0);
        } else if (remove_only_) {
            VSOMEIP_INFO << "ONLY Calling removeAcl with user ID 1001 and exit -> removes complete allow policy (disables all method calls / events)";
            call_acl_deployment_interface(false);
            sleep(1);
            exit(0);
        } else if (subscribe_only_) {
            VSOMEIP_INFO << "Subscribe to still blacklisted event 0x8004 and do not expect an event!";
            // check that events for service 0x101 and event 0x8004 are never received as policy does not allow it
            std::set<vsomeip::eventgroup_t> its_eventgroups;
            its_eventgroups.insert(security_config_plugin_test::security_config_test_serviceinfo_1.eventgroup_id);
            app_->request_event(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                    security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                    static_cast<vsomeip::event_t>(0x8004),
                    its_eventgroups, true);
            app_->subscribe(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                    security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                    security_config_plugin_test::security_config_test_serviceinfo_1.eventgroup_id,
                    vsomeip::DEFAULT_MAJOR, vsomeip::subscription_type_e::SU_RELIABLE_AND_UNRELIABLE,
                    static_cast<vsomeip::event_t>(0x8004));
            sleep(1);
            EXPECT_EQ(number_of_received_events_4, (uint32_t) 0);
            exit(0);
        }

        VSOMEIP_INFO << "Do method calls to blacklisted service/methods 0x101/0x01 and 0x102/0x02 and do not expect any response";

        // do method calls to blacklisted services -> no response expected
        call_method(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                    security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                    security_config_plugin_test::security_config_test_serviceinfo_1.method_id);

        call_method(security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
                    security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
                    security_config_plugin_test::security_config_test_serviceinfo_2.method_id);


        // check that no events are received before policy allows subscribing
        VSOMEIP_INFO << "Subscribe to blacklisted service/event 0x101/0x8001 and do not expect any event";
        std::set<vsomeip::eventgroup_t> its_eventgroups;
        its_eventgroups.insert(security_config_plugin_test::security_config_test_serviceinfo_1.eventgroup_id);

        app_->request_event(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                static_cast<vsomeip::event_t>(security_config_plugin_test::security_config_test_serviceinfo_1.event_id),
                its_eventgroups, true);
        app_->subscribe(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.eventgroup_id,
                vsomeip::DEFAULT_MAJOR, vsomeip::subscription_type_e::SU_RELIABLE_AND_UNRELIABLE,
                static_cast<vsomeip::event_t>(security_config_plugin_test::security_config_test_serviceinfo_1.event_id));

        sleep(1);

        EXPECT_EQ(number_of_received_responses_method_1, (uint32_t) 0);
        EXPECT_EQ(number_of_received_responses_method_2, (uint32_t) 0);
        EXPECT_EQ(number_of_received_events_1, (uint32_t) 0);
        EXPECT_EQ(number_of_received_events_2, (uint32_t) 0);

        VSOMEIP_INFO << "Calling updateAcl which shall allow method calls and events...";
        call_acl_deployment_interface(true);
        VSOMEIP_INFO << "wait until control service is available ...";

        // wait until control service is available
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_control_service_available_) {
                condition_.wait(its_lock);
            }
            wait_until_control_service_available_ = true;
            VSOMEIP_INFO << "local control service 0x0103 available";
        }
        // Todo wait until plugin has answered the acl update
        sleep(1);

        // Send message_try_offer request in order to trigger stopoffer and reoffering of now allowed services
        VSOMEIP_INFO << "trigger stopoffer / reoffer of service 0x0101 and 0x0102 ...";
        call_try_offer();

        // try requesting the services 0x0101 and 0x0102 again
        // (initial request service was rejected before which causes no routing info to be sent after policy update)
        app_->request_service(
                security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR);

        app_->request_service(
                security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_3.instance_id,
                vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR);

        // check that services 0x0101 get available after acl update
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_updated_service_101_available_) {
                condition_.wait(its_lock);
            }
            wait_until_updated_service_101_available_ = true;
            VSOMEIP_INFO << "By policy update allowed service 0x0101 got available";
        }

        // check that services 0x0102 get available after acl update
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_updated_service_102_available_) {
                condition_.wait(its_lock);
            }
            wait_until_updated_service_102_available_ = true;
            VSOMEIP_INFO << "By policy update allowed service 0x0102 got available";
        }

        VSOMEIP_INFO << "Call now allowed service/methods 0x101/0x01 and 0x102/0x02 after allow policy update and expect responses!";

        // call method ids which should now be allowed to be requested
        call_method(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                    security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                    security_config_plugin_test::security_config_test_serviceinfo_1.method_id);

        // check that method calls are working
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_method_1_responses_received_) {
                condition_.wait(its_lock);
            }
            wait_until_method_1_responses_received_ = true;
            VSOMEIP_INFO << "By policy update allowed response to request of service/instance/method "
                    << std::hex << security_config_plugin_test::security_config_test_serviceinfo_1.service_id
                    << "/" << security_config_plugin_test::security_config_test_serviceinfo_1.instance_id
                    << "/" << security_config_plugin_test::security_config_test_serviceinfo_1.method_id << " received";
        }

        call_method(security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
                    security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
                    security_config_plugin_test::security_config_test_serviceinfo_2.method_id);
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_method_2_responses_received_) {
                condition_.wait(its_lock);
            }
            wait_until_method_2_responses_received_ = true;
            VSOMEIP_INFO << "By policy update allowed response to request of service/instance/method "
                    << std::hex << security_config_plugin_test::security_config_test_serviceinfo_2.service_id
                    << "/" << security_config_plugin_test::security_config_test_serviceinfo_2.instance_id
                    << "/" << security_config_plugin_test::security_config_test_serviceinfo_2.method_id << " received";
        }

        EXPECT_EQ(number_of_received_responses_method_1, (uint32_t) 1);
        EXPECT_EQ(number_of_received_responses_method_2, (uint32_t) 1);

        // check that events are received now
        VSOMEIP_INFO << "Subscribe to service 0x101 / 0x102 events 0x8001 / 0x8002 and expect initial events";
        app_->request_event(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                static_cast<vsomeip::event_t>(security_config_plugin_test::security_config_test_serviceinfo_1.event_id),
                its_eventgroups, true);
        app_->subscribe(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.eventgroup_id,
                vsomeip::DEFAULT_MAJOR, vsomeip::subscription_type_e::SU_RELIABLE_AND_UNRELIABLE,
                static_cast<vsomeip::event_t>(security_config_plugin_test::security_config_test_serviceinfo_1.event_id));

        // check that from first service events were received
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_notifications_1_received_) {
                condition_.wait(its_lock);
            }
            wait_until_notifications_1_received_ = true;
            VSOMEIP_INFO << "notifications from service 0x0101 received";
        }

        app_->request_event(security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
                static_cast<vsomeip::event_t>(security_config_plugin_test::security_config_test_serviceinfo_2.event_id),
                its_eventgroups, true);
        app_->subscribe(security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
                security_config_plugin_test::security_config_test_serviceinfo_2.eventgroup_id,
                vsomeip::DEFAULT_MAJOR, vsomeip::subscription_type_e::SU_RELIABLE_AND_UNRELIABLE,
                static_cast<vsomeip::event_t>(security_config_plugin_test::security_config_test_serviceinfo_2.event_id));

        // check that from second service events were received
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_notifications_2_received_) {
                condition_.wait(its_lock);
            }
            wait_until_notifications_2_received_ = true;
            VSOMEIP_INFO << "notifications from service 0x0102 received";
        }

        VSOMEIP_INFO << "Subscribe to still blacklisted event 0x8004 and do not expect an event!";
        // check that events for service 0x101 and event 0x8004 are never received as policy does not allow it
        app_->request_event(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                static_cast<vsomeip::event_t>(0x8004),
                its_eventgroups, true);
        app_->subscribe(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.eventgroup_id,
                vsomeip::DEFAULT_MAJOR, vsomeip::subscription_type_e::SU_RELIABLE_AND_UNRELIABLE,
                static_cast<vsomeip::event_t>(0x8004));
        sleep(1);
        EXPECT_EQ(number_of_received_events_4, (uint32_t) 0);


        // unsusbcribe
        app_->unsubscribe(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.eventgroup_id);

        app_->unsubscribe(security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
                security_config_plugin_test::security_config_test_serviceinfo_2.eventgroup_id);

        // tigger removeAcl (removes the whole configuration for uid 1000)
        VSOMEIP_INFO << "Calling removeAcl which removes complete allow policy (disables all method calls / events)";
        call_acl_deployment_interface(false);

        //Todo wait for plugin response
        sleep(1);

        VSOMEIP_INFO << "Call now blacklisted methods after complete allow policy removal and do not expect any response!";
        // no response expected as policy was removed
        call_method(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                    security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                    security_config_plugin_test::security_config_test_serviceinfo_1.method_id);

        call_method(security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
                    security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
                    security_config_plugin_test::security_config_test_serviceinfo_2.method_id);

        // check that no more initial events are received now on resubscribe
        VSOMEIP_INFO << "Subscribe to now blacklisted events after complete allow policy removal and do not expect initial events!";
        app_->subscribe(security_config_plugin_test::security_config_test_serviceinfo_1.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.instance_id,
                security_config_plugin_test::security_config_test_serviceinfo_1.eventgroup_id);

        app_->subscribe(security_config_plugin_test::security_config_test_serviceinfo_2.service_id,
                security_config_plugin_test::security_config_test_serviceinfo_2.instance_id,
                security_config_plugin_test::security_config_test_serviceinfo_2.eventgroup_id);

        sleep(1);

        EXPECT_EQ(number_of_received_responses_method_1, (uint32_t) 1);
        EXPECT_EQ(number_of_received_responses_method_2, (uint32_t) 1);
        EXPECT_EQ(number_of_received_events_1, (uint32_t) 1);
        EXPECT_EQ(number_of_received_events_2, (uint32_t) 1);

        {
            std::lock_guard<std::mutex> its_lock(stop_mutex_);
            sleep(1);
            wait_for_stop_ = false;
            stop_condition_.notify_one();
        }
    }

private:
    bool update_only_;
    bool remove_only_;
    bool subscribe_only_;
    std::shared_ptr<vsomeip::application> app_;
    bool wait_until_registered_;
    bool wait_until_security_config_service_available_;
    bool wait_until_control_service_available_;
    bool wait_until_updated_service_101_available_;
    bool wait_until_updated_service_102_available_;
    bool wait_until_method_1_responses_received_;
    bool wait_until_method_2_responses_received_;
    uint32_t number_of_received_responses_method_1;
    uint32_t number_of_received_responses_method_2;
    uint32_t number_of_received_events_1;
    uint32_t number_of_received_events_2;
    uint32_t number_of_received_events_4;
    bool wait_until_notifications_1_received_;
    bool wait_until_notifications_2_received_;
    std::mutex mutex_;
    std::condition_variable condition_;

    bool wait_for_stop_;
    bool update_ok_;
    bool removal_ok_;

    std::mutex stop_mutex_;
    std::condition_variable stop_condition_;
    std::thread stop_thread_;

    std::thread run_thread_;
    std::map<std::pair<vsomeip::service_t, vsomeip::method_t>, std::uint32_t> other_services_received_notification_;

};

#if 1
static bool only_update;
static bool only_remove;
static bool only_subscribe;
#endif

extern "C" void signal_handler(int signum) {
    the_client->handle_signal(signum);
}

TEST(someip_security_config_plugin_test, update_remove_security_config_policy)
{
        security_config_plugin_test_client its_sample(only_update, only_remove, only_subscribe);
}

#ifndef _WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
#if 1
    only_update = false;
    only_remove = false;
    only_subscribe = false;
    if (argc > 1) {
        if(std::string("UPDATE") == std::string(argv[1])) {
            only_update = true;
        } else if(std::string("REMOVE") == std::string(argv[1])) {
            only_remove = true;
        } else if (std::string("SUBSCRIBE") == std::string(argv[1])) {
            only_subscribe = true;
        }
    }

#endif

    return RUN_ALL_TESTS();
}
#endif
