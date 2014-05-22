//
// payload_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_PAYLOAD_IMPL_HPP
#define VSOMEIP_INTERNAL_PAYLOAD_IMPL_HPP

#include <vsomeip/payload.hpp>
#include <vector>

namespace vsomeip {

class serializer;
class deserializer;

class payload_impl : public payload {
public:
	payload_impl();
	payload_impl(const payload_impl& payload);
	virtual ~payload_impl();

	uint8_t * get_data();
	const uint8_t * get_data() const;
	uint32_t get_length() const;

	void set_capacity(uint32_t _capacity);

	void set_data(const uint8_t *data, uint32_t length);
	void set_data(const std::vector<uint8_t>& data);

	bool serialize(serializer *_to) const;
	bool deserialize(deserializer *_from);

private:
	std::vector< uint8_t > data_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_PAYLOAD_IMPL_HPP
