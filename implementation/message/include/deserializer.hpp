// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_DESERIALIZER_HPP
#define VSOMEIP_DESERIALIZER_HPP

#include <vector>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class message;

class deserializer {
public:
	deserializer();
	deserializer(byte_t *_data, std::size_t _length);
	deserializer(const deserializer& _other);
	virtual ~deserializer();

	void set_data(const byte_t *_data, std::size_t _length);
	void append_data(const byte_t *_data, std::size_t _length);
	void drop_data(std::size_t _length);

	std::size_t get_available() const;
	std::size_t get_remaining() const;
	void set_remaining(std::size_t _length);

	// to be used by applications to deserialize a message
	message * deserialize_message();

	// to be used (internally) by objects to deserialize their members
	// Note: this needs to be encapsulated!
	bool deserialize(uint8_t& _value);
	bool deserialize(uint16_t& _value);
	bool deserialize(uint32_t& _value, bool _omit_last_byte = false);
	bool deserialize(uint8_t *_data, std::size_t _length);
	bool deserialize(std::vector< uint8_t >& _value);

	bool look_ahead(std::size_t _index, uint8_t &_value) const;
	bool look_ahead(std::size_t _index, uint16_t &_value) const;
	bool look_ahead(std::size_t _index, uint32_t &_value) const;

	void reset();
#ifdef VSOMEIP_DEBUGGING
	void show() const;
#endif
protected:
	std::vector< byte_t > data_;
	std::vector< byte_t >::iterator position_;
	std::size_t remaining_;
};

} // namespace vsomeip

#endif // VSOMEIP_DESERIALIZER_HPP
