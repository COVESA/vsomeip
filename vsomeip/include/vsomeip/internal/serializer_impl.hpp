//
// serializer_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SERIALIZER_IMPL_HPP
#define VSOMEIP_INTERNAL_SERIALIZER_IMPL_HPP

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

	bool serialize(const serializable *_from);

	bool serialize(const uint8_t _value);
	bool serialize(const uint16_t _value);
	bool serialize(const uint32_t _value, bool _omit_last_byte = false);
	bool serialize(const uint8_t *_data, uint32_t _length);

	virtual uint8_t * get_data() const;
	virtual uint32_t get_capacity() const;
	virtual uint32_t get_size() const;

	virtual void create_data(uint32_t _capacity);
	virtual void set_data(uint8_t *_data, uint32_t _capacity);

	virtual void reset();

private:
	uint8_t *data_;
	uint32_t capacity_;

	uint8_t *position_;
	uint32_t remaining_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SERIALIZER_IMPL_HPP
