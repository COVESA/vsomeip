// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_E2E_PROFILE_FACTORY_IMPL_HPP
#define VSOMEIP_E2E_PROFILE_FACTORY_IMPL_HPP

#include "./profile_factory.hpp"
#include "../../../../../interface/vsomeip/export.hpp"
#include "../../../../../interface/vsomeip/plugin.hpp"

namespace vsomeip {
namespace e2e {

class profile_factory_impl:
		public profile_factory,
        public plugin_impl<profile_factory_impl>,
        public std::enable_shared_from_this<profile_factory_impl> {
public:
	VSOMEIP_EXPORT profile_factory_impl();
	VSOMEIP_EXPORT ~profile_factory_impl() override;

	VSOMEIP_EXPORT bool is_e2e_profile_supported(const std::string& name) const override;

	VSOMEIP_EXPORT
	std::tuple<std::shared_ptr<profile_interface::checker>, std::shared_ptr<profile_interface::protector>>
	process_e2e_config(std::shared_ptr<cfg::e2e> config) const override;
};

} // namespace e2e
} // namespace vsomeip

#endif	// VSOMEIP_E2E_PROFILE_FACTORY_IMPL_HPP
