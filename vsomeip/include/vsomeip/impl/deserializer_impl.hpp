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

#include <vector>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/deserializer.hpp>

namespace vsomeip {

class deserializer_impl
		: virtual public deserializer {
public:
	deserializer_impl();
	deserializer_impl(uint8_t *_data, std::size_t _length);
	deserializer_impl(const deserializer_impl& _deserializer);
	virtual ~deserializer_impl();

	virtual void set_data(uint8_t *_data, std::size_t _length);
	virtual void append_data(const uint8_t *_data, std::size_t _length);

	virtual std::size_t get_available() const;
	virtual std::size_t get_remaining() const;
	virtual void set_remaining(std::size_t _length);

	// to be used by applications to deserialize a message
	virtual message_base * deserialize_message();

	// to be used (internally) by objects to deserialize their members
	// Note: this needs to be encapsulated!
	virtual bool deserialize(uint8_t& _value);
	virtual bool deserialize(uint16_t& _value);
	virtual bool deserialize(uint32_t& _value, bool _omit_last_byte = false);
	virtual bool deserialize(uint8_t *_data, std::size_t _length);
	virtual bool deserialize(std::vector<uint8_t>& _value);

	virtual bool look_ahead(std::size_t _index, uint8_t &_value) const;
	virtual bool look_ahead(std::size_t _index, uint16_t &_value) const;
	virtual bool look_ahead(std::size_t _index, uint32_t &_value) const;

	virtual void reset();

protected:
	std::vector< uint8_t > data_;
	std::vector< uint8_t >::iterator position_;
	std::size_t remaining_;
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_DESERIALIZER_IMPL_HPP
