// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_E2E_PROFILE_FACTORY_HPP
#define VSOMEIP_E2E_PROFILE_FACTORY_HPP

#include <memory>
#include <string>
#include <tuple>

#include "profile_interface/protector.hpp"
#include "profile_interface/checker.hpp"
#include "../../../../configuration/include/e2e.hpp"

namespace vsomeip {
namespace e2e {

class profile_factory {
public:
	virtual ~profile_factory() = 0;

	virtual bool is_e2e_profile_supported(const std::string& name) const = 0;

	virtual
	std::tuple<std::shared_ptr<profile_interface::checker>, std::shared_ptr<profile_interface::protector>>
	process_e2e_config(std::shared_ptr<cfg::e2e> config) const = 0;
};

} // namespace e2e
} // namespace vsomeip

#endif	// VSOMEIP_E2E_PROFILE_FACTORY_HPP
