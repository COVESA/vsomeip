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

#include <gtest/gtest.h>

#ifndef _WIN32
#include <signal.h>
#endif

#include <vsomeip/vsomeip.hpp>
#include "../../implementation/logging/include/logger.hpp"

#include "initial_event_test_globals.hpp"
class initial_event_test_client;
static initial_event_test_client* the_client;
extern "C" void signal_handler(int _signum);


class initial_event_test_client {
public:
    initial_event_test_client(int _client_number,
                              vsomeip::subscription_type_e _subscription_type,
                              std::array<initial_event_test::service_info, 7> _service_infos,
                              bool _subscribe_on_available, std::uint32_t _events_to_subscribe,
                              bool _initial_event_strict_checking,
                              bool _dont_exit, bool _subscribe_only_one) :
            client_number_(_client_number),
            service_infos_(_service_infos),
            subscription_type_(_subscription_type),
            app_(vsomeip::runtime::get()->create_application()),
            wait_until_registered_(true),
            wait_until_other_services_available_(true),
            wait_for_stop_(true),
            subscribe_on_available_(_subscribe_on_available),
            events_to_subscribe_(_events_to_subscribe),
            initial_event_strict_checking_(_initial_event_strict_checking),
            dont_exit_(_dont_exit),
            subscribe_only_one_(_subscribe_only_one),
            stop_thread_(std::bind(&initial_event_test_client::wait_for_stop, this)) {
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
                std::bind(&initial_event_test_client::on_state, this,
                        std::placeholders::_1));

        app_->register_message_handler(vsomeip::ANY_SERVICE,
                vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD,
                std::bind(&initial_event_test_client::on_message, this,
                        std::placeholders::_1));

        // register availability for all other services and request their event.
        for(const auto& i : service_infos_) {
            if (i.service_id == 0xFFFF && i.instance_id == 0xFFFF) {
                continue;
            }
            app_->register_availability_handler(i.service_id, i.instance_id,
                    std::bind(&initial_event_test_client::on_availability, this,
                            std::placeholders::_1, std::placeholders::_2,
                            std::placeholders::_3));
            app_->request_service(i.service_id, i.instance_id);

            std::set<vsomeip::eventgroup_t> its_eventgroups;
            its_eventgroups.insert(i.eventgroup_id);
            for (std::uint32_t j = 0; j < events_to_subscribe_; j++ ) {
                app_->request_event(i.service_id, i.instance_id,
                        static_cast<vsomeip::event_t>(i.event_id + j),
                        its_eventgroups, true);
            }

            other_services_available_[std::make_pair(i.service_id, i.instance_id)] = false;

            if (!subscribe_on_available_) {
                if (events_to_subscribe_ == 1) {
                    app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id,
                                    vsomeip::DEFAULT_MAJOR, subscription_type_);
                    other_services_received_notification_[std::make_pair(i.service_id, i.event_id)] = 0;
                } else if (events_to_subscribe_ > 1) {
                    if (!subscribe_only_one_) {
                        for (std::uint32_t j = 0; j < events_to_subscribe_; j++ ) {
                            app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id,
                                            vsomeip::DEFAULT_MAJOR, subscription_type_,
                                            static_cast<vsomeip::event_t>(i.event_id + j));
                            other_services_received_notification_[std::make_pair(i.service_id, i.event_id + j)] = 0;
                        }
                    } else {
                        app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id,
                                        vsomeip::DEFAULT_MAJOR, subscription_type_,
                                        static_cast<vsomeip::event_t>(i.event_id));
                        other_services_received_notification_[std::make_pair(i.service_id, i.event_id)] = 0;
                    }
                }
            } else {
                for (std::uint32_t j = 0; j < events_to_subscribe_; j++ ) {
                    other_services_received_notification_[std::make_pair(i.service_id, i.event_id + j)] = 0;
                }
            }
        }

        app_->start();
    }

    ~initial_event_test_client() {
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
            auto its_service = other_services_available_.find(std::make_pair(_service, _instance));
            if(its_service != other_services_available_.end()) {
                if(its_service->second != _is_available) {
                its_service->second = true;
                VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
                        << client_number_ << "] Service ["
                << std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
                << "] is available.";

                }
            }

            if(std::all_of(other_services_available_.cbegin(),
                           other_services_available_.cend(),
                           [](const std::map<std::pair<vsomeip::service_t,
                                   vsomeip::instance_t>, bool>::value_type& v) {
                                return v.second;})) {
                VSOMEIP_INFO << "[" << std::setw(4) << std::setfill('0') << std::hex
                        << client_number_ << "] all services are available.";
                if (subscribe_on_available_) {
                    for(const auto& i : service_infos_) {
                        if (i.service_id == 0xFFFF && i.instance_id == 0xFFFF) {
                            continue;
                        }
                        if (events_to_subscribe_ == 1 ) {
                            app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id,
                                    vsomeip::DEFAULT_MAJOR, subscription_type_);
                        } else if (events_to_subscribe_ > 1) {
                            for (std::uint32_t j = 0; j < events_to_subscribe_; j++ ) {
                                app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id,
                                        vsomeip::DEFAULT_MAJOR, subscription_type_,
                                        static_cast<vsomeip::event_t>(i.event_id + j));
                            }
                        }
                    }
                }
            }
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message> &_message) {
        if(_message->get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {

            other_services_received_notification_[std::make_pair(_message->get_service(),
                                                             _message->get_method())]++;

            VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
            << client_number_ << "] "
            << "Received a notification with Client/Session [" << std::setw(4)
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
            bool notify(false);
            switch(subscription_type_) {
                case vsomeip::subscription_type_e::SU_UNRELIABLE:
                case vsomeip::subscription_type_e::SU_RELIABLE:
                case vsomeip::subscription_type_e::SU_PREFER_UNRELIABLE:
                case vsomeip::subscription_type_e::SU_PREFER_RELIABLE:
                    if (all_notifications_received()) {
                        notify = true;
                    }
                    break;
                case vsomeip::subscription_type_e::SU_RELIABLE_AND_UNRELIABLE:
                    if (all_notifications_received_tcp_and_udp()) {
                        notify = true;
                    }
                    break;
            }

            if(notify && !dont_exit_) {
                std::lock_guard<std::mutex> its_lock(stop_mutex_);
                wait_for_stop_ = false;
                stop_condition_.notify_one();
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
                    if (v.second == initial_event_test::notifications_to_send) {
                        return true;
                    } else {
                        if (v.second >= initial_event_test::notifications_to_send) {
                            VSOMEIP_WARNING
                                    << "[" << std::setw(4) << std::setfill('0') << std::hex
                                        << client_number_ << "] "
                                    << " Received multiple initial events from service/instance: "
                                    << std::setw(4) << std::setfill('0') << std::hex << v.first.first
                                    << "."
                                    << std::setw(4) << std::setfill('0') << std::hex << v.first.second
                                    << " number of received events: " << v.second
                                    << ". This is caused by StopSubscribe/Subscribe messages and/or"
                                    << " service offered via UDP and TCP";
                            if (initial_event_strict_checking_) {
                                ADD_FAILURE() << "[" << std::setw(4) << std::setfill('0') << std::hex
                                    << client_number_ << "] "
                                    << " Received multiple initial events from service/instance: "
                                    << std::setw(4) << std::setfill('0') << std::hex << v.first.first
                                    << "."
                                    << std::setw(4) << std::setfill('0') << std::hex << v.first.second
                                    << " number of received events: " << v.second;
                            }
                            return initial_event_strict_checking_ ? false : true;

                        } else {
                            return false;
                        }
                    }
                }
        );
    }

    bool all_notifications_received_tcp_and_udp() {
        std::uint32_t received_twice(0);
        std::uint32_t received_normal(0);
        for(const auto &v : other_services_received_notification_) {
            if (!initial_event_strict_checking_ &&
                    v.second > initial_event_test::notifications_to_send * 2) {
                VSOMEIP_WARNING
                        << "[" << std::setw(4) << std::setfill('0') << std::hex
                        << client_number_ << "] "
                        << " Received multiple initial events from service/instance: "
                        << std::setw(4) << std::setfill('0') << std::hex << v.first.first
                        << "."
                        << std::setw(4) << std::setfill('0') << std::hex << v.first.second
                        << ". This is caused by StopSubscribe/Subscribe messages and/or"
                        << " service offered via UDP and TCP";
                received_twice++;
            } else if (initial_event_strict_checking_ &&
                    v.second > initial_event_test::notifications_to_send * 2) {
                ADD_FAILURE() << "[" << std::setw(4) << std::setfill('0') << std::hex
                    << client_number_ << "] "
                    << " Received multiple initial events from service/instance: "
                    << std::setw(4) << std::setfill('0') << std::hex << v.first.first
                    << "."
                    << std::setw(4) << std::setfill('0') << std::hex << v.first.second
                    << " number of received events: " << v.second;
            } else if (v.second == initial_event_test::notifications_to_send * 2) {
                received_twice++;
            } else if(v.second == initial_event_test::notifications_to_send) {
                received_normal++;
            }
        }

        if(   received_twice == ((service_infos_.size() - 1) * events_to_subscribe_)/ 2
           && received_normal == ((service_infos_.size() - 1) * events_to_subscribe_)/ 2) {
            // routing manager stub receives the notification
            // - twice from external nodes
            // - and normal from all internal nodes
            VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
                        << client_number_ << "] "
                        << "Received notifications:"
                        << " Normal: " << received_normal
                        << " Twice: " << received_twice;
            return true;
        } else if (initial_event_strict_checking_ && (
                received_twice > ((service_infos_.size() - 1) * events_to_subscribe_)/ 2)) {
            ADD_FAILURE() << "[" << std::setw(4) << std::setfill('0') << std::hex
                << client_number_ << "] "
                << " Received too much initial events twice: " << received_twice;
        } else if (received_normal == (events_to_subscribe_ * (service_infos_.size() - 1))) {
            return true;
        }
        return false;
    }

    void handle_signal(int _signum) {
        VSOMEIP_DEBUG << "[" << std::setw(4) << std::setfill('0') << std::hex
                << client_number_ << "] Catched signal, going down ("
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
            VSOMEIP_INFO << "[" << std::setw(4) << std::setfill('0') << std::hex
                    << client_number_
                    << "] Received notifications from all services, going down";
        }
        for (const auto& i : service_infos_) {
            if (i.service_id == 0xFFFF && i.instance_id == 0xFFFF) {
                continue;
            }
            app_->unsubscribe(i.service_id, i.instance_id, i.eventgroup_id);
        }
        app_->clear_all_handler();
        app_->stop();
    }

private:
    int client_number_;
    std::array<initial_event_test::service_info, 7> service_infos_;
    vsomeip::subscription_type_e subscription_type_;
    std::shared_ptr<vsomeip::application> app_;
    std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, bool> other_services_available_;
    std::map<std::pair<vsomeip::service_t, vsomeip::method_t>, std::uint32_t> other_services_received_notification_;

    bool wait_until_registered_;
    bool wait_until_other_services_available_;
    std::mutex mutex_;
    std::condition_variable condition_;

    bool wait_for_stop_;

    bool subscribe_on_available_;
    std::uint32_t events_to_subscribe_;
    bool initial_event_strict_checking_;
    bool dont_exit_;
    bool subscribe_only_one_;

    std::mutex stop_mutex_;
    std::condition_variable stop_condition_;
    std::thread stop_thread_;
};

static int client_number;
static vsomeip::subscription_type_e subscription_type;
static bool use_same_service_id;
static bool subscribe_on_available;
static std::uint32_t subscribe_multiple_events;
static bool initial_event_strict_checking;
static bool dont_exit;
static bool subscribe_only_one;

extern "C" void signal_handler(int signum) {
    the_client->handle_signal(signum);
}

TEST(someip_initial_event_test, wait_for_initial_events_of_all_services)
{
    if(use_same_service_id) {
        initial_event_test_client its_sample(client_number,
                subscription_type,
                initial_event_test::service_infos_same_service_id, subscribe_on_available,
                subscribe_multiple_events, initial_event_strict_checking, dont_exit,
                subscribe_only_one);
    } else {
        initial_event_test_client its_sample(client_number, subscription_type,
                initial_event_test::service_infos, subscribe_on_available,
                subscribe_multiple_events, initial_event_strict_checking, dont_exit,
                subscribe_only_one);
    }
}

#ifndef _WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if(argc < 3) {
        std::cerr << "Please specify a client number and subscription type, like: " << argv[0] << " 2 UDP SUBSCRIBE_BEFORE_START SAME_SERVICE_ID" << std::endl;
        std::cerr << "Valid client numbers are from 0 to 0xFFFF" << std::endl;
        std::cerr << "Valid subscription types include:" << std::endl;
        std::cerr << "[TCP_AND_UDP, PREFER_UDP, PREFER_TCP, UDP, TCP]" << std::endl;
        std::cerr << "After client number and subscription types one/multiple of these flags can be specified:";
        std::cerr << " - Time of subscription, valid values: [SUBSCRIBE_ON_AVAILABILITY, SUBSCRIBE_BEFORE_START], default SUBSCRIBE_BEFORE_START" << std::endl;
        std::cerr << " - SAME_SERVICE_ID flag. If set the test is run w/ multiple instances of the same service, default false" << std::endl;
        std::cerr << " - MULTIPLE_EVENTS flag. If set the test will subscribe to multiple events in the eventgroup, default false" << std::endl;
        std::cerr << " - STRICT_CHECKING flag. If set the test will only successfully finish if exactly the number of initial events were received (and not more). Default false" << std::endl;
        std::cerr << " - DONT_EXIT flag. If set the test will not exit if all notifications have been received. Default false" << std::endl;
        std::cerr << " - SUBSCRIBE_ONLY_ONE flag. If set the test will only subscribe to one event even if MULTIPLE_EVENTS is set. Default false" << std::endl;
        return 1;
    }

    client_number = std::stoi(std::string(argv[1]), nullptr);

    if(std::string("TCP_AND_UDP") == std::string(argv[2])) {
        subscription_type = vsomeip::subscription_type_e::SU_RELIABLE_AND_UNRELIABLE;
    } else if(std::string("PREFER_UDP") == std::string(argv[2])) {
        subscription_type = vsomeip::subscription_type_e::SU_PREFER_UNRELIABLE;
    } else if(std::string("PREFER_TCP") == std::string(argv[2])) {
        subscription_type = vsomeip::subscription_type_e::SU_PREFER_RELIABLE;
    } else if(std::string("UDP") == std::string(argv[2])) {
        subscription_type = vsomeip::subscription_type_e::SU_UNRELIABLE;
    } else if(std::string("TCP") == std::string(argv[2])) {
        subscription_type = vsomeip::subscription_type_e::SU_RELIABLE;
    } else {
        std::cerr << "Wrong subscription type passed, exiting" << std::endl;
        std::cerr << "Valid subscription types include:" << std::endl;
        std::cerr << "[TCP_AND_UDP, PREFER_UDP, PREFER_TCP, UDP, TCP]" << std::endl;
        return 1;
    }

    subscribe_on_available = false;
    initial_event_strict_checking = false;
    use_same_service_id = false;
    subscribe_multiple_events = 1;
    dont_exit = false;
    subscribe_only_one = false;
    if (argc > 3) {
        for (int i = 3; i < argc; i++) {
            if (std::string("SUBSCRIBE_ON_AVAILABILITY") == std::string(argv[i])) {
                subscribe_on_available = true;
            } else if (std::string("SUBSCRIBE_BEFORE_START") == std::string(argv[i])) {
                subscribe_on_available = false;
            } else if (std::string("SAME_SERVICE_ID") == std::string(argv[i])) {
                use_same_service_id = true;
            } else if (std::string("MULTIPLE_EVENTS") == std::string(argv[i])) {
                subscribe_multiple_events = 5;
            } else if (std::string("STRICT_CHECKING") == std::string(argv[i])) {
                initial_event_strict_checking = true;
            } else if (std::string("DONT_EXIT") == std::string(argv[i])) {
                dont_exit = true;
            } else if (std::string("SUBSCRIBE_ONLY_ONE") == std::string(argv[i])) {
                subscribe_only_one = true;
            }
        }
    }

    return RUN_ALL_TESTS();
}
#endif
