// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_MESSAGE_IMPL_HPP
#define VSOMEIP_SD_MESSAGE_IMPL_HPP

#include <memory>
#include <vector>
#include <atomic>
#include <mutex>

#include <vsomeip/message.hpp>

#include "../include/primitive_types.hpp"
#include "../../message/include/message_base_impl.hpp"
#include "../../endpoints/include/endpoint_definition.hpp"

#  if _MSC_VER >= 1300
/*
* Diamond inheritance is used for the vsomeip::message_base base class.
* The Microsoft compiler put warning (C4250) using a desired c++ feature: "Delegating to a sister class"
* A powerful technique that arises from using virtual inheritance is to delegate a method from a class in another class
* by using a common abstract base class. This is also called cross delegation.
*/
#    pragma warning( disable : 4250 )
#  endif

namespace vsomeip {
namespace sd {

class entry_impl;
class eventgroupentry_impl;
class serviceentry_impl;

class option_impl;
class configuration_option_impl;
class ipv4_option_impl;
class ipv6_option_impl;
class load_balancing_option_impl;
class protection_option_impl;

class message_impl: public vsomeip::message, public vsomeip::message_base_impl {
public:
    typedef std::vector<std::shared_ptr<entry_impl>> entries_t;
    typedef std::vector<std::shared_ptr<option_impl>> options_t;
    struct forced_initial_events_t {
        std::shared_ptr<vsomeip::endpoint_definition> target_;
        vsomeip::service_t service_;
        vsomeip::instance_t instance_;
        vsomeip::eventgroup_t eventgroup_;
    };
    message_impl();
    virtual ~message_impl();

    length_t get_length() const;
    void set_length(length_t _length);

    bool get_reboot_flag() const;
    void set_reboot_flag(bool _is_set);

    bool get_unicast_flag() const;
    void set_unicast_flag(bool _is_set);

    std::shared_ptr<eventgroupentry_impl> create_eventgroup_entry();
    std::shared_ptr<serviceentry_impl> create_service_entry();

    std::shared_ptr<configuration_option_impl> create_configuration_option();
    std::shared_ptr<ipv4_option_impl> create_ipv4_option(bool _is_multicast);
    std::shared_ptr<ipv6_option_impl> create_ipv6_option(bool _is_multicast);
    std::shared_ptr<load_balancing_option_impl> create_load_balancing_option();
    std::shared_ptr<protection_option_impl> create_protection_option();

    const entries_t & get_entries() const;
    const options_t & get_options() const;

    int16_t get_option_index(const std::shared_ptr<option_impl> &_option) const;
    uint32_t get_options_length();

    std::shared_ptr<payload> get_payload() const;
    void set_payload(std::shared_ptr<payload> _payload);

    bool serialize(vsomeip::serializer *_to) const;
    bool deserialize(vsomeip::deserializer *_from);

    length_t get_someip_length() const;

    std::uint8_t get_number_required_acks() const;
    std::uint8_t get_number_contained_acks() const;
    void set_number_required_acks(std::uint8_t _required_acks);
    void increase_number_required_acks(std::uint8_t _amount = 1);
    void decrease_number_required_acks(std::uint8_t _amount = 1);
    void increase_number_contained_acks();
    bool all_required_acks_contained() const;
    std::unique_lock<std::mutex> get_message_lock();

    void forced_initial_events_add(forced_initial_events_t _entry);
    const std::vector<forced_initial_events_t> forced_initial_events_get();

    void set_initial_events_required(bool _initial_events);
    bool initial_events_required() const;

private:
    entry_impl * deserialize_entry(vsomeip::deserializer *_from);
    option_impl * deserialize_option(vsomeip::deserializer *_from);

private:
    flags_t flags_;
    uint32_t options_length_;

    entries_t entries_;
    options_t options_;
    std::atomic<std::uint8_t> number_required_acks_;
    std::atomic<std::uint8_t> number_contained_acks_;
    std::mutex message_mutex_;

    std::mutex forced_initial_events_mutex_;
    std::vector<forced_initial_events_t> forced_initial_events_info_;

    std::atomic<bool> initial_events_required_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_MESSAGE_IMPL_HPP
