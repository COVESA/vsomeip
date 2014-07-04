// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_RUNTIME_IMPL_HPP
#define VSOMEIP_RUNTIME_IMPL_HPP

#include <vsomeip/runtime.hpp>

namespace vsomeip {

class runtime_impl: public runtime {
public:
	static runtime * get();

	virtual ~runtime_impl();

	std::shared_ptr< application > create_application(const std::string &_name) const;

	std::shared_ptr< message > create_request() const;
	std::shared_ptr< message > create_response(const std::shared_ptr< message > &_request) const;

	std::shared_ptr< message > create_notification() const;

	std::shared_ptr< payload > create_payload() const;
};

} // namespace vsomeip

#endif // VSOMEIP_RUNTIME_IMPL_HPP
