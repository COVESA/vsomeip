// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_TYPES_HPP
#define VSOMEIP_ROUTING_TYPES_HPP

#include <map>
#include <memory>
#include <boost/asio/ip/address.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/constants.hpp>

#include "../../service_discovery/include/message_impl.hpp"
#include "../../configuration/include/internal.hpp"

namespace vsomeip {

class serviceinfo;
class endpoint_definition;


typedef std::map<service_t,
                 std::map<instance_t,
                          std::shared_ptr<serviceinfo> > > services_t;

class eventgroupinfo;

typedef std::map<service_t,
                 std::map<instance_t,
                          std::map<eventgroup_t,
                                   std::shared_ptr<
                                       eventgroupinfo> > > > eventgroups_t;

enum class registration_type_e : std::uint8_t {
    REGISTER = 0x1,
    DEREGISTER = 0x2,
    DEREGISTER_ON_ERROR = 0x3
};

struct sd_message_identifier_t {
    sd_message_identifier_t(session_t _session,
                            boost::asio::ip::address _sender,
                            boost::asio::ip::address _destination,
                            const std::shared_ptr<sd::message_impl> &_response) :
            session_(_session),
            sender_(_sender),
            destination_(_destination),
            response_(_response) {
    }

    sd_message_identifier_t() :
        session_(0),
        sender_(boost::asio::ip::address()),
        destination_(boost::asio::ip::address()),
        response_(std::shared_ptr<sd::message_impl>()) {
    }

    bool operator==(const sd_message_identifier_t &_other) const {
        return !(session_ != _other.session_ ||
                sender_ != _other.sender_ ||
                destination_ != _other.destination_ ||
                response_ != _other.response_);
    }

    bool operator<(const sd_message_identifier_t &_other) const {
        return (session_ < _other.session_
                || (session_ == _other.session_ && sender_ < _other.sender_)
                || (session_ == _other.session_ && sender_ == _other.sender_
                        && destination_ < _other.destination_)
                || (session_ == _other.session_ && sender_ == _other.sender_
                        && destination_ == _other.destination_
                        && response_ < _other.response_));
    }

    session_t session_;
    boost::asio::ip::address sender_;
    boost::asio::ip::address destination_;
    std::shared_ptr<sd::message_impl> response_;
};

struct pending_subscription_t {
    pending_subscription_t(
            std::shared_ptr<sd_message_identifier_t> _sd_message_identifier,
            std::shared_ptr<endpoint_definition> _subscriber,
            std::shared_ptr<endpoint_definition> _target,
            ttl_t _ttl,
            client_t _subscribing_client) :
            sd_message_identifier_(_sd_message_identifier),
            subscriber_(_subscriber),
            target_(_target),
            ttl_(_ttl),
            subscribing_client_(_subscribing_client),
            pending_subscription_id_(DEFAULT_SUBSCRIPTION) {
    }
    pending_subscription_t () :
        sd_message_identifier_(std::shared_ptr<sd_message_identifier_t>()),
        subscriber_(std::shared_ptr<endpoint_definition>()),
        target_(std::shared_ptr<endpoint_definition>()),
        ttl_(0),
        subscribing_client_(VSOMEIP_ROUTING_CLIENT),
        pending_subscription_id_(DEFAULT_SUBSCRIPTION) {
    }
    std::shared_ptr<sd_message_identifier_t> sd_message_identifier_;
    std::shared_ptr<endpoint_definition> subscriber_;
    std::shared_ptr<endpoint_definition> target_;
    ttl_t ttl_;
    client_t subscribing_client_;
    pending_subscription_id_t pending_subscription_id_;
};

enum remote_subscription_state_e : std::uint8_t {
    SUBSCRIPTION_ACKED,
    SUBSCRIPTION_NACKED,
    SUBSCRIPTION_PENDING,
    SUBSCRIPTION_ERROR
};

}
// namespace vsomeip

#endif // VSOMEIP_ROUTING_TYPES_HPP
