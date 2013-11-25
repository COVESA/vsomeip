//
// daemon-config.h
//
// Date: 	Oct 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_DAEMON_CONFIG_H__
#define __VSOMEIP_DAEMON_CONFIG_H__

#define VSOMEIP_KEY_PORT							"-port"
#define VSOMEIP_KEY_PORT_DEFAULT					"55555"

#define VSOMEIP_KEY_VIRTUALMODE						"--virtual"
#define VSOMEIP_KEY_VIRTUALMODE_DEFAULT				"false"

#define VSOMEIP_KEY_SERVICEDISCOVERYMODE			"--service_discovery"
#define VSOMEIP_KEY_SERVICEDISCOVERYMODE_DEFAULT	"false"

#define VSOMEIP_KEY_LOGLEVEL						"--loglevel"
#define VSOMEIP_KEY_LOGLEVEL_DEFAULT				"INFO"

#define VSOMEIP_MAX_CLIENTS							25

#define VSOMEIP_DEFAULT_CONFIGURATION_FILE			"/etc/vsomeip.conf"

#endif // __VSOMEIP_DAEMON_CONFIG_H__
