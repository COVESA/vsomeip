//
// registry.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/sd/service_behavior.hpp>

#include "daemon.hpp"
#include "registry.hpp"

namespace vsomeip {
namespace sd {

registry::registry(daemon &_daemon)
	: daemon_(_daemon) {
}

void registry::on_provide_service(
		service_id _service, instance_id _instance) {
	auto find_service = administrated_.find(_service);
	if (find_service != administrated_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance == find_service->second.end()) {
			find_service->second[_instance].reset(new service_fsm::behavior(this));
		}
	}
}

void registry::on_withdraw_service(
		service_id _service, instance_id _instance) {
	auto find_service = administrated_.find(_service);
	if (find_service != administrated_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			find_service->second.erase(find_instance);
		}
	}
}

void registry::on_start_service(
		service_id _service, instance_id _instance) {
	auto find_service = administrated_.find(_service);
	if (find_service != administrated_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			find_instance->second->process_event(ev_service_status_change(false));
		}
	}
}

void registry::on_stop_service(
		service_id _service, instance_id _instance) {
	auto find_service = administrated_.find(_service);
	if (find_service != administrated_.end()) {
		auto find_instance = find_service->second.find(_instance);
		if (find_instance != find_service->second.end()) {
			find_instance->second->process_event(ev_service_status_change(false));
		}
	}
}

bool registry::send(const message_base *_message, bool _flush) {
	return daemon_.send(_message, _flush);
}

} // namespace sd
} // namespace vsomeip
