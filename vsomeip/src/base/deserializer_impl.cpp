//
// deserializer_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <cstring>
#include <iomanip>
#include <iostream>

#include <vsomeip/impl/byteorder_impl.hpp>
#include <vsomeip/impl/message_impl.hpp>
#include <vsomeip/impl/deserializer_impl.hpp>

namespace vsomeip {

deserializer_impl::deserializer_impl() {
	position_ = data_.begin();
}

deserializer_impl::deserializer_impl(uint8_t *_data, std::size_t _length)
	: data_(_data, _data + _length),
	  position_(data_.begin()) {
}

deserializer_impl::deserializer_impl(const deserializer_impl& _deserializer)
	: data_(_deserializer.data_),
	  position_(_deserializer.position_){
}

deserializer_impl::~deserializer_impl() {
}

std::size_t deserializer_impl::get_available() const {
	return data_.size();
}

std::size_t deserializer_impl::get_remaining() const {
	return remaining_;
}

void deserializer_impl::set_remaining(std::size_t _remaining) {
	remaining_ = _remaining;
}

bool deserializer_impl::deserialize(uint8_t& _value) {
	if (0 == remaining_)
		return false;

	_value = *position_++;

	remaining_--;
	return true;
}

bool deserializer_impl::deserialize(uint16_t& _value) {
	if (2 > remaining_)
		return false;

	uint8_t byte0, byte1;
	byte0 = *position_++;
	byte1 = *position_++;
	remaining_ -= 2;

	_value = VSOMEIP_BYTES_TO_WORD(byte0, byte1);

	return true;
}

bool deserializer_impl::deserialize(uint32_t &_value, bool _omit_last_byte) {
	if (3 > remaining_ || (!_omit_last_byte && 4 > remaining_))
		return false;

	uint8_t byte0 = 0, byte1, byte2, byte3;
	if (!_omit_last_byte) {
		byte0 = *position_++;
		remaining_--;
	}
	byte1 = *position_++;
	byte2 = *position_++;
	byte3 = *position_++;
	remaining_ -= 3;

	_value = VSOMEIP_BYTES_TO_LONG(
			byte0, byte1, byte2, byte3);

	return true;
}

bool deserializer_impl::deserialize(uint8_t *_data, std::size_t _length) {
	if (_length > remaining_)
		return false;

	::memcpy(_data, &_data[position_ - data_.begin()], _length);
	position_ += _length;
	remaining_ -= _length;

	return true;
}

bool deserializer_impl::deserialize(std::vector<uint8_t>& _value) {
	if (_value.capacity() > remaining_)
		return false;

	_value.assign(position_, position_ + _value.capacity());
	remaining_ -= _value.capacity();
	position_ += _value.capacity();

	return true;
}

bool deserializer_impl::look_ahead(std::size_t _index, uint8_t &_value) const {
	if (_index >= data_.size())
		return false;

	_value = *(position_ + _index);

	return true;
}

bool deserializer_impl::look_ahead(std::size_t _index, uint16_t &_value) const {
	if (_index+1 >= data_.size())
		return false;

	std::vector< uint8_t >::iterator i = position_ + _index;
	_value = VSOMEIP_BYTES_TO_WORD(*i, *(i+1));

	return true;
}

bool deserializer_impl::look_ahead(std::size_t _index, uint32_t &_value) const {
	if (_index+3 >= data_.size())
		return false;

	std::vector< uint8_t >::const_iterator i = position_ + _index;
	_value = VSOMEIP_BYTES_TO_LONG(*i, *(i+1), *(i+2), *(i+3));

	return true;
}

message_base * deserializer_impl::deserialize_message() {
	message_impl* deserialized_message = new message_impl;
	if (0 != deserialized_message) {
		if (false == deserialized_message->deserialize(this)) {
			delete deserialized_message;
			deserialized_message = 0;
		}
	}

	return deserialized_message;
}

void deserializer_impl::set_data(uint8_t *_data,  std::size_t _length) {
	if (0 != _data) {
		std::size_t offset = position_ - data_.begin();
		data_.assign(_data, _data + _length);
		position_ = data_.begin() + offset;
	}
}

void deserializer_impl::append_data(const uint8_t *_data, std::size_t _length) {
	std::size_t offset = (position_ - data_.begin());
	data_.insert(data_.end(), _data, _data + _length);
	position_ = data_.begin() + offset;
}

void deserializer_impl::reset() {
	data_.erase(data_.begin(), position_);
	position_ = data_.begin();
	remaining_ = data_.size();
}

} // namespace vsomeip
