//
// optionimpl.cpp
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <vsomeip/serializer.h>
#include <vsomeip/deserializer.h>

#include <vsomeip/sd/optionimpl.h>

namespace vsomeip {

namespace sd {

OptionImpl::OptionImpl() {
	m_length = 0;
	m_type = OptionType::UNKNOWN;
}

OptionImpl::~OptionImpl() {
}

uint16_t OptionImpl::getLength() const {
	return m_length;
}

OptionType OptionImpl::getType() const {
	return m_type;
}

bool OptionImpl::serialize(Serializer *a_serializer) const {
	uint8_t l_type = static_cast<uint8_t>(m_type);
	return a_serializer->serialize16(m_length)
			&& a_serializer->serialize8(l_type)
			&& a_serializer->serialize8(0x00);
}

bool OptionImpl::deserialize(Deserializer *a_deserializer) {
	uint8_t l_type, l_reserved;
	bool l_result = a_deserializer->deserialize16(m_length)
			&& a_deserializer->deserialize8(l_type)
			&& a_deserializer->deserialize8(l_reserved);

	if (l_result) {
		m_type = static_cast<OptionType>(l_type);
	}

	return l_result;
}

} // namespace sd

} // namespace vsomeip

