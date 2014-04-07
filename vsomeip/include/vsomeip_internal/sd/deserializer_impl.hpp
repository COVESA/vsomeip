//
// deserializer_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_DESERIALIZER_IMPL_HPP
#define VSOMEIP_INTERNAL_SD_DESERIALIZER_IMPL_HPP

#include <vsomeip_internal/deserializer_impl.hpp>

namespace vsomeip {
namespace sd {

class deserializer_impl
	: virtual public vsomeip::deserializer_impl {
public:
	virtual ~deserializer_impl();

	message_base * deserialize_message();
};

} // namespace sd
} // vsomeip

#endif // VSOMEIP_INTERNAL_SD_DESERIALIZER_IMPL_HPP
