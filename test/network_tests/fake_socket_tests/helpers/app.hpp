// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>
#include "../../../implementation/security/include/policy.hpp"
#include "service_state.hpp"
#include "test_logging.hpp"
#include "attribute_recorder.hpp"

#include <thread>

namespace vsomeip_v3::testing {

/**
 * This class is a small wrapper around a vsomeip::application to ease the writing
 * of tests with some helpers.
 * The public methods are not thread safe.
 * If one would like to use this wrapper for testing the communication between
 * multiple vsomeip applications, it is recommended to use the
 * base_fake_socket_fixture for instantiation + setup/teardown orchestration.
 */
class app {
public:
    /**
     * Create a vsomeip application with the passed in name
     */
    app(std::string const& _name);

    /**
     * Invokes app::stop
     */
    ~app();

    /**
     * 1. Invokes vsomeip::init of the created application,
     * 2. registers a state handler to record vsomeip::state_type_e of the application
     * 3. registers a message handler for any message to be recorded in @see message_record_
     * 4. spawns a background thread that will call vsomeip::application::start()
     */
    [[nodiscard]] bool start();

    /**
     * Get underlying vsomeip::application
     */
    std::shared_ptr<vsomeip::application> get_application() const { return app_; }

    /**
     * Get the client ID of this application
     */
    vsomeip::client_t get_client_id() const { return app_->get_client(); }

    /**
     * if running has been called:
     * 1. calls vsomeip::clear_all_handlers()
     * 2. calls vsomeip::stop()
     * 3. awaits the background thread to join (the one that called vsomeip::application::start())
     */
    void stop();

    /**
     * Get the vsomeip app client id
     */
    vsomeip::client_t get_client();

    /**
     * Whether application is a router, e.g., is routingmanagerd
     */
    bool is_router() const;

    /**
     * Forwards the request to offer the contained service, instance, event and field.
     **/
    void offer(interface const& _interface);

    /**
     * Forwards the request to the vsomeip::application::offer_service()
     */
    void offer(service_instance _si);

    /**
     * Forwards the request to the vsomeip::application::offer()
     */
    void offer_event(event_ids const& _ei);

    /**
     * Forwards the request to the vsomeip::application::offer()
     */
    void offer_field(event_ids const& _ei);

    /**
     * Forwards the request to the vsomeip::application::stop_offer_service()
     */
    void stop_offer(service_instance const& _si);

    /**
     * Forwards the request to the vsomeip::application::request_service
     */
    void request_service(service_instance _si);

    /**
     * Forwards the request to the vsomeip::application::release_service
     */
    void release_service(service_instance _si);

    /**
     * Forwards the request to the vsomeip::application::send()
     */
    void send_request(request const& _req);

    /**
     * Forwards the request for the service and instance and subscribes to all events and fields that are part of the interface
     **/
    void subscribe(interface const& _interface);

    /**
     * Forwards the request to the vsomeip::application::update_security_policy_configuration()
     */
    void update_security_policy_configuration(uid_t _uid, gid_t _gid);

    /**
     * 1. registers a subscription handler to record subscription events with @see
     * subscription_record_.
     * 2. calls vsomeip::application::request_event()
     * 3. calls vsomeip::application::subscribe()
     *
     * Note: Received events are recorded in the @see message_record_.
     */
    void subscribe_event(event_ids const& _ei);

    /**
     * 1. registers a subscription handler to record subscription events with @see
     * subscription_record_.
     * 2. calls vsomeip::application::request_event()
     * 3. calls vsomeip::application::subscribe()
     *
     * Note: Received events are recorded in the @see message_record_.
     */
    void subscribe_field(event_ids const& _ei);

    /**
     * 1. registers a subscription handler to record subscription events with @see
     * subscription_record_.
     * 2. calls vsomeip::application::subscribe() with specified event_type_e
     *
     * Note: Received events are recorded in the @see message_record_.
     */
    void subscribe_selective(event_ids const& _ei);

    /**
     * 1. registers a subscription handler to record subscription events with @see
     * subscription_record_.
     * 2. calls vsomeip::application::request_event()
     * 3. calls vsomeip::application::subscribe(), with only the event group
     *
     * Note: Received events are recorded in the @see message_record_.
     */
    void subscribe_eventgroup_event(event_ids const& _ei);

    /**
     * 1. registers a subscription handler to record subscription events with @see
     * subscription_record_.
     * 2. calls vsomeip::application::request_event()
     * 3. calls vsomeip::application::subscribe(), with only the event group
     *
     * Note: Received events are recorded in the @see message_record_.
     */
    void subscribe_eventgroup_field(event_ids const& _ei);

    /**
     * @see subscribe_event
     *
     * Uses `subscribe_with_debounce`
     */
    void subscribe_event_debounce(event_ids const& _ei, debounce_filter_t const& _filter);

    /*
     * @see subscribe_field
     *
     * Uses `subscribe_with_debounce`
     */
    void subscribe_field_debounce(event_ids const& _ei, debounce_filter_t const& _filter);

    /**
     * registers a message handler that will
     * 1. record the incoming request in the
     * @see message_record_
     * 2. uses the _payload_creator to fill the response
     * 3. dispatch the response with vsomeip::application::send()
     */
    void answer_request(request const& _r, std::function<std::vector<unsigned char>()> _payload_creator);

    /**
     * Forwards the event payload to the vsomeip::application::notify()
     */
    void send_event(event_ids const& _ei, std::vector<unsigned char> const& _payload);

    /**
     * Wait for message (payloads!) to reach given state
     *
     * It is essentially an advanced wrapper around `message_record_`, which only looks at payloads (and ignores service/method/session/...)
     */
    bool wait_for_messages(std::vector<std::vector<uint8_t>> const& _payloads,
                           std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        return message_record_.wait_for(
                [&_payloads](auto const& _record) {
                    if (_record.size() != _payloads.size()) {
                        return false;
                    }

                    bool equal = true;
                    for (size_t i = 0; i < _record.size(); ++i) {
                        equal = equal && _record[i].payload_ == _payloads[i];
                    }
                    return equal;
                },
                timeout);
    }

    /**
     * Sets the routing state of the application
     */
    void set_routing_state(vsomeip::routing_state_e _state);

    /**
     * After app::start() has been called this helper can be used to await the registration
     * of the application
     **/
    attribute_recorder<vsomeip::state_type_e> app_state_record_;

    /**
     * This helper can be used to await any sort of message either from the client, or the service.
     * But not from the vsomeipd
     **/
    attribute_recorder<testing::message> message_record_;

    /**
     * This helper can be used to await availability changes of services
     **/
    attribute_recorder<testing::service_availability> availability_record_;

    /**
     * This helper can be used to await the subscription of any event.
     **/
    attribute_recorder<testing::event_subscription> subscription_record_;

private:
    void on_state(vsomeip::state_type_e _state);
    void on_message(const std::shared_ptr<vsomeip::message>& _message);
    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, vsomeip::availability_state_e _state);
    void on_subscription_status_changed(vsomeip::service_t _service, vsomeip::instance_t _instance, vsomeip::eventgroup_t _eventgroup,
                                        vsomeip::event_t _event, uint16_t error_code);
    void subscribe(event_ids const& _ei, vsomeip::event_type_e _et);
    void subscribe_eventgroup(event_ids const& _ei, vsomeip::event_type_e _et);
    void subscribe_with_debounce(event_ids const& _ei, vsomeip::event_type_e _et, debounce_filter_t const& _filter);
    void offer(event_ids const& _ei, vsomeip::event_type_e _et);

    bool is_running_{false};
    std::shared_ptr<vsomeip::application> app_;
    std::thread runner_;
};
}
