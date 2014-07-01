// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERIALIZER_HPP
#define VSOMEIP_SERIALIZER_HPP

#include <cstdint>
#include <vector>

namespace vsomeip {

class serializable;

class serializer
{
public:
	serializer();
	virtual ~serializer();

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

#endif // VSOMEIP_SERIALIZER_IMPL_HPP
