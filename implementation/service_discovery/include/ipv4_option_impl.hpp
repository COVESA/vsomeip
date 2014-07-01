// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_IPV4_OPTION_IMPL_HPP
#define VSOMEIP_SD_IPV4_OPTION_IMPL_HPP

#include "option_impl.hpp"

namespace vsomeip {
namespace sd {

class ipv4_option_impl: public option_impl {
public:
	ipv4_option_impl(bool _is_multicast);
	virtual ~ipv4_option_impl();
	bool operator == (const option_impl &_option) const;

	bool is_multicast() const;

	bool serialize(vsomeip::serializer *_to) const;
	bool deserialize(vsomeip::deserializer *_from);

protected:
	// TODO: add endpoint description based on boost classes
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_IPV4_OPTION_IMPL_HPP
