// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/servicegroup.hpp"

namespace vsomeip {

servicegroup::servicegroup(const std::string &_name)
	: name_(_name) {
}

servicegroup::~servicegroup() {
}

std::string servicegroup::get_name() const {
	return name_;
}

void servicegroup::add_service(std::shared_ptr< serviceinfo > _service) {
	services_.insert(_service);
}

void servicegroup::remove_service(std::shared_ptr< serviceinfo > _service) {
	services_.erase(_service);
}

} // namespace vsomeip



