// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

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

[[nodiscard]] size_t parse(std::vector<unsigned char> const& _message, command_message& _out_message);
[[nodiscard]] size_t parse(unsigned char const* _data, size_t _size, command_message& _out_message);

std::ostream& operator<<(std::ostream& _out, command_message const& _m);

/** Default type, not a vector. */
template<typename Type>
struct is_std_vector : std::false_type { };

/** vector type.*/
template<typename Type, typename Alloc>
struct is_std_vector<std::vector<Type, Alloc>> : std::true_type { };

/**
 * @brief Constructs basic vsomeip commands, example:
 * construct_basic_raw_command(protocol::id_e::ASSIGN_CLIENT_ID, static_cast<uint16_t>(0), client_one_id,
                                                   static_cast<uint32_t>(size of client name), "client_name");
 *
 * @param payload All fields needed to construct the command.
 * @return std::vector<unsigned char> Constructed raw message.
 */
template<typename... Ts>
std::vector<unsigned char> construct_basic_raw_command(Ts&&... payload) {
    std::vector<unsigned char> message;
    (..., ([&] {
         using U = std::decay_t<decltype(payload)>;
         if constexpr (std::is_same_v<U, std::string> || is_std_vector<U>::value) {
             message.insert(message.end(), payload.begin(), payload.end());
         } else if constexpr (std::is_convertible_v<U, const char*>) {
             std::string s(payload);
             message.insert(message.end(), s.begin(), s.end());
         } else {
             message.insert(message.end(), reinterpret_cast<unsigned char*>(&payload),
                            reinterpret_cast<unsigned char*>(&payload) + sizeof(payload));
         }
     }()));

    return message;
}
}
