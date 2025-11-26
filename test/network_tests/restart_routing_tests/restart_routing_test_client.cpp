// Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include "restart_routing_test_client.hpp"
#include <boost/interprocess/managed_shared_memory.hpp>

namespace bpi = boost::interprocess;

routing_restart_test_client::routing_restart_test_client(uint32_t app_id) :
    app_(vsomeip::runtime::get()->create_application()), is_available_(false), received_responses_(0), app_id_{app_id} { }

void routing_restart_test_client::stop() {
    VSOMEIP_INFO << "Stopping...";

    shutdown_service();

    // magic sleep to give time for the last message to be sent
    // TODO: FIXME! REMOVE THIS!
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    app_->clear_all_handler();
    app_->stop();
    if (starter_.joinable()) {
        starter_.join();
    }
}

void routing_restart_test_client::on_state(vsomeip::state_type_e _state) {
    if (_state == vsomeip::state_type_e::ST_REGISTERED) {
        app_->request_service(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID, false);
        {
            bpi::scoped_lock<bpi::interprocess_mutex> its_lock(ip_sync->client_mutex_);
            ip_sync->client_status_[app_id_] =
                    restart_routing::restart_routing_test_interprocess_sync::registration_status::STATE_REGISTERED;
        }
        restart_routing::restart_routing_test_interprocess_utils::notify_all_component_unlocked(ip_sync->client_cv_);
    }
}

void routing_restart_test_client::on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {

    VSOMEIP_INFO << std::hex << "Client 0x" << app_->get_client() << " : Service [" << std::hex << std::setfill('0') << std::setw(4)
                 << _service << "." << _instance << "] is " << (_is_available ? "available." : "NOT available.");

    if (vsomeip_test::TEST_SERVICE_SERVICE_ID == _service && vsomeip_test::TEST_SERVICE_INSTANCE_ID == _instance) {
        std::unique_lock<std::mutex> its_lock(mutex_);
        if (is_available_ && !_is_available) {
            is_available_ = false;
        } else if (_is_available && !is_available_) {
            is_available_ = true;
            condition_.notify_one();
        }
    }
}

void routing_restart_test_client::on_message(const std::shared_ptr<vsomeip::message>& _response) {
    VSOMEIP_INFO << "Received a response from Service [" << std::hex << std::setfill('0') << std::setw(4) << _response->get_service() << "."
                 << std::setw(4) << _response->get_instance() << "] to Client/Session [" << std::setw(4) << _response->get_client() << "/"
                 << std::setw(4) << _response->get_session() << "]";

    if (_response->get_service() == vsomeip_test::TEST_SERVICE_SERVICE_ID
        && _response->get_instance() == vsomeip_test::TEST_SERVICE_INSTANCE_ID) {
        VSOMEIP_INFO << " Currently received responses " << received_responses_;

        received_responses_++;
        if (received_responses_ == vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND_ROUTING_RESTART_TESTS) {
            VSOMEIP_WARNING << std::hex << app_->get_client() << ": Received all messages ~> going down!";
            all_responses_received_.set_value();
        }
    }
}

void routing_restart_test_client::run() {
    // Create a shared memory object.
    bpi::shared_memory_object shm(bpi::open_only, // only open
                                  "RestartRoutingSync", // name
                                  bpi::read_write // read-write mode
    );
    // Map the whole shared memory in this process
    bpi::mapped_region region(shm, // What to map
                              bpi::read_write // Map it as read-write
    );
    void* addr = region.get_address();
    ip_sync = static_cast<restart_routing::restart_routing_test_interprocess_sync*>(addr);

    if (!app_->init()) {
        ADD_FAILURE() << "Couldn't initialize application";
        exit(1);
    }

    app_->register_state_handler(std::bind(&routing_restart_test_client::on_state, this, std::placeholders::_1));
    app_->register_message_handler(vsomeip::ANY_SERVICE, vsomeip_test::TEST_SERVICE_INSTANCE_ID, vsomeip::ANY_METHOD,
                                   std::bind(&routing_restart_test_client::on_message, this, std::placeholders::_1));
    app_->register_availability_handler(vsomeip_test::TEST_SERVICE_SERVICE_ID, vsomeip_test::TEST_SERVICE_INSTANCE_ID,
                                        std::bind(&routing_restart_test_client::on_availability, this, std::placeholders::_1,
                                                  std::placeholders::_2, std::placeholders::_3));

    std::promise<bool> its_promise;
    starter_ = std::thread([&]() {
        its_promise.set_value(true);
        app_->start();
    });
    EXPECT_TRUE(its_promise.get_future().get());

    {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(ip_sync->client_mutex_);
        restart_routing::restart_routing_test_interprocess_utils::wait_and_check_unlocked(
                ip_sync->client_cv_, its_lock, 10, ip_sync->sending_status_,
                restart_routing::restart_routing_test_interprocess_sync::sending_status::SEND_MESSAGES);
    }

    std::uint32_t its_sent_requests(0);
    bool its_availability_timeout = false;
    while (its_sent_requests < vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND_ROUTING_RESTART_TESTS) {
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (!is_available_) {
                if (!condition_.wait_for(its_lock, std::chrono::milliseconds(10000), [this] { return is_available_; })) {
                    VSOMEIP_WARNING << "Service not available for 10s. Quit waiting";
                    its_availability_timeout = true;
                    break;
                }
                if (its_sent_requests > 0 && received_responses_ > 0 && its_sent_requests > received_responses_) {
                    VSOMEIP_WARNING << "Sent/Recv messages mismatch (" << its_sent_requests << "/" << received_responses_
                                    << ") : Resending non-responded requests";
                    its_sent_requests = received_responses_;
                }
            }
        }

        if (its_availability_timeout) {
            break;
        }
        auto request = vsomeip::runtime::get()->create_request(false);
        request->set_service(vsomeip_test::TEST_SERVICE_SERVICE_ID);
        request->set_instance(vsomeip_test::TEST_SERVICE_INSTANCE_ID);
        request->set_method(vsomeip_test::TEST_SERVICE_METHOD_ID);
        app_->send(request);

        its_sent_requests++;
        VSOMEIP_INFO << "Sent request " << its_sent_requests << " with session id " << std::hex << std::setfill('0') << std::setw(4)
                     << request->get_session();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    if (std::future_status::ready == all_responses_received_.get_future().wait_for(std::chrono::milliseconds(100))) {
        EXPECT_EQ(vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND_ROUTING_RESTART_TESTS, received_responses_);
        VSOMEIP_WARNING << "Received all answers";
    } else {
        ADD_FAILURE() << "Didn't receive all responses within time";
    }

    stop();
}

void routing_restart_test_client::shutdown_service() {
    auto request = vsomeip::runtime::get()->create_request(false);
    request->set_service(vsomeip_test::TEST_SERVICE_SERVICE_ID);
    request->set_instance(vsomeip_test::TEST_SERVICE_INSTANCE_ID);
    request->set_method(vsomeip_test::TEST_SERVICE_METHOD_ID_SHUTDOWN);
    app_->send(request);
}

TEST(someip_restart_routing_test, request_response_over_restart) {
    try {
        routing_restart_test_client test_client{static_cast<std::uint32_t>(std::stoul(getenv("VSOMEIP_APPLICATION_ID"), NULL, 10))};
        test_client.run();
    } catch (const std::exception& e) {
        ADD_FAILURE() << __func__ << ":" << e.what();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
