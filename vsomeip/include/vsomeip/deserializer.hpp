//
// deserializer.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_DESERIALIZER_HPP
#define VSOMEIP_DESERIALIZER_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/message_base.hpp>
#include <vector>

namespace vsomeip {

class deserializer {
public:
	virtual message_base * deserialize_message() = 0;

	virtual bool deserialize(uint8_t& _value) = 0;
	virtual bool deserialize(uint16_t& _value) = 0;
	virtual bool deserialize(uint32_t& _value, bool _omit_last_byte = false) = 0;
	virtual bool deserialize(uint8_t *_data, std::size_t _length) = 0;
	virtual bool deserialize(std::vector<uint8_t>& _value) = 0;

	virtual bool look_ahead(std::size_t _index, uint8_t &_value) const = 0;
	virtual bool look_ahead(std::size_t _index, uint16_t &_value) const = 0;
	virtual bool look_ahead(std::size_t _index, uint32_t &_value) const = 0;

	virtual std::size_t get_available() const = 0;
	virtual std::size_t get_remaining() const = 0;
	virtual void set_remaining(std::size_t _remaining) = 0;

	virtual void set_data(uint8_t *_data, std::size_t _length) = 0;
	virtual void append_data(const uint8_t *_data, std::size_t _length) = 0;

	virtual void reset() = 0;

protected:
	virtual ~deserializer() {};
};

} // namespace vsomeip

#endif // VSOMEIP_DESERIALIZER_HPP
