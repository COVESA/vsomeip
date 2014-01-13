//
// serializer_impl.hpp
//
// Date: 	Nov 29, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef SERIALIZER_IMPL_HPP
#define SERIALIZER_IMPL_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/serializer.hpp>

#include <vector>

namespace vsomeip {

class serializer_impl : public serializer
{
public:
	serializer_impl();
	virtual ~serializer_impl();

	bool serialize(serializable &_from);

	bool serialize(const uint8_t _value);
	bool serialize(const uint16_t _value);
	bool serialize(const uint32_t _value, bool _omit_last_byte = false);
	bool serialize(const uint8_t *_data, uint32_t _length);

	virtual const uint8_t * get_buffer() const;
	virtual uint32_t get_length() const;
	virtual void get_buffer_info(uint8_t *&_data, uint32_t &_length) const;

	virtual void create_buffer(uint32_t _length);
	virtual void set_buffer(uint8_t *_data, uint32_t _length);

private:
	uint8_t *data_;
	uint32_t length_;

	uint8_t *position_;
	uint32_t remaining_;
};

} // namespace vsomeip

#endif // SERIALIZER_IMPL_HPP
