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

/// Interface for sender/receiver statistics. Instances are embedded into
/// #statistics_owner instances and provide methods to retrieve the current
/// number of sent/received messages and bytes.
class statistics {
public:
    /// Intentionally undocumented.
	virtual ~statistics() {};

    /// Get the number of messages that were sent at the time of calling.
    /// \returns number of sent messages
	virtual uint32_t get_sent_messages_count() const = 0;

    /// Get the number of bytes that were sent at the time of calling.
    /// \returns number of sent bytes
	virtual uint32_t get_sent_bytes_count() const = 0;

    /// Get the number of messages that were received at the time of calling.
    /// \returns number of received messages
	virtual uint32_t get_received_messages_count() const = 0;

    /// Get the number of bytes that were received at the time of calling.
    /// \returns number of received bytes
	virtual uint32_t get_received_bytes_count() const = 0;

    /// Set the counters for sent/received messages and bytes to 0.
	virtual void reset() = 0;
};

} // namespace vsomeip

#endif // USE_VSOMEIP_STATISTICS

#endif // VSOMEIP_STATISTICS_HPP
