// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <gmock/gmock.h>

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <tuple>

#include <vsomeip/primitive_types.hpp>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif

#include "mock_routing_manager_host.hpp"
#include "../../../../implementation/routing/include/routing_manager_base.hpp"

namespace vsomeip_v3 {

class mock_rmb final : public routing_manager_base {
public:
    explicit mock_rmb(mock_routing_manager_host* host) : routing_manager_base(static_cast<routing_manager_host*>(host)) {
        using ::testing::_;
        using ::testing::Invoke;

        ON_CALL(*this, create_placeholder_event_and_subscribe(_, _, _, _, _, _))
                .WillByDefault(Invoke(this, &mock_rmb::create_placeholder_event_and_subscribe_default));
    }

    ~mock_rmb() override = default;

    using vsomeip_v3::routing_manager_base::insert_subscription;

    MOCK_METHOD(std::string, get_env, (client_t _client), (const, override));
    MOCK_METHOD(void, init, (), (override));

    MOCK_METHOD(void, subscribe,
                (client_t _client, const vsomeip_sec_client_t* _sec_client, service_t _service, instance_t _instance,
                 eventgroup_t _eventgroup, major_version_t _major, event_t _event, const std::shared_ptr<debounce_filter_impl_t>& _filter),
                (override));

    MOCK_METHOD(bool, send,
                (client_t _client, const byte_t* _data, uint32_t _size, instance_t _instance, bool _reliable, client_t _bound_client,
                 const vsomeip_sec_client_t* _sec_client, uint8_t _status_check, bool _sent_from_remote, bool _force),
                (override));

    MOCK_METHOD(void, on_message, (const byte_t*, length_t, client_t, const vsomeip_sec_client_t*), (override));

    MOCK_METHOD(void, register_client_error_handler, (client_t _client, const std::shared_ptr<local_endpoint>& _endpoint), (override));

    MOCK_METHOD(void, send_get_offered_services_info, (client_t _client, offer_type_e _offer_type), (override));

    MOCK_METHOD(void, on_register_application, (client_t _client, const boost::asio::ip::address& _address, port_t _port), (override));

    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, stop, (), (override));

    MOCK_METHOD(bool, send_to, (client_t _client, const std::shared_ptr<endpoint_definition>& _target, std::shared_ptr<message> _message),
                (override));

    MOCK_METHOD(bool, send_to,
                (const std::shared_ptr<endpoint_definition>& _target, const byte_t* _data, uint32_t _size, instance_t _instance),
                (override));
    MOCK_METHOD(bool, is_local_client, (client_t _client), (const, override));

    MOCK_METHOD(void, send_subscribe,
                (client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup, major_version_t _major,
                 event_t _event, const std::shared_ptr<debounce_filter_impl_t>& _filter),
                (override));

    MOCK_METHOD(bool, create_placeholder_event_and_subscribe,
                (service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                 const std::shared_ptr<debounce_filter_impl_t>& _filter, client_t _client),
                (override));

private:
    // Defines a "Default" method to be called as create_placeholder_event_and_subscribe() is a pure virtual and is extremely necessary to
    // test rmb_fixture.double_insert_subscription
    bool create_placeholder_event_and_subscribe_default(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                                                        const std::shared_ptr<debounce_filter_impl_t>& _filter, client_t _client) {

        bool is_inserted(false);
        std::set<eventgroup_t> its_eventgroups({_eventgroup});

        routing_manager_base::register_event(_client, _service, _instance, _event, its_eventgroups, event_type_e::ET_UNKNOWN,
                                             reliability_type_e::RT_UNKNOWN, std::chrono::milliseconds::zero(), false, true, nullptr, false,
                                             false, true);

        std::shared_ptr<event> its_event = find_event(_service, _instance, _event);
        if (its_event) {
            is_inserted = its_event->add_subscriber(_eventgroup, _filter, _client, false);
        }

        return is_inserted;
    }
};

} // namespace vsomeip_v3
