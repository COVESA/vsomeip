// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_COMMAND_MESSAGE_HPP_
#define VSOMEIP_V3_COMMAND_MESSAGE_HPP_

#include <vsomeip/vsomeip.hpp>
#include "../../../../implementation/protocol/include/protocol.hpp"
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
class command_payload {
public:
    explicit command_payload() = default;

    template<typename T>
    explicit command_payload(T&& t) : data_(std::make_shared<derived<T>>(std::move(t))) { }

    friend std::string to_string(command_payload const& _payload) { return _payload.data_->to_string_impl(); }

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

struct command_message {
    vsomeip_v3::protocol::id_e id_{0};
    uint16_t client_id_{0};
    command_payload payload_;
    // the hard coded version is of little interest
};

[[nodiscard]] bool parse(std::vector<unsigned char> const& _message, command_message& _out);

std::ostream& operator<<(std::ostream& _out, command_message const& _m);

}

#endif
