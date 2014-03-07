//
// statistics_owner_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2024 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_STATISTICS_OWNER_IMPL_HPP
#define VSOMEIP_INTERNAL_STATISTICS_OWNER_IMPL_HPP

#ifdef USE_VSOMEIP_STATISTICS
#include <vsomeip_internal/statistics_owner.hpp>
#include <vsomeip_internal/statistics_impl.hpp>

namespace vsomeip {

class statistics_owner_impl
			: virtual public statistics_owner {
protected:
	virtual ~statistics_owner_impl();

public:
	const statistics * get_statistics() const;

protected:
	statistics_impl statistics_;
};

} // namespace vsomeip
#endif // USE_VSOMEIP_STATISTICS
#endif // VSOMEIP_INTERNAL_STATISTICS_OWNER_IMPL_HPP
