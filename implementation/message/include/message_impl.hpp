// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_MESSAGE_IMPL_HPP
#define VSOMEIP_MESSAGE_IMPL_HPP

#include "message_base_impl.hpp"
#include "payload_impl.hpp"

namespace vsomeip {

class message_impl
		: virtual public message,
		  virtual public message_base_impl {
public:
	virtual ~message_impl();

	length_t get_length() const;
	void set_length(length_t _length);

	payload & get_payload();
	const payload & get_payload() const;

	bool serialize(serializer *_to) const;
	bool deserialize(deserializer *_from);

protected: // members
	payload_impl payload_;
};

} // namespace vsomeip

#endif // VSOMEIP_MESSAGE_IMPL_HPP
