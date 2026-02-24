// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_SOMEIP_MESSAGE_HPP_
#define VSOMEIP_V3_SOMEIP_MESSAGE_HPP_

#include "../../../../implementation/service_discovery/include/message_impl.hpp"
#include "service_state.hpp"

#include <vsomeip/vsomeip.hpp>
#include "to_string.hpp"

namespace vsomeip_v3::testing {

/**
 * Abstract wrapper around the payload that extends the internal commands.
 * This allows to abstract around the actual payload but allows for unified
 * printing.
 * Internally anything of type T could be stored for which a to_string(T) can
 * can be found.
 * If it is of interest do add e.g. an equality operator the same principle
 * pattern could be applied.
 *
 * (see "Runtime Polymorphism" from Sean Parent)
 **/
class someip_payload {
public:
    explicit someip_payload() = default;

    template<typename T>
    explicit someip_payload(T&& t) : data_(std::make_shared<derived<T>>(std::move(t))) { }

    friend std::string to_string(someip_payload const& _payload) { return _payload.data_->to_string_impl(); }

private:
    class base {
    public:
        virtual ~base() = default;
        virtual std::string to_string_impl() const = 0;
    };
    template<typename T>
    class derived : public base {
    public:
        explicit derived(T&& _t) : t_(std::move(_t)) { }

        virtual std::string to_string_impl() const override { return to_string(t_); }
        T t_;
    };
    std::shared_ptr<base const> data_;
};

struct someip_message {
    vsomeip_v3::service_t service_;
    vsomeip_v3::method_t method_;
    vsomeip_v3::client_t client_;
    bool is_sd_;
    someip_payload payload_;
};

[[nodiscard]] size_t parse(std::vector<unsigned char>& message, someip_message& _out_message);
[[nodiscard]] std::shared_ptr<vsomeip_v3::sd::message_impl> parse_sd(std::vector<unsigned char>& _message);
std::vector<unsigned char> construct_subscription(event_ids const& _subscription, boost::asio::ip::address _address, uint16_t _port);
std::vector<unsigned char> construct_offer(event_ids const& _offer, boost::asio::ip::address _address, uint16_t _port);

std::ostream& operator<<(std::ostream& _out, someip_message const& _m);
}

#endif
