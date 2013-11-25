//
// eventgroupentry.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SD_EVENTGROUPENTRY_H__
#define __VSOMEIP_SD_EVENTGROUPENTRY_H__

#include <vsomeip/sd/entry.h>

namespace vsomeip {

namespace sd {

class EventGroupEntry : virtual public Entry {
public:
	virtual ~EventGroupEntry() {};

	virtual EventGroupId getEventGroupId() const = 0;
	virtual void setEventGroupId(EventGroupId a_eventGroupId) = 0;
};

} // namespace sd

} // namespace vsomeip

#endif /* EVENTGROUPENTRY_H_ */
