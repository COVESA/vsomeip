//
// serviceentryimpl.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_LIBRARY_SD_SERVICEENTRY_H__
#define __VSOMEIP_LIBRARY_SD_SERVICEENTRY_H__

#include <vsomeip/sd/serviceentry.h>
#include <vsomeip/sd/entryimpl.h>

namespace vsomeip {

namespace sd {

class ServiceEntryImpl : virtual public ServiceEntry, virtual public EntryImpl {
public:
	ServiceEntryImpl();
	virtual ~ServiceEntryImpl();

	MinorVersion getMinorVersion() const;
	void setMinorVersion(MinorVersion a_minorVersion);

	bool serialize(vsomeip::Serializer *a_serializer) const;
	bool deserialize(vsomeip::Deserializer *a_deserializer);

private:
	MinorVersion m_minorVersion;
};

} // namespace sd

} // namespace vsomeip


#endif /* SERVICEENTRY_H_ */
