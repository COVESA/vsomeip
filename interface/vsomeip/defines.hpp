// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_DEFINES_HPP
#define VSOMEIP_DEFINES_HPP

#define VSOMEIP_DEFAULT_CONFIGURATION_FILE_PATH		"/etc/vsomeip.xml"
#define VSOMEIP_ENV_CONFIGURATION_FILE_PATH			"VSOMEIP_CONFIGURATION_FILE"
#define VSOMEIP_LOCAL_CONFIGURATION_FILE_PATH		"./vsomeip.xml"

#define VSOMEIP_PROTOCOL_VERSION			0x1


#define VSOMEIP_MAX_LOCAL_MESSAGE_SIZE		32768
#define VSOMEIP_MAX_TCP_MESSAGE_SIZE		4095
#define VSOMEIP_MAX_UDP_MESSAGE_SIZE		1446

#define VSOMEIP_SOMEIP_HEADER_SIZE			8
#define VSOMEIP_SOMEIP_MAGIC_COOKIE_SIZE	8

#define VSOMEIP_SERVICE_POS_MIN				 0
#define VSOMEIP_SERVICE_POS_MAX				 1
#define VSOMEIP_METHOD_POS_MIN				 2
#define VSOMEIP_METHOD_POS_MAX				 3
#define VSOMEIP_LENGTH_POS_MIN				 4
#define VSOMEIP_LENGTH_POS_MAX				 7
#define VSOMEIP_CLIENT_POS_MIN				 8
#define VSOMEIP_CLIENT_POS_MAX				 9
#define VSOMEIP_SESSION_POS_MIN				10
#define VSOMEIP_SESSION_POS_MAX				11
#define VSOMEIP_PROTOCOL_VERSION_POS		12
#define VSOMEIP_INTERFACE_VERSION_POS		13
#define VSOMEIP_MESSAGE_TYPE_POS			14
#define VSOMEIP_RETURN_CODE_POS				15

#endif // VSOMEIP_DEFINES_HPP
