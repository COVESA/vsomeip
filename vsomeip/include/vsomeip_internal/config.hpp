//
// config.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2024 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_CONFIG_HPP
#define VSOMEIP_INTERNAL_CONFIG_HPP

#define VSOMEIP_DEFAULT_CONFIGURATION_FILE_PATH	 \
			"/etc/vsomeip.conf"

#define VSOMEIP_PROTOCOL_OVERHEAD		  17

#define VSOMEIP_MAX_TCP_MESSAGE_SIZE	4095
#define VSOMEIP_MAX_UDP_MESSAGE_SIZE	1446

#define VSOMEIP_FLUSH_TIMEOUT			1000

#define VSOMEIP_WATCHDOG_CYCLE			2000
#define VSOMEIP_WATCHDOG_TIMEOUT		 200
#define VSOMEIP_MAX_MISSING_PONGS		   5

#endif // VSOMEIP_INTERNAL_CONFIG_HPP
