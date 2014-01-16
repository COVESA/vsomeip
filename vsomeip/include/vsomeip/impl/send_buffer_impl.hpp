//
// send_buffer_impl.hpp
//
// Date: 	Jan 16, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_SEND_BUFFER_IMPL_HPP
#define VSOMEIP_IMPL_SEND_BUFFER_IMPL_HPP

#include <vector>

namespace vsomeip {

class send_buffer_impl {
public:
	send_buffer_impl()
		: flush_(false) {};
	send_buffer_impl(const send_buffer_impl &_send_buffer)
		: flush_(_send_buffer.flush_), data_(_send_buffer.data_) {};

public:
	bool flush_;
	std::vector< uint8_t > data_;
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_SEND_BUFFER_IMPL_HPP
