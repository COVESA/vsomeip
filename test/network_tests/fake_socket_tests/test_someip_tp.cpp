// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sample_configurations.hpp"

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/ecu_setup.hpp"
#include "helpers/message_checker.hpp"

#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdint>

namespace vsomeip_v3::testing {

struct test_someip_tp : public base_fake_socket_fixture {
    method_t method_{0x3333};
    std::vector<event_spec> const fields_specs_{{0x8002, {0x1}, vsomeip::reliability_type_e::RT_UNRELIABLE}};
    std::optional<someip_tp> tp_{someip_tp{{method_, 1392, 0}, {method_, 1392, 0}}};

    interface multi_field_service_{{0x3344, 0x1}, {}, fields_specs_, tp_};
    ecu_config ecu_one_config_extended_{boardnet::ecu_one_config};
    ecu_config ecu_two_config_extended_{boardnet::ecu_two_config};

    ecu_setup ecu_one_{"ecu_one", ecu_one_config_extended_.add_interface({multi_field_service_}), *socket_manager_};
    ecu_setup ecu_two_{"ecu_two", ecu_two_config_extended_.add_interface({multi_field_service_}), *socket_manager_};

    boost::asio::ip::udp::endpoint src_ep_ = boost::asio::ip::udp::endpoint(ecu_two_.config().unicast_ip_, 30491);
    boost::asio::ip::udp::endpoint dst_ep_ = boost::asio::ip::udp::endpoint(ecu_one_.config().unicast_ip_, 30501);

    void prepare_ecus_and_apps() {
        ecu_one_.prepare();
        ecu_two_.prepare();

        ecu_one_.start_apps();
        ecu_two_.start_apps();
    }
};

TEST_F(test_someip_tp, test_someip_tp_mem_corruption) {
    /// Ensure that fully overlapping segments are handled correctly and do not cause memory corruption.
    prepare_ecus_and_apps();

    auto* ecu_one = ecu_one_.router_;
    auto* ecu_two = ecu_two_.router_;

    ecu_one->offer(multi_field_service_);
    ecu_two->request_service(multi_field_service_.instance_);

    auto first_tp_message = construct_someip_tp_segment({.service_ = multi_field_service_.instance_.service_,
                                                         .method_ = method_,
                                                         .client_ = ecu_one->get_client(),
                                                         .session_ = 0x0001,
                                                         .offset_ = 0,
                                                         .more_segments_ = true, // not the last segment
                                                         .payload_ = std::vector<unsigned char>(32, 0xAA)});

    someip_tp_segment seconds_tp_frame{.service_ = multi_field_service_.instance_.service_,
                                       .method_ = method_,
                                       .client_ = ecu_one->get_client(),
                                       .session_ = 0x0001,
                                       .offset_ = 16,
                                       .more_segments_ = false, // last segment
                                       .payload_ = std::vector<unsigned char>(16, 0xBB)};
    auto second_tp_message = construct_someip_tp_segment(seconds_tp_frame);

    ASSERT_TRUE(ecu_two->availability_record_.wait_for_last(service_availability::available(multi_field_service_.instance_)));

    inject_message_udp(src_ep_, dst_ep_, first_tp_message);
    inject_message_udp(src_ep_, dst_ep_, second_tp_message);

    message_checker tp_checker{std::nullopt, multi_field_service_.instance_, method_, vsomeip::message_type_e::MT_REQUEST,
                               std::vector<unsigned char>(32, 0xAA)};
    ASSERT_TRUE(ecu_one->message_record_.wait_for_any(tp_checker));

    // Partial overlapping segments.
    seconds_tp_frame.payload_ = std::vector<unsigned char>(32, 0xBB);
    auto partial_tp_message = construct_someip_tp_segment(seconds_tp_frame);

    inject_message_udp(src_ep_, dst_ep_, first_tp_message);
    inject_message_udp(src_ep_, dst_ep_, partial_tp_message);

    tp_checker.payload_.value().insert(tp_checker.payload_.value().end(), 16, 0xBB);
    ASSERT_TRUE(ecu_one->message_record_.wait_for_any(tp_checker));
}
}
