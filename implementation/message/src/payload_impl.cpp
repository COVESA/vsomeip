// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/deserializer.hpp"
#include "../include/payload_impl.hpp"
#include "../include/payload_owner.hpp"
#include "../include/serializer.hpp"

namespace vsomeip {

payload_impl::payload_impl()
	: data_(), owner_(0) {
}

payload_impl::payload_impl(const payload_owner *_owner)
	: data_(), owner_(_owner) {
}

payload_impl::payload_impl(const payload_impl& _payload)
	: data_(_payload.data_) {
}

payload_impl::~payload_impl() {
}

byte_t * payload_impl::get_data() {
	return data_.data();
}

const byte_t * payload_impl::get_data() const {
	return data_.data();
}

length_t payload_impl::get_length() const {
	return data_.size();
}

void payload_impl::set_capacity(length_t _capacity) {
	data_.reserve(_capacity);
}

void payload_impl::set_data(const byte_t *_data, const length_t _length) {
	bool is_changed = false;
	if (data_.size() != _length) {
		is_changed = true;
	} else {
		for (std::size_t i = 0; i < _length; ++i) {
			if (data_[i] != _data[i]) {
				is_changed = true;
				break;
			}
		}
	}
	data_.assign(_data, _data + _length);

	if (is_changed && owner_)
		owner_->notify();
}

void payload_impl::set_data(const std::vector< byte_t > &_data) {
	bool is_changed = false;
	if (data_.size() != _data.size()) {
		is_changed = true;
	} else {
		for (std::size_t i = 0; i < _data.size(); ++i) {
			if (data_[i] != _data[i]) {
				is_changed = true;
				break;
			}
		}
	}
	data_ = _data;

	if (is_changed && owner_)
		owner_->notify();
}

bool payload_impl::serialize(serializer *_to) const {
	return (0 != _to && _to->serialize(data_.data(), data_.size()));
}

bool payload_impl::deserialize(deserializer *_from) {
	return (0 != _from && _from->deserialize(data_));
}

} // namespace vsomeip
