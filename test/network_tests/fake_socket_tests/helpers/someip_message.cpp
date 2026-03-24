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
#include "../../../../implementation/utility/include/utility.hpp"

namespace vsomeip_v3::testing {

[[nodiscard]] size_t parse(std::vector<unsigned char>& _message, someip_message& _out_message) {
    if (_message.size() < VSOMEIP_FULL_HEADER_SIZE) {
        TEST_LOG << "Message not long enough to contain someip header";
        return 0;
    }

    vsomeip_v3::deserializer des{&_message[0], _message.size(), VSOMEIP_DEFAULT_BUFFER_SHRINK_THRESHOLD};
    auto msg = des.deserialize_message();

    if (!msg) {
        TEST_LOG << "Failure during message deserialization";
        return 0;
    }
    if (msg->get_service() == VSOMEIP_SD_SERVICE && msg->get_method() == VSOMEIP_SD_METHOD) {
        auto sd_msg = parse_sd(_message);
        if (sd_msg) {
            _out_message.sd_ = sd_msg;
            return _message.size();
        }
    }
    _out_message.msg_ = msg;

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
    if (_m.msg_) {
        _out << "someip-message: { type: " << to_string(_m.msg_->get_message_type()) << ", service: " << hex4(_m.msg_->get_service())
             << ", instance: " << hex4(_m.msg_->get_instance()) << ", method: " << hex4(_m.msg_->get_method())
             << ", client: " << hex4(_m.msg_->get_client()) << ", session: " << hex4(_m.msg_->get_session())
             << ", return_code: " << to_string(_m.msg_->get_return_code()) << ", is_reliable: " << std::boolalpha << _m.msg_->is_reliable()
             << ", is_initial: " << std::boolalpha << _m.msg_->is_initial() << ", payload: " << to_string(*(_m.msg_->get_payload())) << "}";
    } else if (_m.sd_) {

        _out << "sd-message: { entries: " << to_string(_m.sd_->get_entries()) << "}";

    } else {
        _out << "ERROR, no valid message";
    }
    return _out;
}
}
