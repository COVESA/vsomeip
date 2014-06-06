//
// statistics_owner.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2024 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_STATISTICS_OWNER_HPP
#define VSOMEIP_STATISTICS_OWNER_HPP

namespace vsomeip {

/// Basic interface for all vsomeip library interfaces that contains
/// informations about the number of messages/bytes it has sent and
/// received.
class statistics_owner {
public:
	virtual ~statistics_owner() {};

    /// Get access to the statistics object.
    /// \returns Pointer to the contained statistics object.
	virtual const class statistics * get_statistics() const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_STATISTICS_OWNER_HPP
