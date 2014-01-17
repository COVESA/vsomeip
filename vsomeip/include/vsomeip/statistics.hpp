//
// statistics.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_STATISTICS_HPP
#define VSOMEIP_STATISTICS_HPP

#ifdef USE_VSOMEIP_STATISTICS

namespace vsomeip {

class statistics {
	virtual get_sent_messages_count() const;
	virtual get_sent_bytes_count() const;
	virtual get_received_messages_count() const;
	virtual get_received_bytes_count() const;
};

} // namespace vsomeip

#endif // USE_VSOMEIP_STATISTICS

#endif // VSOMEIP_STATISTICS_HPP
