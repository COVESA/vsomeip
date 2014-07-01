// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_EVENT_HPP
#define VSOMEIP_EVENT_HPP

#include <chrono>
#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class payload;

class event {
public:
	virtual ~event() {};

	virtual service_t get_service() const = 0;
	virtual void set_service(service_t _service) = 0;

	virtual instance_t get_instance() const = 0;
	virtual void set_instance(instance_t _instance) = 0;

	virtual event_t get_event() const = 0;
	virtual void set_event(event_t _event) = 0;

	virtual std::shared_ptr< payload > get_payload() const = 0;
	virtual void set_payload(std::shared_ptr< payload > _payload) = 0;

	virtual void set_update_on_change(bool _is_on) = 0;
	virtual void set_update_cycle(std::chrono::milliseconds &_cycle) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_EVENT_HPP

