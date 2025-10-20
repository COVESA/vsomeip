// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "app.hpp"

namespace vsomeip_v3::testing {
app::app(std::string const& _name) : app_(vsomeip::runtime::get()->create_application(_name)) { }
app::~app() {
    stop();
}

[[nodiscard]] bool app::start() {
    if (is_running_) {
        return true;
    }
    is_running_ = true;
    bool const result = app_->init();
    if (!result) {
        return false;
    }
    app_->register_state_handler(std::bind(&app::on_state, this, std::placeholders::_1));
    app_->register_message_handler(vsomeip::ANY_SERVICE, vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD,
                                   std::bind(&app::on_message, this, std::placeholders::_1));
    app_->register_availability_handler(
            vsomeip::ANY_SERVICE, vsomeip::ANY_INSTANCE,
            std::bind(&app::on_availability, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    runner_ = std::thread([this] { app_->start(); });
    return true;
}

void app::stop() {
    if (!is_running_) {
        return;
    }
    is_running_ = false;
    app_->clear_all_handler();
    app_->stop();
    if (runner_.joinable()) {
        runner_.join();
    }
    app_ = nullptr;
}

void app::offer(service_instance _si) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is offering: " << _si;
    app_->offer_service(_si.service_, _si.instance_);
}

void app::offer_event(event_ids const& _ei) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is offering: " << _ei;
    offer(_ei, vsomeip::event_type_e::ET_EVENT);
}

void app::offer_field(event_ids const& _ei) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is offering: " << _ei;
    offer(_ei, vsomeip::event_type_e::ET_FIELD);
}

void app::subscribe_event(event_ids const& _ei) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is subscribing to: " << _ei;
    subscribe(_ei, vsomeip::event_type_e::ET_EVENT);
}

void app::subscribe_field(event_ids const& _ei) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is subscribing to: " << _ei;
    subscribe(_ei, vsomeip::event_type_e::ET_FIELD);
}

void app::answer_request(request const& r, std::function<std::vector<unsigned char>()> payload_creator) {
    app_->register_message_handler(r.service_instance_.service_, r.service_instance_.instance_, r.method_,
                                   [this, payload_creator](auto const& message) {
                                       // capture the message and log the reception
                                       on_message(message);
                                       auto response = vsomeip::runtime::get()->create_response(message);
                                       auto payload = vsomeip::runtime::get()->create_payload();
                                       payload->set_data(payload_creator());
                                       response->set_payload(payload);
                                       app_->send(response);
                                   });
}

void app::request_service(service_instance _si) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is requesting: " << _si;
    app_->request_service(_si.service_, _si.instance_);
}

void app::send_event(event_ids const& _ei, std::vector<unsigned char> const& _payload) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is sending: " << _ei;
    auto payload = vsomeip::runtime::get()->create_payload();
    payload->set_data(_payload);
    app_->notify(_ei.si_.service_, _ei.si_.instance_, _ei.event_id_, payload, false);
}

void app::send_request(request const& _req) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is requesting: " << _req;
    auto message = vsomeip::runtime::get()->create_request(true);
    message->set_service(_req.service_instance_.service_);
    message->set_instance(_req.service_instance_.instance_);
    message->set_method(_req.method_);
    message->set_message_type(_req.message_type_);
    auto its_payload = vsomeip::runtime::get()->create_payload();
    its_payload->set_data(_req.payload_);
    message->set_payload(its_payload);
    app_->send(message);
}

void app::stop_offer(service_instance const& _si) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is no longer offering: " << _si;
    app_->stop_offer_service(_si.service_, _si.instance_);
}

void app::on_state(vsomeip::state_type_e _state) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is "
             << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.");
    app_state_record_.record(_state);
}

void app::on_message(const std::shared_ptr<vsomeip::message>& _message) {
    std::vector<unsigned char> payload;
    payload.reserve(_message->get_payload()->get_length());
    for (unsigned char const* ptr = _message->get_payload()->get_data();
         ptr < _message->get_payload()->get_data() + _message->get_payload()->get_length(); ++ptr) {
        payload.push_back(*ptr);
    }
    auto m = message{client_session{_message->get_client(), _message->get_session()},
                     service_instance{_message->get_service(), _message->get_instance()}, _message->get_method(),
                     _message->get_message_type(), std::move(payload)};
    TEST_LOG << "[app] \"" << app_->get_name() << "\" received: " << m;
    message_record_.record(m);
}

void app::on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, vsomeip::availability_state_e _state) {
    auto const avail = service_availability{_service, _instance, _state};
    TEST_LOG << "[app] \"" << app_->get_name() << "\" availability changed: " << avail;
    availability_record_.record(avail);
}

void app::on_subscription_status_changed(vsomeip::service_t _service, vsomeip::instance_t _instance, vsomeip::eventgroup_t _eventgroup,
                                         vsomeip::event_t _event, uint16_t error_code) {
    auto const event_sub = event_subscription{{{_service, _instance}, _event, _eventgroup}, error_code};
    TEST_LOG << "[app] \"" << app_->get_name() << "\" subscription status changed: " << event_sub;
    subscription_record_.record(event_sub);
}

void app::subscribe(event_ids const& _ei, vsomeip::event_type_e _et) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is requesting: " << _ei;
    std::set<vsomeip::eventgroup_t> its_eventgroups;
    app_->register_subscription_status_handler(_ei.si_.service_, _ei.si_.instance_, _ei.eventgroup_id_, _ei.event_id_,
                                               std::bind(&app::on_subscription_status_changed, this, std::placeholders::_1,
                                                         std::placeholders::_2, std::placeholders::_3, std::placeholders::_4,
                                                         std::placeholders::_5));

    its_eventgroups.insert(_ei.eventgroup_id_);
    app_->request_event(_ei.si_.service_, _ei.si_.instance_, _ei.event_id_, its_eventgroups, _et, vsomeip::reliability_type_e::RT_RELIABLE);
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is subscribing to: " << _ei;

    app_->subscribe(_ei.si_.service_, _ei.si_.instance_, _ei.eventgroup_id_, 0, _ei.event_id_);
}

void app::offer(event_ids const& _ei, vsomeip::event_type_e _et) {
    TEST_LOG << "[app] \"" << app_->get_name() << "\" is offering: " << _ei;
    std::set<vsomeip::eventgroup_t> its_eventgroups;
    its_eventgroups.insert(_ei.eventgroup_id_);
    app_->offer_event(_ei.si_.service_, _ei.si_.instance_, _ei.event_id_, its_eventgroups, _et, std::chrono::milliseconds::zero(), false,
                      true, nullptr, vsomeip::reliability_type_e::RT_RELIABLE);
}
}
