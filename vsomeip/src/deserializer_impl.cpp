//
// deserializer_impl.cpp
//
// Date: 	Nov 14, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <cstring>

#include <vsomeip/impl/byteorder_impl.hpp>
#include <vsomeip/impl/message_impl.hpp>
#include <vsomeip/impl/deserializer_impl.hpp>

namespace vsomeip {

deserializer_impl::deserializer_impl() {
	data_ = 0;
	length_ = 0;
	position_ = 0;
	remaining_ = 0;
	is_owning_data_ = false;
}

deserializer_impl::deserializer_impl(uint8_t *_data, uint32_t _length) {
	data_ = _data;
	length_ = _length;
	position_ = _data;
	remaining_ = _length;
	is_owning_data_ = true;
}

deserializer_impl::deserializer_impl(const deserializer_impl& _deserializer, bool _is_deep_copy_request) {
	length_ = _deserializer.length_;
	remaining_ = _deserializer.remaining_;

	if (_is_deep_copy_request && _deserializer.data_) {
		data_ = new uint8_t[length_];
		is_owning_data_ = true;
		if (data_) {
			::memcpy(data_, _deserializer.data_, length_);
		} else {
			// TODO: throw exception here!
		}
	} else {
		data_ = _deserializer.data_;
		is_owning_data_ = false;
	}

	position_ = data_ + (length_ - remaining_);
}

copyable * deserializer_impl::copy(bool _is_deep_copy_request) const {
	return new deserializer_impl(*this, _is_deep_copy_request);
}

deserializer_impl::~deserializer_impl() {
	if (is_owning_data_)
		delete [] data_;
}

uint32_t deserializer_impl::get_remaining() const {
	return remaining_;
}

void deserializer_impl::set_remaining(uint32_t _remaining) {
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

bool deserializer_impl::deserialize(uint32_t& _value, bool _omit_last_byte) {
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

	_value = VSOMEIP_BYTES_TO_LONG(byte0, byte1, byte2, byte3);

	return true;
}

bool deserializer_impl::deserialize(uint8_t *_data, uint32_t _length) {
	if (_length > remaining_)
		return false;

	::memcpy(_data, position_, _length);
	position_ += _length;
	remaining_ -= _length;

	return true;
}

bool deserializer_impl::deserialize(std::vector<uint8_t>& _value) {
	if (_value.capacity() > remaining_)
		return false;

	_value.assign(position_, position_ + remaining_);
	remaining_ -= _value.capacity();

	return true;
}

bool deserializer_impl::look_ahead(uint32_t _index, uint8_t &_value) const {
	if (_index >= remaining_)
		return false;

	_value = position_[_index];

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

void deserializer_impl::set_data(uint8_t *_data,  uint32_t _length) {
	if (0 != data_)
		delete [] data_;

	data_ = _data;
	length_ = _length;
	position_ = data_;
	remaining_ = length_;
}

} // namespace vsomeip
