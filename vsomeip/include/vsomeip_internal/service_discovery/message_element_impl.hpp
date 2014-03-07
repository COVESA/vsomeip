//
// message_element.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SERVICE_DISCOVERY_MESSAGE_ELEMENT_IMPL_HPP
#define VSOMEIP_INTERNAL_SERVICE_DISCOVERY_MESSAGE_ELEMENT_IMPL_HPP

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

#endif // VSOMEIP_INTERNAL_SERVICE_DISCOVERY_MESSAGE_ELEMENT_IMPL_HPP
