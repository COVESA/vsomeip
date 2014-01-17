//
// statistic_owner_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2024 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#include <vsomeip/impl/statistics_owner_impl.hpp>

namespace vsomeip {

statistics_owner_impl::~statistics_owner_impl() {
}

#ifdef USE_VSOMEIP_STATISTICS
const statistics * statistics_owner_impl::get_statistics() const {
	return &statistics_;
}
#endif

} // namespace vsomeip
