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

#ifndef VSOMEIP_IMPL_STATISTICS_IMPL_HPP
#define VSOMEIP_IMPL_STATISTICS_IMPL_HPP

#ifdef USE_VSOMEIP_STATISTICS

#include <cstdint>
#include <vsomeip/statistics.hpp>

namespace vsomeip {

class statistics_impl : public statistics {

private:
	uint32_t sent_messages_;
	uint32_t sent_bytes_;

	uint32_t received_messages_;
	uint32_t received_bytes_;
};

} // namespace vsomeip

#endif // USE_VSOMEIP_STATISTICS

#endif // VSOMEIP_IMPL_STATISTICS_IMPL_HPP
