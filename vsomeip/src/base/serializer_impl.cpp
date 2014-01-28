//
// serializer_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <cstring>

#include <vsomeip/internal/byteorder.hpp>
#include <vsomeip/internal/serializer_impl.hpp>

namespace vsomeip {

serializer_impl::serializer_impl()
	: data_(0), capacity_(0), position_(0), remaining_(0) {
}

serializer_impl::~serializer_impl() {
};

bool serializer_impl::serialize(const serializable *_from) {
	return _from->serialize(this);
}

bool serializer_impl::serialize(const uint8_t _value) {
	if (1 > remaining_)
		return false;

	*position_++ = _value;
	remaining_--;

	return true;
}

bool serializer_impl::serialize(const uint16_t _value) {
	if (2 > remaining_)
		return false;

	*position_++ = VSOMEIP_WORD_BYTE1(_value);
	*position_++ = VSOMEIP_WORD_BYTE0(_value);
	remaining_ -= 2;

	return true;
}

bool serializer_impl::serialize(const uint32_t _value, bool _omit_last_byte) {
	if (3 > remaining_ || (!_omit_last_byte && 4 > remaining_))
		return false;

	if (!_omit_last_byte) {
		*position_++ = VSOMEIP_LONG_BYTE3(_value);
		remaining_--;
	}
	*position_++ = VSOMEIP_LONG_BYTE2(_value);
	*position_++ = VSOMEIP_LONG_BYTE1(_value);
	*position_++ = VSOMEIP_LONG_BYTE0(_value);
	remaining_ -= 3;

	return true;
}

bool serializer_impl::serialize(const uint8_t *_data, uint32_t _length) {
	if (_length > remaining_)
		return false;

	::memcpy(position_, _data, _length);
	position_ += _length;
	remaining_ -= _length;

	return true;
}

uint8_t * serializer_impl::get_data() const {
	return data_;
}

uint32_t serializer_impl::get_capacity() const {
	return capacity_;
}

uint32_t serializer_impl::get_size() const {
	return capacity_ - remaining_;
}

void serializer_impl::create_data(uint32_t _capacity) {
	if (0 != data_)
		delete [] data_;

	data_ = new uint8_t[_capacity];
	// TODO: check memory allocation

	position_ = data_;
	capacity_ = _capacity;
	remaining_ = _capacity;
}

void serializer_impl::set_data(uint8_t *_data, uint32_t _capacity) {
	if (0 != data_)
		delete [] data_;

	data_ = _data;
	position_ = _data;
	capacity_ = _capacity;
	remaining_ = _capacity;
}

void serializer_impl::reset() {
	position_ = data_;
	remaining_ = capacity_;
}

} // namespace vsomeip





