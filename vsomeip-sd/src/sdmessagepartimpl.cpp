//
// sdmessagepartimpl.cpp
//
// Date: 	Nov 20, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <vsomeip/sd/sdmessagepartimpl.h>

namespace vsomeip {

namespace sd {

SdMessagePartImpl::SdMessagePartImpl() {
	m_message = 0;
}

SdMessagePartImpl::SdMessagePartImpl(const SdMessagePartImpl& a_part) {
	m_message = a_part.m_message;
}

SdMessagePartImpl::~SdMessagePartImpl() {
}

const SdMessageImpl * SdMessagePartImpl::getMessage() const {
	return m_message;
}

void SdMessagePartImpl::setMessage(const SdMessageImpl *a_message) {
	m_message = a_message;
}

} // namespace sd

} // namespace vsomeip
