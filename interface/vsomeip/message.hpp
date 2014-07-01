// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_MESSAGE_HPP
#define VSOMEIP_MESSAGE_HPP

#include <vsomeip/message_base.hpp>

namespace vsomeip {

class payload;

class message
		: virtual public message_base {
public:
	virtual ~message() {};

	virtual payload & get_payload() = 0;
	virtual const payload & get_payload() const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_MESSAGE_HPP
