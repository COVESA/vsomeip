//
// message_element.hpp
//
// Date: 	Jan 10, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_IMPL_MESSAGE_ELEMENT_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_IMPL_MESSAGE_ELEMENT_IMPL_HPP

namespace vsomeip {
namespace service_discovery {

class message_impl;

class message_element_impl {
public:
	message_element_impl();

	message_impl * get_owning_message() const;
	void set_owning_message(message_impl *_owner);

protected:
	message_impl *owner_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_IMPL_MESSAGE_ELEMENT_IMPL_HPP
