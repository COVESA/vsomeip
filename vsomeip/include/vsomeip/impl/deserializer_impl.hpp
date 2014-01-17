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

#ifndef VSOMEIP_IMPL_DESERIALIZER_IMPL_HPP
#define VSOMEIP_IMPL_DESERIALIZER_IMPL_HPP

#include <deque>
#include <vector>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/deserializer.hpp>

namespace vsomeip {

class deserializer_impl
		: virtual public deserializer {
public:
	deserializer_impl();
	deserializer_impl(uint8_t *_data, uint32_t _length);
	deserializer_impl(const deserializer_impl& _deserializer, bool _is_deep_copy);
	virtual ~deserializer_impl();

	// set the data source
	void set_data(uint8_t *_data, uint32_t _length);

	// get/set remaining content length (setting is used for shallow copies)
	uint32_t get_remaining() const;
	void set_remaining(uint32_t _length);

	// to be used by applications to deserialize a message
	virtual message_base * deserialize_message();

	// to be used (internally) by objects to deserialize their members
	// Note: this needs to be encapsulated!
	virtual bool deserialize(uint8_t& _value);
	virtual bool deserialize(uint16_t& _value);
	virtual bool deserialize(uint32_t& _value, bool _omit_last_byte = false);
	virtual bool deserialize(uint8_t *_data, uint32_t _length);
	virtual bool deserialize(std::vector<uint8_t>& _value);

	virtual bool look_ahead(uint32_t _index, uint8_t &_value) const;

protected:
	uint8_t *data_;
	uint32_t length_;

	uint8_t *position_;
	uint32_t remaining_;

	bool is_owning_data_;
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_DESERIALIZER_IMPL_HPP
