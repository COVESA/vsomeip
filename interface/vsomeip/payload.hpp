// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_PAYLOAD_HPP
#define VSOMEIP_PAYLOAD_HPP

#include <vector>

#include <vsomeip/deserializable.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/serializable.hpp>

namespace vsomeip {

class payload :
		public serializable,
		public deserializable {
public:
	virtual ~payload() {};

	virtual bool operator == (const payload &_other) = 0;

	virtual byte_t * get_data() = 0;
	virtual const byte_t * get_data() const = 0;
	virtual void set_data(const byte_t *_data, length_t _length) = 0;
	virtual void set_data(const std::vector< byte_t > &_data) = 0;

	virtual length_t get_length() const = 0;
	
	virtual void set_capacity(length_t _length) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_PAYLOAD_HPP
