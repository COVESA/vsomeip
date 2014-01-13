//
// payload.hpp
//
// Date: 	Jan 9, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_PAYLOAD_HPP__
#define __VSOMEIP_PAYLOAD_HPP__

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

#endif // __VSOMEIP_PAYLOAD_HPP__
