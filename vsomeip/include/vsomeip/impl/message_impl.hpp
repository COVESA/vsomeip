//
// message_impl.hpp
//
// Date: 	Nov 28, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_MESSAGE_IMPL_HPP
#define VSOMEIP_MESSAGE_IMPL_HPP

#include <vsomeip/impl/message_base_impl.hpp>
#include <vsomeip/impl/payload_impl.hpp>

namespace vsomeip {

class message_impl
		: virtual public message,
		  virtual public message_base_impl {
public:
	virtual ~message_impl();

	virtual length get_length() const;
	virtual payload & get_payload();

	virtual bool serialize(serializer *_to) const;
	virtual bool deserialize(deserializer *_from);

protected: // members
	payload_impl payload_;
};

} // namespace vsomeip

#endif // VSOMEIP_MESSAGE_IMPL_HPP
