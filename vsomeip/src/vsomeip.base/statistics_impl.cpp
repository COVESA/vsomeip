//
// statistics_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#ifdef USE_VSOMEIP_STATISTICS

#include <vsomeip_internal/statistics_impl.hpp>

namespace vsomeip {

statistics_impl::statistics_impl()
	: sent_messages_(0), sent_bytes_(0), received_messages_(0), received_bytes_(0) {
}

statistics_impl::~statistics_impl() {
}

uint32_t statistics_impl::get_sent_messages_count() const {
	return sent_messages_;
}

uint32_t statistics_impl::get_sent_bytes_count() const {
	return sent_bytes_;
}

uint32_t statistics_impl::get_received_messages_count() const {
	return received_messages_;
}

uint32_t statistics_impl::get_received_bytes_count() const {
	return received_bytes_;
}

void statistics_impl::reset() {
	sent_messages_ = 0;
	sent_bytes_ = 0;
	received_messages_ = 0;
	received_bytes_ = 0;
}

} // namespace vsomeip

#endif // USE_VSOMEIP_STATISTICS
