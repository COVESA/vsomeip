// Copyright(C) 2015 - 2025 Bayerische Motoren Werke Aktiengesellschaft(BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v.2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http: // mozilla.org/MPL/2.0/.
#include "registration_check_service.hpp"

service::service() :
    vsomeip_app(vsomeip::runtime::get()->create_application()), registration_state(vsomeip::state_type_e::ST_DEREGISTERED) { }

service::~service() {
    stop();
}

bool service::init() {
    std::scoped_lock its_lock(service_mutex);
    if (!vsomeip_app->init()) {
        VSOMEIP_ERROR << "Couldn't initialize application";
        return false;
    }

    vsomeip_app->register_state_handler(std::bind(&service::on_state, this, std::placeholders::_1));
    return true;
}

void service::start() {
    app_thread = std::thread([&] { vsomeip_app->start(); });
}

void service::stop() {
    vsomeip_app->stop();
    if (app_thread.joinable()) {
        app_thread.join();
    }
    vsomeip_app->clear_all_handler();
}

void service::on_state(vsomeip::state_type_e _state) {
    std::unique_lock<std::mutex> its_lock(service_mutex);

    auto prev_state = registration_state.load();
    registration_state = _state;

    VSOMEIP_INFO << __func__ << " Application " << vsomeip_app->get_name() << " changed state from "
                 << (prev_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.") << " to "
                 << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.");
}

bool service::is_registered() {
    std::unique_lock<std::mutex> its_lock(service_mutex);
    return registration_state == vsomeip::state_type_e::ST_REGISTERED;
}
