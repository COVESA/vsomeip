//
// deserializer.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_DESERIALIZER_HPP
#define VSOMEIP_INTERNAL_SD_DESERIALIZER_HPP

#include <vsomeip_internal/deserializer.hpp>

namespace vsomeip {
namespace sd {

class message;

class deserializer
	: public vsomeip::deserializer {
public:
	deserializer();
	deserializer(uint8_t *_data, std::size_t _length);
	deserializer(const deserializer &_other);
	virtual ~deserializer();

	message * deserialize_sd_message();
};

} // namespace sd
} // vsomeip

#endif // VSOMEIP_INTERNAL_SD_DESERIALIZER_HPP
