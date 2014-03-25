//
// protection_option_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/serializer.hpp>
#include <vsomeip/deserializer.hpp>
#include <vsomeip_internal/service_discovery/protection_option_impl.hpp>

namespace vsomeip {
namespace service_discovery {

protection_option_impl::protection_option_impl() {
	length_ = 1 + 4 + 4;
	type_ = option_type::PROTECTION;
	counter_ = 0;
	crc_ = 0;
}

protection_option_impl::~protection_option_impl() {
}

bool protection_option_impl::operator ==(const option &_other) const {
	if (_other.get_type() != option_type::PROTECTION)
		return false;

	const protection_option_impl& other
		= dynamic_cast< const protection_option_impl & >(_other);

	return (counter_ == other.counter_
		 && crc_ == other.crc_);
}

alive_counter protection_option_impl::get_alive_counter() const {
	return counter_;
}

void protection_option_impl::set_alive_counter(alive_counter _counter) {
	counter_ = _counter;
}

crc protection_option_impl::get_crc() const {
	return crc_;
}

void protection_option_impl::set_crc(crc _crc) {
	crc_ = _crc;
}

bool protection_option_impl::serialize(vsomeip::serializer *_to) const {
	bool is_successful = option_impl::serialize(_to);
	is_successful = is_successful && _to->serialize(static_cast<uint32_t>(counter_));
	is_successful = is_successful && _to->serialize(static_cast<uint32_t>(crc_));
	return is_successful;
}

bool protection_option_impl::deserialize(vsomeip::deserializer *_from) {
	bool is_successful = option_impl::deserialize(_from);

	uint32_t tmp_alive_counter = 0;
	is_successful = is_successful && _from->deserialize(tmp_alive_counter);
	counter_ = static_cast<alive_counter>(tmp_alive_counter);

	uint32_t tmp_crc = 0;
	is_successful = is_successful && _from->deserialize(tmp_crc);
	crc_ = static_cast<crc>(tmp_crc);

	return is_successful;
}

} // namespace service_discovery
} // namespace vsomeip
