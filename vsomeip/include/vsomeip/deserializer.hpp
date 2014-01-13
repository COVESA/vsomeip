//
// deserializer.hpp
//
// Date: 	Jan 9, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef DESERIALIZER_HPP
#define DESERIALIZER_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/copyable.hpp>
#include <vsomeip/message_base.hpp>
#include <vector>

namespace vsomeip {

class deserializer
	: virtual public copyable {
public:
	virtual message_base * deserialize_message() = 0;

	virtual bool deserialize(uint8_t& _value) = 0;
	virtual bool deserialize(uint16_t& _value) = 0;
	virtual bool deserialize(uint32_t& _value, bool _omit_last_byte = false) = 0;
	virtual bool deserialize(uint8_t *_data, uint32_t _length) = 0;
	virtual bool deserialize(std::vector<uint8_t>& _value) = 0;

	virtual bool look_ahead(uint32_t _index, uint8_t &_value) const = 0;

	virtual uint32_t get_remaining() const = 0;
	virtual void set_remaining(uint32_t _remaining) = 0;

protected:
	virtual ~deserializer() {};
};

} // namespace vsomeip

#endif // DESERIALIZER_HPP
