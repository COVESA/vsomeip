/*
 * service_administrator_client_behavior.hpp
 *
 *  Created on: Feb 27, 2014
 *      Author: someip
 */

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_BEHAVIOR_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_BEHAVIOR_HPP

namespace vsomeip {
namespace service_discovery {
namespace client {

struct initial;
struct client_behavior
		: sc::state_machine< service_administrator_behavior, initial >,
		  public timer_service

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_ADMINISTRATOR_CLIENT_BEHAVIOR_HPP
