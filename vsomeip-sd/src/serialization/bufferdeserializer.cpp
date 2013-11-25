//
// bufferdeserializer.cpp
//
// Date: 	Nov 14, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <cstring>

#include <vsomeip/sd/bufferdeserializer.h>

#include <vsomeip/macros.h>
#include <vsomeip/sd/sdmessageimpl.h>
#include <vsomeip/sd/eventgroupentryimpl.h>
#include <vsomeip/sd/serviceentryimpl.h>
#include <vsomeip/sd/configurationoptionimpl.h>
#include <vsomeip/sd/ipv4endpointoptionimpl.h>
#include <vsomeip/sd/ipv6endpointoptionimpl.h>
#include <vsomeip/sd/loadbalancingoptionimpl.h>
#include <vsomeip/sd/protectionoptionimpl.h>

namespace vsomeip {

namespace sd {

BufferDeserializer::BufferDeserializer() {
}

BufferDeserializer::BufferDeserializer(const BufferDeserializer& a_deserializer, bool a_isDeepCopy)
	: vsomeip::BufferDeserializer(a_deserializer) {
}

Deserializer *BufferDeserializer::copy(bool a_isDeepCopy) {
	return new BufferDeserializer(*this, a_isDeepCopy);
}

BufferDeserializer::~BufferDeserializer() {
}

Message * BufferDeserializer::deserializeMessage() {
	Message* l_message = 0;

	if (m_length >= 4) {

		// Look ahead and instantiate Message or Service Discovery Message
		if (m_buffer[0] == 0xFF && m_buffer[1] == 0xFF && m_buffer[2] == 0x81 && m_buffer[3] == 0x00) {
			l_message = new sd::SdMessageImpl;
			if (0 != l_message) {
				if (false == l_message->deserialize(this)) {
					delete l_message;
					l_message = 0;
				}
			}
		} else {
			l_message = vsomeip::BufferDeserializer::deserializeMessage();
		}
	}

	return l_message;
}

Entry * BufferDeserializer::deserializeEntry() {
	Entry* l_entry = 0;

	if (m_length > 0) {
		EntryType l_entryType = static_cast<sd::EntryType>(m_buffer[0]);

		switch (l_entryType) {
		case EntryType::FIND_SERVICE:
		case EntryType::OFFER_SERVICE:
		//case EntryType::STOP_OFFER_SERVICE:
		case EntryType::REQUEST_SERVICE:
			l_entry = new ServiceEntryImpl;
			break;

		case EntryType::FIND_EVENT_GROUP:
		case EntryType::PUBLISH_EVENTGROUP:
		//case EntryType::STOP_PUBLISH_EVENTGROUP:
		case EntryType::SUBSCRIBE_EVENTGROUP:
		//case EntryType::STOP_SUBSCRIBE_EVENTGROUP:
		case EntryType::SUBSCRIBE_EVENTGROUP_ACK:
		//case EntryType::STOP_SUBSCRIBE_EVENTGROUP_ACK:
			l_entry = new EventGroupEntryImpl;
			break;

		default:
			break;
		};

		// deserialize object
		if (!l_entry->deserialize(this)) {
			delete l_entry;
			l_entry = 0;
		};
	}

	return l_entry;
}

sd::Option * BufferDeserializer::deserializeOption() {
	sd::Option* l_option = 0;

	if (m_length > 2) {
		OptionType l_optionType = static_cast<OptionType>(m_buffer[2]);

		switch (l_optionType) {

		case OptionType::CONFIGURATION:
			l_option = new ConfigurationOptionImpl;
			break;
		case sd::OptionType::LOAD_BALANCING:
			l_option = new LoadBalancingOptionImpl;
			break;
		case sd::OptionType::PROTECTION:
			l_option = new ProtectionOptionImpl;
			break;
		case sd::OptionType::IP4_ENDPOINT:
			l_option = new IPv4EndpointOptionImpl;
			break;
		case sd::OptionType::IP6_ENDPOINT:
			l_option = new IPv6EndpointOptionImpl;
			break;

		default:
			break;
		};

		// deserialize object
		if (!l_option->deserialize(this)) {
			delete l_option;
			l_option = 0;
		};
	}

	return l_option;
}

} // namespace sd

} // namespace vsomeip
