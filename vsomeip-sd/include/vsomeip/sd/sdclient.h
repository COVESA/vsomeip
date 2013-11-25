//
// client.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_SDCLIENT_H__
#define __VSOMEIP_SD_SDCLIENT_H__

namespace vsomeip {

namespace sd {

class SdClient {
public:
	bool registerService();
	bool deregisterService();

	bool getService();
	bool getAllServices();
};

} // namespace sd

} // namespace vsomeip

#endif // __VSOMEIP_CLIENT_H__
