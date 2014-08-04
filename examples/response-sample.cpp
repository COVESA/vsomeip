// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include <vsomeip/vsomeip.hpp>

#include "sample-ids.hpp"

class service_sample {
 public:
  service_sample(bool _use_tcp)
      : app_(vsomeip::runtime::get()->create_application()),
        is_registered_(false),
        use_tcp_(_use_tcp),
        offer_thread_(std::bind(&service_sample::run, this)),
        notification_thread_(std::bind(&service_sample::notify, this)) {
  }

  void init() {
    std::lock_guard < std::mutex > its_lock(mutex_);

    app_->init();
    app_->register_message_handler(
        SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_METHOD_ID,
        std::bind(&service_sample::on_message, this, std::placeholders::_1));

    app_->register_event_handler(
        std::bind(&service_sample::on_event, this, std::placeholders::_1));

    blocked_ = true;
    condition_.notify_one();
  }

  void start() {
    app_->start();
  }

  void offer() {
    VSOMEIP_INFO << "Offering service.";
    //std::lock_guard < std::mutex > its_lock(notification_mutex_);
    app_->offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);

    notification_blocked_ = true;
    //condition_.notify_one();
  }

  void stop_offer() {
    VSOMEIP_INFO << "Stop offering service.";
    app_->stop_offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
    notification_blocked_ = false;
  }

  void update() {
    static std::shared_ptr<vsomeip::payload> its_payload =
        vsomeip::runtime::get()->create_payload();

    static vsomeip::byte_t its_data[1000];
    static uint32_t its_size = 0;

    its_size++;
    for (uint32_t i = 0; i < its_size; ++i)
      its_data[i] = (i % 256);

    its_payload->set_data(its_data, its_size);

    VSOMEIP_INFO << "Updating event to " << its_size << " bytes.";
    app_->set(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_EVENT_ID,
              its_payload);
  }

  void on_event(vsomeip::event_type_e _event) {
    VSOMEIP_INFO << "Application " << app_->get_name() << " is "
        << (_event == vsomeip::event_type_e::REGISTERED ?
            "registered." : "deregistered.");

    if (_event == vsomeip::event_type_e::REGISTERED) {
      if (!is_registered_) {
        is_registered_ = true;
      }
    } else {
      is_registered_ = false;
    }
  }

  void on_message(std::shared_ptr<vsomeip::message> &_request) {
    VSOMEIP_INFO << "Received a message with Client/Session [" << std::setw(4)
        << std::setfill('0') << std::hex << _request->get_client() << "/"
        << std::setw(4) << std::setfill('0') << std::hex
        << _request->get_session() << "]";

    std::shared_ptr<vsomeip::message> its_response = vsomeip::runtime::get()
        ->create_response(_request);

    std::shared_ptr<vsomeip::payload> its_payload = vsomeip::runtime::get()
        ->create_payload();
    std::vector<vsomeip::byte_t> its_payload_data;
    for (std::size_t i = 0; i < 120; ++i)
      its_payload_data.push_back(i % 256);
    its_payload->set_data(its_payload_data);
    its_response->set_payload(its_payload);

    app_->send(its_response, true, use_tcp_);
  }

  void run() {
    std::unique_lock < std::mutex > its_lock(mutex_);
    while (!blocked_)
      condition_.wait(its_lock);

    bool is_offer(true);
    while (true) {
      if (is_offer)
        offer();
      else
        stop_offer();
      std::this_thread::sleep_for(std::chrono::milliseconds(10000));
      is_offer = !is_offer;
    }
  }

  void notify() {
    while (true) {
      //std::unique_lock < std::mutex > its_lock(notification_mutex_);
      //while (!notification_blocked_)
      //  notification_condition_.wait(its_lock);
      //while (notification_blocked_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (notification_blocked_) update();
      //}
    }
  }

 private:
  std::shared_ptr<vsomeip::application> app_;
  bool is_registered_;
  bool use_tcp_;

  std::thread offer_thread_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool blocked_;

  std::thread notification_thread_;
  std::mutex notification_mutex_;
  std::condition_variable notification_condition_;
  bool notification_blocked_;
};

int main(int argc, char **argv) {
  bool use_tcp = false;

  std::string tcp_enable("--tcp");
  std::string udp_enable("--udp");

  for (int i = 1; i < argc; i++) {
    if (tcp_enable == argv[i]) {
      use_tcp = true;
      break;
    }
    if (udp_enable == argv[i]) {
      use_tcp = false;
      break;
    }
  }

  service_sample its_sample(use_tcp);
  its_sample.init();
  its_sample.start();

  return 0;
}
