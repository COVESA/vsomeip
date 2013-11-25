//
// sdmessagepartimpl.h
//
// Date: 	Nov 20, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_LIBRARY_SD_SDMESSAGEPARTIMPL_H__
#define __VSOMEIP_LIBRARY_SD_SDMESSAGEPARTIMPL_H__

namespace vsomeip {

namespace sd {

class SdMessageImpl;

class SdMessagePartImpl {
public:
	SdMessagePartImpl();
	SdMessagePartImpl(const SdMessagePartImpl& a_part);
	virtual ~SdMessagePartImpl();

	const SdMessageImpl * getMessage() const;
	void setMessage(const SdMessageImpl* a_message);

private:
	const SdMessageImpl *m_message;
};

} // namespace sd

} // namespace vsomeip

#endif /* __VSOMEIP_LIBRARY_SD_SDMESSAGEPARTIMPL_H__ */
