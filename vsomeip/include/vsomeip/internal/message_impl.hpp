//
// message_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_MESSAGE_IMPL_HPP
#define VSOMEIP_INTERNAL_MESSAGE_IMPL_HPP

#include <vsomeip/internal/message_base_impl.hpp>
#include <vsomeip/internal/payload_impl.hpp>

namespace vsomeip {

class message_impl
		: virtual public message,
		  virtual public message_base_impl {
public:
	virtual ~message_impl();

	virtual length get_length() const;
	virtual void set_length(length _length);
	virtual payload & get_payload();

	virtual bool serialize(serializer *_to) const;
	virtual bool deserialize(deserializer *_from);

protected: // members
	payload_impl payload_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_MESSAGE_IMPL_HPP
