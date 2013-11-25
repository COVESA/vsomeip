//
// loadbalancingoption.cpp
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

#include <vsomeip/sd/loadbalancingoptionimpl.h>

namespace vsomeip {

namespace sd {

LoadBalancingOptionImpl::LoadBalancingOptionImpl() {
	m_length = 1 + 2 + 2;
	m_type = OptionType::LOAD_BALANCING;
}

LoadBalancingOptionImpl::~LoadBalancingOptionImpl() {
}

bool LoadBalancingOptionImpl::operator ==(const Option& a_option) const {
	if (a_option.getType() != OptionType::LOAD_BALANCING)
		return false;

	const LoadBalancingOptionImpl& l_loadBalancingOption
		= reinterpret_cast<const LoadBalancingOptionImpl&>(a_option);

	return (m_priority == l_loadBalancingOption.m_priority
		 && m_weight == l_loadBalancingOption.m_weight);
}

Priority LoadBalancingOptionImpl::getPriority() const {
	return m_priority;
}

void LoadBalancingOptionImpl::setPriority(Priority a_priority) {
	m_priority = a_priority;
}

Weight LoadBalancingOptionImpl::getWeight() const {
	return m_weight;
}

void LoadBalancingOptionImpl::setWeight(Weight a_weight) {
	m_weight = a_weight;
}

bool LoadBalancingOptionImpl::serialize(Serializer *a_serializer) const {
	bool l_isSuccessful = OptionImpl::serialize(a_serializer);
	l_isSuccessful = l_isSuccessful && a_serializer->serialize16(static_cast<uint16_t>(m_priority));
	l_isSuccessful = l_isSuccessful && a_serializer->serialize16(static_cast<uint16_t>(m_weight));
	return l_isSuccessful;
}

bool LoadBalancingOptionImpl::deserialize(Deserializer *a_deserializer) {
	bool l_isSuccessful = OptionImpl::deserialize(a_deserializer);

	uint16_t l_priority;
	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize16(l_priority);
	m_priority = static_cast<Priority>(l_priority);

	uint16_t l_weight;
	l_isSuccessful = l_isSuccessful && a_deserializer->deserialize16(l_weight);
	m_weight = static_cast<Weight>(l_weight);

	return l_isSuccessful;
}

} // namespace sd

} // namespace vsomeip
