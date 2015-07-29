// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstring>

#ifdef VSOMEIP_DEBUGGING
#include <iomanip>
#include <sstream>

#include <vsomeip/logger.hpp>
#endif

#include "../include/message_impl.hpp"
#include "../include/deserializer.hpp"
#include "../../utility/include/byteorder.hpp"

namespace vsomeip {

deserializer::deserializer()
	: position_(data_.begin()),
	  remaining_(0) {
}

deserializer::deserializer(byte_t *_data, std::size_t _length)
	: data_(_data, _data + _length),
	  position_(data_.begin()),
	  remaining_(_length) {
}

deserializer::deserializer(const deserializer &_other)
	: data_(_other.data_),
	  position_(_other.position_){
}

deserializer::~deserializer() {
}

std::size_t deserializer::get_available() const {
	return data_.size();
}

std::size_t deserializer::get_remaining() const {
	return remaining_;
}

void deserializer::set_remaining(std::size_t _remaining) {
	remaining_ = _remaining;
}

bool deserializer::deserialize(uint8_t& _value) {
	if (0 == remaining_)
		return false;

	_value = *position_++;

	remaining_--;
	return true;
}

bool deserializer::deserialize(uint16_t& _value) {
	if (2 > remaining_)
		return false;

	uint8_t byte0, byte1;
	byte0 = *position_++;
	byte1 = *position_++;
	remaining_ -= 2;

	_value = VSOMEIP_BYTES_TO_WORD(byte0, byte1);

	return true;
}

bool deserializer::deserialize(uint32_t &_value, bool _omit_last_byte) {
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

bool deserializer::deserialize(uint8_t *_data, std::size_t _length) {
	if (_length > remaining_)
		return false;

	std::memcpy(_data, &data_[position_ - data_.begin()], _length);
	position_ += _length;
	remaining_ -= _length;

	return true;
}

bool deserializer::deserialize(std::vector< uint8_t >& _value) {
	if (_value.capacity() > remaining_)
		return false;

	_value.assign(position_, position_ + _value.capacity());
	position_ += _value.capacity();
	remaining_ -= _value.capacity();

	return true;
}

bool deserializer::look_ahead(std::size_t _index, uint8_t &_value) const {
	if (_index >= data_.size())
		return false;

	_value = *(position_ + _index);

	return true;
}

bool deserializer::look_ahead(std::size_t _index, uint16_t &_value) const {
	if (_index+1 >= data_.size())
		return false;

	std::vector< uint8_t >::iterator i = position_ + _index;
	_value = VSOMEIP_BYTES_TO_WORD(*i, *(i+1));

	return true;
}

bool deserializer::look_ahead(std::size_t _index, uint32_t &_value) const {
	if (_index+3 >= data_.size())
		return false;

	std::vector< uint8_t >::const_iterator i = position_ + _index;
	_value = VSOMEIP_BYTES_TO_LONG(*i, *(i+1), *(i+2), *(i+3));

	return true;
}

message * deserializer::deserialize_message() {
	message_impl* deserialized_message = new message_impl;
	if (0 != deserialized_message) {
		if (false == deserialized_message->deserialize(this)) {
			delete deserialized_message;
			deserialized_message = 0;
		}
	}

	return deserialized_message;
}

void deserializer::set_data(const byte_t *_data,  std::size_t _length) {
	if (0 != _data) {
		data_.assign(_data, _data + _length);
		position_ = data_.begin();
		remaining_ = data_.end() - position_;
	} else {
		data_.clear();
		position_ = data_.end();
		remaining_ = 0;
	}
}

void deserializer::append_data(const byte_t *_data, std::size_t _length) {
	std::size_t offset = (position_ - data_.begin());
	data_.insert(data_.end(), _data, _data + _length);
	position_ = data_.begin() + offset;
	remaining_ += _length;
}

void deserializer::drop_data(std::size_t _length) {
	if (position_ + _length < data_.end())
		position_ += _length;
	else
		position_ = data_.end();
}

void deserializer::reset() {
	data_.erase(data_.begin(), position_);
	position_ = data_.begin();
	remaining_ = data_.size();
}

#ifdef VSOMEIP_DEBUGGING
void deserializer::show() const {
	std::stringstream its_message;
	its_message << "("
			<< std::hex << std::setw(2) << std::setfill('0')
			<< (int)*position_ << ", "
			<< std:: dec << remaining_ << ") ";
	for (int i = 0; i < data_.size(); ++i)
		its_message << std::hex << std::setw(2) << std::setfill('0')
					<< (int)data_[i] << " ";
	VSOMEIP_DEBUG << its_message;
}
#endif

} // namespace vsomeip
