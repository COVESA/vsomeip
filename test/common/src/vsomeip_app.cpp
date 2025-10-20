// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <iomanip>
#include <iostream>
#include "vsomeip/message.hpp"
#include <vsomeip/vsomeip.hpp>
#include <common/vsomeip_app.hpp>

namespace common {

std::shared_ptr<vsomeip_v3::message> create_standard_vsip_request(vsomeip::service_t _service, vsomeip::instance_t _instance,
                                                                  vsomeip_v3::method_t _method, vsomeip_v3::interface_version_t _interface,
                                                                  vsomeip_v3::message_type_e _message_type) {
    auto its_runtime = vsomeip::runtime::get();
    auto its_payload = its_runtime->create_payload();
    auto its_message = its_runtime->create_request(false);
    its_message->set_service(_service);
    its_message->set_instance(_instance);
    its_message->set_method(_method);
    its_message->set_interface_version(_interface);
    its_message->set_message_type(_message_type);
    its_message->set_payload(its_payload);

    return its_message;
}

base_logger::base_logger(const char* dlt_application_id_, const char* dlt_application_name_) :
    dlt_application_id_(dlt_application_id_), dlt_application_name_(dlt_application_name_) {
#ifdef USE_DLT
#ifndef ANDROID
    DLT_REGISTER_APP(dlt_application_id_, dlt_application_name_);
#endif
#endif
}

base_logger::~base_logger() {
#ifdef USE_DLT
#ifndef ANDROID
    DLT_UNREGISTER_APP();
#endif
#endif
}

std::set<vsomeip_v3::eventgroup_t> service_info_t::get_eventgroups_for_event(const service_info_t& service_info,
                                                                             vsomeip_v3::event_t event_id) {
    std::set<vsomeip_v3::eventgroup_t> result;
    for (const auto& eventgroup : service_info.eventgroups) {
        if (std::find(eventgroup.event_ids.begin(), eventgroup.event_ids.end(), event_id) != eventgroup.event_ids.end()) {
            result.insert(eventgroup.group_id);
        }
    }
    return result;
}

// ----------------------------------------------------------------------
// Base VSIP App
// ----------------------------------------------------------------------

base_vsip_app::base_vsip_app(const char* app_name_, const char* app_id_) : base_logger(app_name_, app_id_) {
    greenlight_ready_ = false;
    _app = vsomeip::runtime::get()->create_application(app_name_);
    _run_thread = std::thread(std::bind(&base_vsip_app::run, this));
}

void base_vsip_app::run() {

    std::unique_lock<std::mutex> lock{greenlight_mutex_};
    greenlight_.wait(lock, [this] { return greenlight_ready_; });
    greenlight_ready_ = false;
    _app->init();
    if (this->_register_state_callback) {
        _app->register_state_handler(this->_register_state_callback.value());
    }
    _app->start();
}

void base_vsip_app::greenlight() {
    std::unique_lock<std::mutex> lock{greenlight_mutex_};
    greenlight_ready_ = true;
    greenlight_.notify_one();
}

void base_vsip_app::stop() {
    _app->clear_all_handler();
    _app->stop();
    _run_thread.join();
}

void base_vsip_app::only_stop() {
    _app->stop();
}

void base_vsip_app::join() {
    _run_thread.join();
}

void base_vsip_app::only_start() {
    _run_thread = std::thread([&]() { _app->start(); });
}

void base_vsip_app::start() {
    greenlight();
}

void base_vsip_app::send_request() {
    for (auto const& request : this->_requests) {
        send_request(request);
    }
}

void base_vsip_app::send_request(const service_info_t& request) {
    if (this->_register_availability_handler) {
        _app->register_availability_handler(request.service_id, request.instance_id, this->_register_availability_handler.value());
    }
    _app->request_service(request.service_id, request.instance_id);
}

void base_vsip_app::send_release() {
    for (auto& info : this->_requests) {
        send_release(info);
    }
}

void base_vsip_app::send_release(const service_info_t& request) {
    for (auto const& eventgroup : request.eventgroups) {
        for (auto const& event : eventgroup.event_ids) {
            _app->release_event(request.service_id, request.instance_id, event);
        }
    }
}

void base_vsip_app::send_offer() {
    for (auto& offer : this->_offers) {
        send_offer(offer);
    }
}

void base_vsip_app::send_offer(service_info_t& offer) {
    for (auto const& [event, event_params] : offer.events) {
        auto const eventgroup_set = service_info_t::get_eventgroups_for_event(offer, event);
        _app->offer_event(offer.service_id, offer.instance_id, event, eventgroup_set, event_params.type, event_params.cycle,
                          event_params.change_resets_cycle, event_params.update_on_change, event_params.epsilon_change_func,
                          event_params.reliability);
    }
    _app->offer_service(offer.service_id, offer.instance_id);
}

void base_vsip_app::set_offer(service_info_t& offer) {
    auto it = std::find_if(this->_offers.begin(), this->_offers.end(),
                           [&](const auto& item) { return item.service_id == offer.service_id && item.instance_id == offer.instance_id; });
    if (it != this->_offers.end()) {
        std::cout << "Reinserting service <" << std::hex << std::setfill('0') << offer.service_id << ":" << std::hex << std::setfill('0')
                  << offer.instance_id << "> onto application offers" << std::endl;
        *it = offer;
    } else {
        this->_offers.emplace_back(offer);
    }
}

void base_vsip_app::set_request(service_info_t& request) {
    auto it = std::find_if(this->_requests.begin(), this->_requests.end(), [&](const auto& item) {
        return item.service_id == request.service_id && item.instance_id == request.instance_id;
    });
    if (it != this->_requests.end()) {
        std::cout << "Reinserting service <" << std::hex << std::setfill('0') << request.service_id << ":" << std::hex << std::setfill('0')
                  << request.instance_id << "> onto application offers" << std::endl;
        *it = request;
    } else {
        this->_requests.emplace_back(request);
    }
}

void base_vsip_app::set_offer(service_info_list_t& offers) {
    this->_offers = offers;
}

void base_vsip_app::set_request(service_info_list_t& requests) {
    this->_requests = requests;
}

void base_vsip_app::send_stop_offer() {
    for (auto& info : this->_offers) {
        send_stop_offer(info);
    }
}

void base_vsip_app::send_stop_offer(const service_info_t& offer) {
    // OFFER_EVENT
    for (auto const& [event, event_params] : offer.events) {
        (void)event_params;
        _app->stop_offer_event(offer.service_id, offer.instance_id, event);
    }
    // OFFER_SERVICE
    _app->stop_offer_service(offer.service_id, offer.instance_id);
}

void base_vsip_app::set_state_callback(base_vsip_app::state_callback_fn& cb) {
    this->_register_state_callback = cb;
}

void base_vsip_app::set_availability_callback(base_vsip_app::availability_callback_fn& cb) {
    this->_register_availability_handler = cb;
}

// ----------------------------------------------------------------------
// BUILDER
// ----------------------------------------------------------------------

base_vsip_app_builder::base_vsip_app_builder(const char* name, const char* id) : app_name(name), app_id(id) { }

base_vsip_app_builder& base_vsip_app_builder::with_request(const service_info_t& info) {
    requests.push_back(info);
    return *this;
}

base_vsip_app_builder& base_vsip_app_builder::with_request(service_info_list_t list) {
    requests.insert(requests.end(), list.begin(), list.end());
    return *this;
}

base_vsip_app_builder& base_vsip_app_builder::with_offer(const service_info_t& info) {
    offers.push_back(info);
    return *this;
}

base_vsip_app_builder& base_vsip_app_builder::with_offer(service_info_list_t list) {
    offers.insert(offers.end(), list.begin(), list.end());
    return *this;
}

base_vsip_app_builder& base_vsip_app_builder::with_state_callback(const base_vsip_app::state_callback_fn& fn) {
    state_callback = fn;
    return *this;
}

base_vsip_app_builder& base_vsip_app_builder::with_availability_callback(const base_vsip_app::availability_callback_fn& fn) {
    availability_callback = fn;
    return *this;
}

std::unique_ptr<base_vsip_app> base_vsip_app_builder::build() {
    auto app = std::make_unique<base_vsip_app>(app_name.c_str(), app_id.c_str());

    if (!requests.empty()) {
        app->set_request(requests);
    }
    if (!offers.empty()) {
        app->set_offer(offers);
    }
    if (state_callback) {
        app->set_state_callback(*state_callback);
    }
    if (availability_callback) {
        app->set_availability_callback(*availability_callback);
    }

    if (auto_start_) {
        app->greenlight();
    }

    return app;
}
} // namespace common