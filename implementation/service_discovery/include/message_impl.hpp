// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_MESSAGE_IMPL_HPP
#define VSOMEIP_SD_MESSAGE_IMPL_HPP

#include <memory>
#include <vector>

#include <vsomeip/message.hpp>

#include "../include/primitive_types.hpp"
#include "../../message/include/message_base_impl.hpp"

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

    const std::vector<std::shared_ptr<entry_impl> > & get_entries() const;
    const std::vector<std::shared_ptr<option_impl> > & get_options() const;

    int16_t get_option_index(const std::shared_ptr<option_impl> &_option) const;

    std::shared_ptr<payload> get_payload() const;
    void set_payload(std::shared_ptr<payload> _payload);

    bool serialize(vsomeip::serializer *_to) const;
    bool deserialize(vsomeip::deserializer *_from);

private:
    entry_impl * deserialize_entry(vsomeip::deserializer *_from);
    option_impl * deserialize_option(vsomeip::deserializer *_from);

private:
    flags_t flags_;

    std::vector<std::shared_ptr<entry_impl> > entries_;
    std::vector<std::shared_ptr<option_impl> > options_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_MESSAGE_IMPL_HPP
