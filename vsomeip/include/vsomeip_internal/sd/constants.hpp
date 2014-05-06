//
// constants.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_CONSTANTS_HPP
#define VSOMEIP_INTERNAL_SD_CONSTANTS_HPP

#include <string>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

const client_id VSOMEIP_SERVICE_DISCOVERY_CLIENT = 0x0000;

const service_id VSOMEIP_SERVICE_DISCOVERY_SERVICE = 0xFFFF;
const instance_id VSOMEIP_SERVICE_DISCOVERY_INSTANCE = 0x0000;
const method_id  VSOMEIP_SERVICE_DISCOVERY_METHOD  = 0x8100;

const std::string VSOMEIP_SERVICE_DISCOVERY_DEFAULT_NETMASK = "255.255.255.0";

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_CONSTANTS_HPP
