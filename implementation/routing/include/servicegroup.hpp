// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICEGROUP_HPP
#define VSOMEIP_SERVICEGROUP_HPP

#include <memory>
#include <set>
#include <string>

namespace vsomeip {

class serviceinfo;

class servicegroup {
public:
	servicegroup(const std::string &_name);
	virtual ~servicegroup();

	std::string get_name() const;

	void add_service(std::shared_ptr< serviceinfo > _service);
	void remove_service(std::shared_ptr< serviceinfo > _service);

private:
	std::string name_;
	std::set< std::shared_ptr< serviceinfo > > services_;
};

} // namespace vsomeip

#endif // VSOMEIP_SERVICEGROUP_HPP
