//
// payload.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_PAYLOAD_HPP
#define VSOMEIP_PAYLOAD_HPP

#include <vector>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class payload {
public:
	virtual uint8_t * get_data() = 0;
	virtual uint32_t get_length() const = 0;
	virtual void set_data(const uint8_t *data, uint32_t length) = 0;
	virtual void set_data(const std::vector<uint8_t>& data) = 0;

protected:
	virtual ~payload() {};
};

} // namespace vsomeip

#endif // VSOMEIP_PAYLOAD_HPP
