// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_PAYLOAD_IMPL_HPP
#define VSOMEIP_PAYLOAD_IMPL_HPP

#include <vsomeip/payload.hpp>

namespace vsomeip {

class serializer;
class deserializer;

class payload_impl: public payload {
public:
	payload_impl();
	payload_impl(const payload_impl& _payload);
	virtual ~payload_impl();

	bool operator == (const payload &_other);

	byte_t * get_data();
	const byte_t * get_data() const;
	length_t get_length() const;

	void set_capacity(length_t _capacity);

	void set_data(const byte_t *_data, length_t _length);
	void set_data(const std::vector< byte_t > &_data);

	bool serialize(serializer *_to) const;
	bool deserialize(deserializer *_from);

private:
	std::vector< byte_t > data_;
};

} // namespace vsomeip

#endif // VSOMEIP_PAYLOAD_IMPL_HPP
