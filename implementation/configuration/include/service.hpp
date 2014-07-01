// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CFG_SERVICE_HPP
#define VSOMEIP_CFG_SERVICE_HPP

namespace vsomeip {
namespace cfg {

struct service {
	service_t service_;
	instance_t instance_;

	uint16_t reliable_;

	uint16_t unreliable_;
	std::string multicast_;

	boost::shared_ptr< servicegroup > group_;
};

} // namespace cfg
} // namespace vsomeip

#endif // VSOMEIP_CFG_SERVICE_HPP
