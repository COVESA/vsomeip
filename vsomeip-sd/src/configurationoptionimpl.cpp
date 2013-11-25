//
// configurationoptionimpl.cpp
//
// Date: 	Oct 30, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//
#include <cstring>

#include <vsomeip/serializer.h>
#include <vsomeip/deserializer.h>

#include <vsomeip/sd/configurationoptionimpl.h>

namespace vsomeip {

namespace sd {

ConfigurationOptionImpl::ConfigurationOptionImpl() {
	m_length = 2; // always contains "Reserved" and the trailing '\0'
	m_type = OptionType::CONFIGURATION;
}

ConfigurationOptionImpl::~ConfigurationOptionImpl() {
}

bool ConfigurationOptionImpl::operator ==(const Option& a_option) const {
	if (a_option.getType() != OptionType::CONFIGURATION)
		return false;

	const ConfigurationOptionImpl& l_configurationOption
		= reinterpret_cast<const ConfigurationOptionImpl&>(a_option);

	return (m_configuration == l_configurationOption.m_configuration);
}

void ConfigurationOptionImpl::addItem(const std::string& a_key, const std::string& a_value) {
	m_configuration[a_key] = a_value;
	m_length += (a_key.length() + a_value.length() + 2); // +2 for the '=' and length
}

void ConfigurationOptionImpl::removeItem(const std::string& a_key) {
	auto it = m_configuration.find(a_key);
	if (it != m_configuration.end()) {
		m_length -= (it->first.length() + it->second.length() + 2);
		m_configuration.erase(it);
	}
}

std::vector<std::string> ConfigurationOptionImpl::getKeys() const {
	std::vector<std::string> l_keys;
	for (auto elem : m_configuration)
		l_keys.push_back(elem.first);
	return l_keys;
}

std::vector<std::string> ConfigurationOptionImpl::getValues() const {
	std::vector<std::string> l_values;
	for (auto elem : m_configuration)
		l_values.push_back(elem.second);
	return l_values;
}

std::string ConfigurationOptionImpl::getValue(const std::string& a_key) const {
	std::string l_value("");
	auto l_elem = m_configuration.find(a_key);
	if (l_elem != m_configuration.end())
		l_value = l_elem->second;
	return l_value;
}

bool ConfigurationOptionImpl::serialize(Serializer *a_serializer) const {
	bool l_isSuccessful;
	std::string l_configurationString;

	for (auto i = m_configuration.begin(); i != m_configuration.end(); ++i) {
		char l_length = 1 + i->first.length() + i->second.length();
		l_configurationString.push_back(l_length);
		l_configurationString.append(i->first);
		l_configurationString.push_back('=');
		l_configurationString.append(i->second);
	}
	l_configurationString.push_back('\0');

	l_isSuccessful = OptionImpl::serialize(a_serializer);
	if (l_isSuccessful) {
		l_isSuccessful = a_serializer->serializeX(
				reinterpret_cast<const uint8_t*>(l_configurationString.c_str()),
				l_configurationString.length());
	}

	return l_isSuccessful;
}

bool ConfigurationOptionImpl::deserialize(Deserializer *a_deserializer) {
	bool l_isSuccessful = OptionImpl::deserialize(a_deserializer);
	uint8_t l_length = 0;
	std::string l_item(256, 0), l_key, l_value;

	do {
		l_isSuccessful = l_isSuccessful && a_deserializer->deserialize8(l_length);
		if (l_length > 0) {
			l_isSuccessful = l_isSuccessful && a_deserializer->deserializeX((uint8_t*)&l_item[0], l_length);
			if (l_isSuccessful) {
				size_t l_eqPos = l_item.find('=');
				l_key = l_item.substr(0, l_eqPos);
				l_value = l_item.substr(l_eqPos+1);

				if (m_configuration.end() == m_configuration.find(l_key)) {
					m_configuration[l_key] = l_value;
				} else {
					// TODO: log reason for failing deserialization
					l_isSuccessful = false;
				}
			}
		}

	} while (l_isSuccessful && l_length > 0);

	return l_isSuccessful;
}

} // namespace sd

} // namespace vsomeip

