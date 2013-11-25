//
// config.h
//
// Date: 	Oct 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//
#ifndef __VSOMEIP_CONFIG_H__
#define __VSOMEIP_CONFIG_H__

#define VSOMEIP_PROTOCOL_VERSION				0x00

#define VSOMEIP_PROTOCOL_RESERVED				0x00
#define VSOMEIP_PROTOCOL_OPTION_TYPE			0x01

#define VSOMEIP_MESSAGE_FULL_HEADER_LENGTH 		16
#define VSOMEIP_MESSAGE_FIXED_HEADER_LENGTH	 	8
#define VSOMEIP_MESSAGE_MAX_LENGTH 				4096

#define VSOMEIP_ENTRY_LENGTH					16
#define VSOMEIP_OPTION_HEADER_LENGTH			3

#define VSOMEIP_MAX_OPTION_RUN					2

#endif // __VSOMEIP_CONFIG_H__
