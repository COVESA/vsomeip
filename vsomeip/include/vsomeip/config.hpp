//
// config.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_CONFIG_HPP
#define VSOMEIP_CONFIG_HPP

/// \def VSOMEIP_DEFAULT_CONFIGURATION_FILE
/// The default location of the vsomeip configuration file
#define VSOMEIP_DEFAULT_CONFIGURATION_FILE	"/etc/vsomeip.conf"

/// \def VSOMEIP_MAX_UDP_MESSAGE_SIZE
/// The maximum message size for using the UDP transport protocol.
#define VSOMEIP_MAX_UDP_MESSAGE_SIZE (1416 - 16)

/// \def VSOMEIP_MAX_TCP_MESSAGE_SIZE
/// The maximum message size for using the TCP transport protocol.
#define VSOMEIP_MAX_TCP_MESSAGE_SIZE (4095 - 16)

/// \def VSOMEIP_LOWEST_VALID_PORT
/// The lowest port address that can be configured for Some/IP services.
#define VSOMEIP_LOWEST_VALID_PORT 49152

/// \def VSOMEIP_HIGHEST_VALID_PORT
/// The highest port address that can be configured for Some/IP services.
#define VSOMEIP_HIGHEST_VALID_PORT	 65535

///
/// Timeout in milliseconds that is used to automatically send messages
/// which are sent using the packetizer functionality.
#define VSOMEIP_FLUSH_TIMEOUT 10000

#endif // VSOMEIP_CONFIG_HPP
