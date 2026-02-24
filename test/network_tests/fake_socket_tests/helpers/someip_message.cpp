// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "someip_message.hpp"
#include "test_logging.hpp"

#include <cstdint>
#include <ostream>
#include <utility>
#include <iomanip>
#include <sstream>
#include <cstring>

#include "../../../../interface/vsomeip/defines.hpp"
#include "../../../../implementation/message/include/deserializer.hpp"
#include "../../../../implementation/message/include/serializer.hpp"
#include "../../../../implementation/configuration/include/internal.hpp"
#include "../../../../implementation/message/include/message_impl.hpp"
#include "../../../../implementation/service_discovery/include/defines.hpp"
#include "../../../../implementation/service_discovery/include/deserializer.hpp"
#include "../../../../implementation/service_discovery/include/eventgroupentry_impl.hpp"
#include "../../../../implementation/service_discovery/include/ipv4_option_impl.hpp"
#include "../../../../implementation/service_discovery/include/serviceentry_impl.hpp"

namespace vsomeip_v3::testing {

[[nodiscard]] size_t parse(std::vector<unsigned char>& _message, someip_message& _out_message) {
    if (_message.size() < VSOMEIP_FULL_HEADER_SIZE) {
        TEST_LOG << "Message not long enough to contain someip header";
        return 0;
    }

    vsomeip_v3::deserializer des{&_message[0], _message.size(), VSOMEIP_DEFAULT_BUFFER_SHRINK_THRESHOLD};
    auto deserialized_message = des.deserialize_message();

    if (!deserialized_message) {
        TEST_LOG << "Failure during message deserialization";
        return 0;
    }

    _out_message.service_ = deserialized_message->get_service();
    _out_message.method_ = deserialized_message->get_method();
    _out_message.client_ = deserialized_message->get_client();
    _out_message.is_sd_ = (_out_message.service_ == VSOMEIP_SD_SERVICE && _out_message.method_ == VSOMEIP_SD_METHOD);

    std::vector<unsigned char> input_payload;
    auto payload = deserialized_message->get_payload();
    auto payload_it = payload->get_data();
    input_payload.reserve(payload->get_length());
    std::copy(payload_it, payload_it + payload->get_length(), std::back_inserter(input_payload));
    _out_message.payload_ = someip_payload(std::move(input_payload));

    return _message.size();
}

[[nodiscard]] std::shared_ptr<vsomeip_v3::sd::message_impl> parse_sd(std::vector<unsigned char>& _message) {
    if (_message.size() < VSOMEIP_FULL_HEADER_SIZE) {
        TEST_LOG << "Message not long enough to contain someip header";
        return 0;
    }

    vsomeip_v3::sd::deserializer des{&_message[0], _message.size(), VSOMEIP_DEFAULT_BUFFER_SHRINK_THRESHOLD};
    auto deserialized_message = des.deserialize_sd_message();
    if (!deserialized_message) {
        TEST_LOG << "Failure during message deserialization";
        return 0;
    }
    std::shared_ptr<vsomeip_v3::sd::message_impl> sd_message{deserialized_message};

    return sd_message;
}

std::vector<unsigned char> construct_subscription(event_ids const& _subscription, boost::asio::ip::address _address, uint16_t _port) {
    std::shared_ptr<vsomeip_v3::sd::entry_impl> entry_;
    std::vector<std::shared_ptr<vsomeip_v3::sd::option_impl>> options_;
    auto its_entry = std::make_shared<vsomeip_v3::sd::eventgroupentry_impl>();
    its_entry->set_type(vsomeip_v3::sd::entry_type_e::SUBSCRIBE_EVENTGROUP);
    its_entry->set_service(_subscription.si_.service_);
    its_entry->set_instance(_subscription.si_.instance_);
    its_entry->set_eventgroup(_subscription.eventgroup_id_);
    its_entry->set_counter(0);
    its_entry->set_major_version(0);
    its_entry->set_ttl(0x03);
    entry_ = its_entry;

    auto its_option = std::make_shared<vsomeip_v3::sd::ipv4_option_impl>(_address, _port, false);
    options_.push_back(its_option);

    auto its_current_message = std::make_shared<vsomeip_v3::sd::message_impl>();
    its_current_message->add_entry_data(entry_, options_);

    vsomeip_v3::serializer serializer{VSOMEIP_DEFAULT_BUFFER_SHRINK_THRESHOLD};
    serializer.serialize(its_current_message.get());
    return std::vector<unsigned char>(serializer.get_data(), serializer.get_data() + serializer.get_size());
}

std::vector<unsigned char> construct_offer(event_ids const& _offer, boost::asio::ip::address _address, uint16_t _port) {
    std::shared_ptr<vsomeip_v3::sd::entry_impl> entry_;
    std::vector<std::shared_ptr<vsomeip_v3::sd::option_impl>> options_;
    auto its_entry = std::make_shared<vsomeip_v3::sd::serviceentry_impl>();
    its_entry->set_type(vsomeip_v3::sd::entry_type_e::OFFER_SERVICE);

    its_entry->set_service(_offer.si_.service_);
    its_entry->set_instance(_offer.si_.instance_);
    its_entry->set_major_version(0);
    its_entry->set_minor_version(0);
    its_entry->set_ttl(0x03);
    entry_ = its_entry;

    auto its_option = std::make_shared<vsomeip_v3::sd::ipv4_option_impl>(_address, _port, false);
    options_.push_back(its_option);

    auto its_current_message = std::make_shared<vsomeip_v3::sd::message_impl>();
    its_current_message->add_entry_data(entry_, options_);

    vsomeip_v3::serializer serializer{VSOMEIP_DEFAULT_BUFFER_SHRINK_THRESHOLD};
    serializer.serialize(its_current_message.get());

    return std::vector<unsigned char>(serializer.get_data(), serializer.get_data() + serializer.get_size());
}

std::ostream& operator<<(std::ostream& _out, someip_message const& _m) {
    _out << "{service: " << _m.service_;
    _out << ", client_id: " << std::hex << std::setfill('0') << std::setw(4) << _m.client_;
    _out << ", payload: [" << to_string(_m.payload_) << "]}";
    return _out;
}
}
