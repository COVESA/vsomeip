//
// deserializer_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_IMPL_DESERIALIZER_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_IMPL_DESERIALIZER_IMPL_HPP

#include <vsomeip/impl/deserializer_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class deserializer_impl : virtual public vsomeip::deserializer_impl {
public:
	deserializer_impl(uint8_t *_data, uint32_t _length);
	virtual ~deserializer_impl();

	message_base * deserialize_message();
};

} // namespace service_discovery
} // vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_IMPL_DESERIALIZER_IMPL_HPP
