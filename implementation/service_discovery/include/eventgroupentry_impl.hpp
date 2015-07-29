// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

    bool serialize(vsomeip::serializer *_to) const;
    bool deserialize(vsomeip::deserializer *_from);

private:
    eventgroup_t eventgroup_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_EVENTGROUPENTRY_IMPL_HPP
