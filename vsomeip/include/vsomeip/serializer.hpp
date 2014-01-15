//
// serializer.hpp
//
// Date: 	Nov 29, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERIALIZER_HPP
#define VSOMEIP_SERIALIZER_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/serializable.hpp>

namespace vsomeip {

class serializer {
public:
	virtual bool serialize(const serializable& _from) = 0;

	virtual bool serialize(const uint8_t _value) = 0;
	virtual bool serialize(const uint16_t _value) = 0;
	virtual bool serialize(const uint32_t _value, bool _omit_last_byte = false) = 0;
	virtual bool serialize(const uint8_t *_data, uint32_t _length) = 0;

	virtual uint8_t * get_data() const = 0;
	virtual uint32_t get_capacity() const = 0;
	virtual uint32_t get_size() const = 0;

	virtual void create_data(uint32_t _capacity) = 0;
	virtual void set_data(uint8_t *_data, uint32_t _capacity) = 0;

protected:
	virtual ~serializer() {};

};

} // namespace vsomeip

#endif // VSOMEIP_SERIALIZER_HPP
