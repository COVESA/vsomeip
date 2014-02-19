//
// daemon-config.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef __VSOMEIP_DAEMON_CONFIG_H__
#define __VSOMEIP_DAEMON_CONFIG_H__

#define VSOMEIP_KEY_PORT								 "--port"
#define VSOMEIP_VALUE_PORT_DEFAULT						 30490

#define VSOMEIP_KEY_REGISTRY_PATH						 "--registry-path"
#define VSOMEIP_VALUE_REGISTRY_PATH_DEFAULT				 "/tmp/vsomeipd"

#define VSOMEIP_KEY_VIRTUALMODE							 "--virtual"
#define VSOMEIP_VALUE_IS_VIRTUAL_MODE_DEFAULT			 false

#define VSOMEIP_KEY_SERVICEDISCOVERYMODE				 "--service_discovery"
#define VSOMEIP_VALUE_IS_SERVICE_DISCOVERY_MODE_DEFAULT false

#define VSOMEIP_KEY_LOGLEVEL							 "--loglevel"

#define VSOMEIP_MAX_CLIENTS								 25

#define VSOMEIP_DEFAULT_CONFIGURATION_FILE				 "/etc/vsomeip.conf"

#endif // __VSOMEIP_DAEMON_CONFIG_H__
