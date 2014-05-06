//
// payload_impl.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/deserializer.hpp>
#include <vsomeip_internal/payload_impl.hpp>
#include <vsomeip_internal/serializer.hpp>

namespace vsomeip {

payload_impl::payload_impl()
	: data_() {
}

payload_impl::payload_impl(const payload_impl& payload)
	: data_(payload.data_) {
}

payload_impl::~payload_impl() {
}

uint8_t * payload_impl::get_data() {
	return data_.data();
}

uint32_t payload_impl::get_length() const {
	return data_.size();
}

void payload_impl::set_capacity(uint32_t _capacity) {
	data_.reserve(_capacity);
}

void payload_impl::set_data(const uint8_t *data, const uint32_t length) {
	data_.assign(data, data + length);
}

void payload_impl::set_data(const std::vector<uint8_t>& data) {
	data_ = data;
}

bool payload_impl::serialize(serializer *_to) const {
	return (0 != _to && _to->serialize(data_.data(), data_.size()));
}

bool payload_impl::deserialize(deserializer *_from) {
	return (0 != _from && _from->deserialize(data_));
}

} // namespace vsomeip
