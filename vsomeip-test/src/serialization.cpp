//
// serialization.cpp
//
// Date: 	Nov 22, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <iomanip>
#include <iostream>

#include <string>
#include <vector>

#include <vsomeip/applicationmessage.h>
#include <vsomeip/sd/factory.h>
#include <vsomeip/sd/sdmessage.h>
#include <vsomeip/sd/configurationoption.h>
#include <vsomeip/sd/ipv4endpointoption.h>
#include <vsomeip/sd/ipv6endpointoption.h>
#include <vsomeip/sd/loadbalancingoption.h>
#include <vsomeip/sd/protectionoption.h>

#include <vsomeip/sd/serviceentry.h>

#include <vsomeip/bufferserializer.h>
#include <vsomeip/sd/bufferdeserializer.h>

int main() {

	std::cout << "Serializing Application message:" << std::endl;
	vsomeip::ApplicationMessage *l_message = vsomeip::Factory::getDefaultFactory()->createApplicationMessage();
	l_message->setMessageId(0x12345678);
	l_message->setRequestId(0x98765432);
	l_message->setInterfaceVersion(0x2);
	l_message->setMessageType(vsomeip::MessageType::REQUEST_NO_RETURN);
	l_message->setReturnCode(vsomeip::ReturnCode::REQUEST);

	uint8_t l_payload[] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0x10, 0x11, 0x12, 0x13, 0x15, 0x14 };
	l_message->setPayload(l_payload, sizeof(l_payload));

	vsomeip::BufferSerializer l_serializer;
	l_serializer.createBuffer(100);

	if (!l_serializer.serialize(l_message)) {
		std::cout << "Serialization failed!" << std::endl;
	}

	uint8_t* l_buffer;
	uint32_t l_length;
	l_serializer.getBufferInfo(l_buffer, l_length);

	for (uint32_t i = 0; i < l_length; i++) {
		if (i % 4 == 0) std::cout << std::endl;
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)l_buffer[i]  << " ";
	}
	std::cout << std::endl << std::endl;

	std::cout << "Serializing ServiceDiscovery message:" << std::endl;

	vsomeip::sd::SdMessage* l_sdMessage = vsomeip::sd::Factory::getDefaultFactory()->createServiceDiscoveryMessage();
	l_sdMessage->setRequestId(0x11223344);
	l_sdMessage->setInterfaceVersion(0x01);
	l_sdMessage->setMessageType(vsomeip::MessageType::NOTIFICATION);
	l_sdMessage->setReturnCode(vsomeip::ReturnCode::REQUEST_NO_RETURN);
	l_sdMessage->setFlags(0x80);

	vsomeip::sd::IPv4EndpointOption& l_option = l_sdMessage->createIPv4EndPointOption();
	l_option.setAddress(0xC0A80001);
	l_option.setPort(0x1A91);
	l_option.setProtocol(vsomeip::sd::IPProtocol::TCP);

	vsomeip::sd::IPv4EndpointOption& l_option2 = l_sdMessage->createIPv4EndPointOption();
	l_option2.setAddress(0xC0A80001);
	l_option2.setPort(0x1A91);
	l_option2.setProtocol(vsomeip::sd::IPProtocol::UDP);

	vsomeip::sd::ServiceEntry& l_entry = l_sdMessage->createServiceEntry();
	l_entry.setType(vsomeip::sd::EntryType::FIND_SERVICE);
	l_entry.setServiceId(0x4711);
	l_entry.setInstanceId(0xFFFF);
	l_entry.setMajorVersion(0xFF);
	l_entry.setMinorVersion(0xFFFFFFFF);
	l_entry.setTimeToLive(3600);

	vsomeip::sd::ServiceEntry& l_entry2 = l_sdMessage->createServiceEntry();
	l_entry2.setType(vsomeip::sd::EntryType::OFFER_SERVICE);
	l_entry2.setServiceId(0x1001);
	l_entry2.setInstanceId(0x0001);
	l_entry2.setMajorVersion(0x01);
	l_entry2.setMinorVersion(0x00000032);
	l_entry2.assignOption(l_option, 1);
	l_entry2.assignOption(l_option2, 1);

	l_serializer.createBuffer(100);
	if (!l_serializer.serialize(l_sdMessage)) {
		std::cout << "Serialization failed!" << std::endl;
	}

	l_serializer.getBufferInfo(l_buffer, l_length);

	for (uint32_t i = 0; i < l_length; i++) {
		if (i % 4 == 0) std::cout << std::endl;
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)l_buffer[i]  << " ";
	}
	std::cout << std::endl << std::endl;

	vsomeip::sd::BufferDeserializer l_deserializer;
	l_deserializer.setBuffer(l_buffer, l_length);

	vsomeip::Message* l_deserializedMessage = l_deserializer.deserializeMessage();

	l_serializer.createBuffer(100);
	l_serializer.serialize(l_deserializedMessage);

	l_serializer.getBufferInfo(l_buffer, l_length);

	for (uint32_t i = 0; i <l_length; i++) {
		if (i % 4 == 0) std::cout << std::endl;
		std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)l_buffer[i]  << " ";
	}
	std::cout << std::endl << std::endl;
}
