// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_EVENTGROUPENTRY_IMPL_HPP
#define VSOMEIP_SD_EVENTGROUPENTRY_IMPL_HPP

#include "entry_impl.hpp"

namespace vsomeip {
namespace sd {

class eventgroupentry_impl: public entry_impl {
public:
    eventgroupentry_impl();
    eventgroupentry_impl(const eventgroupentry_impl &_entry);
    virtual ~eventgroupentry_impl();

    eventgroup_t get_eventgroup() const;
    void set_eventgroup(eventgroup_t _eventgroup);

    uint16_t get_reserved() const;
    void set_reserved(uint16_t _reserved);

    uint8_t get_counter() const;
    void set_counter(uint8_t _counter);

    bool serialize(vsomeip::serializer *_to) const;
    bool deserialize(vsomeip::deserializer *_from);

private:
    eventgroup_t eventgroup_;
    uint16_t reserved_;

    // counter field to differentiate parallel subscriptions on same event group
    // 4Bit only (max 16. parralel subscriptions)
    uint8_t counter_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_EVENTGROUPENTRY_IMPL_HPP
