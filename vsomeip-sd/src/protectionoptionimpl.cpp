//
// protectionoptionimpl.cpp
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

#include <vsomeip/sd/protectionoptionimpl.h>

namespace vsomeip {

namespace sd {

ProtectionOptionImpl::ProtectionOptionImpl() {
	m_length = 1 + 4 + 4;
	m_type = OptionType::PROTECTION;
}

ProtectionOptionImpl::~ProtectionOptionImpl() {
}

bool ProtectionOptionImpl::operator ==(const Option& a_option) const {
	if (a_option.getType() != OptionType::PROTECTION)
		return false;

	const ProtectionOptionImpl& l_protectionOption
		= reinterpret_cast<const ProtectionOptionImpl&>(a_option);

	return (m_crc == l_protectionOption.m_crc
		 && m_aliveCounter == l_protectionOption.m_aliveCounter);
}

AliveCounter ProtectionOptionImpl::getAliveCounter() const {
	return m_aliveCounter;
}

void ProtectionOptionImpl::setAliveCounter(AliveCounter a_aliveCounter) {
	m_aliveCounter = a_aliveCounter;
}

Crc ProtectionOptionImpl::getCrc() const {
	return m_crc;
}

void ProtectionOptionImpl::setCrc(Crc a_crc) {
	m_crc = a_crc;
}

bool ProtectionOptionImpl::serialize(Serializer *a_serializer) const {
	bool l_isSuccessful = OptionImpl::serialize(a_serializer);

	l_isSuccessful = l_isSuccessful && a_serializer->serialize32(static_cast<uint32_t>(m_aliveCounter));
	l_isSuccessful = l_isSuccessful && a_serializer->serialize32(static_cast<uint32_t>(m_crc));

	return l_isSuccessful;
}

bool ProtectionOptionImpl::deserialize(Deserializer *a_deserializer) {
	bool l_isSuccessful = OptionImpl::deserialize(a_deserializer);

	uint32_t l_aliveCounter;
	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize32(l_aliveCounter);
	m_aliveCounter = static_cast<AliveCounter>(l_aliveCounter);

	uint32_t l_crc;
	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize32(l_crc);
	m_crc = static_cast<AliveCounter>(l_crc);

	return l_isSuccessful;
}

} // namespace sd

} // namespace vsomeip


