// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_RUNTIME_HPP
#define VSOMEIP_RUNTIME_HPP

#include <memory>
#include <string>

namespace vsomeip {

class application;
class message;
class payload;

class runtime {
public:
	static runtime * get();

	virtual ~runtime() {};

	virtual std::shared_ptr< application > create_application(const std::string &_name) const = 0;

	virtual std::shared_ptr< message > create_request() const = 0;
	virtual std::shared_ptr< message > create_response(std::shared_ptr< message > &_request) const = 0;

	virtual std::shared_ptr< message > create_notification() const = 0;

	virtual std::shared_ptr< payload > create_payload() const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_RUNTIME_HPP
