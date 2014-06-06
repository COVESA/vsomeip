//
// serializer.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <cstring>

#include <vsomeip/serializable.hpp>
#include <vsomeip_internal/byteorder.hpp>
#include <vsomeip_internal/serializer.hpp>

namespace vsomeip {

serializer::serializer()
	: data_(0), capacity_(0), position_(0), remaining_(0) {
}

serializer::~serializer() {
};

bool serializer::serialize(const serializable *_from) {
	return (_from && _from->serialize(this));
}

bool serializer::serialize(const uint8_t _value) {
	if (1 > remaining_)
		return false;

	*position_++ = _value;
	remaining_--;

	return true;
}

bool serializer::serialize(const uint16_t _value) {
	if (2 > remaining_)
		return false;

	*position_++ = VSOMEIP_WORD_BYTE1(_value);
	*position_++ = VSOMEIP_WORD_BYTE0(_value);
	remaining_ -= 2;

	return true;
}

bool serializer::serialize(const uint32_t _value, bool _omit_last_byte) {
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

bool serializer::serialize(const uint8_t *_data, uint32_t _length) {
	if (_length > remaining_)
		return false;

	::memcpy(position_, _data, _length);
	position_ += _length;
	remaining_ -= _length;

	return true;
}

uint8_t * serializer::get_data() const {
	return data_;
}

uint32_t serializer::get_capacity() const {
	return capacity_;
}

uint32_t serializer::get_size() const {
	return capacity_ - remaining_;
}

void serializer::create_data(uint32_t _capacity) {
	if (0 != data_)
		delete [] data_;

	data_ = new uint8_t[_capacity];
	position_ = data_;
	if (0 != data_) {
		capacity_ = remaining_ = _capacity;
	} else {
		capacity_ = remaining_ = 0;
	}
}

void serializer::set_data(uint8_t *_data, uint32_t _capacity) {
	delete [] data_;

	data_ = _data;
	position_ = _data;

	if (0 != data_) {
		capacity_ = remaining_ = _capacity;
	} else {
		capacity_ = remaining_ = 0;
	}
}

void serializer::reset() {
	position_ = data_;
	remaining_ = capacity_;
}

} // namespace vsomeip





