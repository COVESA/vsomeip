//
// statistics_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_STATISTICS_IMPL_HPP
#define VSOMEIP_INTERNAL_STATISTICS_IMPL_HPP

#ifdef USE_VSOMEIP_STATISTICS

#include <cstdint>
#include <vsomeip/statistics.hpp>

namespace vsomeip {

class statistics_impl : public statistics {
public:
	statistics_impl();
	virtual ~statistics_impl();

	uint32_t get_sent_messages_count() const;
	uint32_t get_sent_bytes_count() const;
	uint32_t get_received_messages_count() const;
	uint32_t get_received_bytes_count() const;

	void reset();

	uint32_t sent_messages_;
	uint32_t sent_bytes_;

	uint32_t received_messages_;
	uint32_t received_bytes_;
};

} // namespace vsomeip

#endif // USE_VSOMEIP_STATISTICS

#endif // VSOMEIP_INTERNAL_STATISTICS_IMPL_HPP
