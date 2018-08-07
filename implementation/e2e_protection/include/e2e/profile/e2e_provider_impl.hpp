// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_E2E_PROVIDER_IMPL_HPP
#define VSOMEIP_E2E_PROVIDER_IMPL_HPP

#include <map>
#include <memory>

#include "e2e_provider.hpp"
#include "profile_interface/checker.hpp"
#include "profile_interface/protector.hpp"

#include "../../../../../interface/vsomeip/export.hpp"
#include "../../../../../interface/vsomeip/plugin.hpp"

namespace vsomeip {
namespace e2e {

class e2e_provider_impl :
        public e2e_provider,
        public plugin_impl<e2e_provider_impl>,
        public std::enable_shared_from_this<e2e_provider_impl> {
public:
    VSOMEIP_EXPORT e2e_provider_impl();
    VSOMEIP_EXPORT ~e2e_provider_impl() override;

    VSOMEIP_EXPORT bool add_configuration(std::shared_ptr<cfg::e2e> config) override;

    VSOMEIP_EXPORT bool is_protected(e2exf::data_identifier id) const override;
    VSOMEIP_EXPORT bool is_checked(e2exf::data_identifier id) const override;

    VSOMEIP_EXPORT void protect(e2exf::data_identifier id, e2e_buffer &_buffer) override;
    VSOMEIP_EXPORT void check(e2exf::data_identifier id, const e2e_buffer &_buffer,
                              vsomeip::e2e::profile_interface::check_status_t &_generic_check_status) override;

private:
    std::map<e2exf::data_identifier, std::shared_ptr<e2e::profile_interface::protector>> custom_protectors;
    std::map<e2exf::data_identifier, std::shared_ptr<e2e::profile_interface::checker>> custom_checkers;

    template<typename config_t>
    config_t make_e2e_profile_config(std::shared_ptr<vsomeip::cfg::e2e>);

    template<typename config_t, typename checker_t, typename protector_t>
    void process_e2e_profile(std::shared_ptr<vsomeip::cfg::e2e> config) {
        const e2exf::data_identifier data_identifier = {config->service_id, config->event_id};
        config_t profile_config = make_e2e_profile_config<config_t>(config);

        std::shared_ptr<vsomeip::e2e::profile_interface::checker> checker;
        if ((config->variant == "checker") || (config->variant == "both")) {
            custom_checkers[data_identifier] = std::make_shared<checker_t>(profile_config);
        }

        std::shared_ptr<vsomeip::e2e::profile_interface::protector> protector;
        if ((config->variant == "protector") || (config->variant == "both")) {
            custom_protectors[data_identifier] = std::make_shared<protector_t>(profile_config);
        }
    }
};

} // namespace e2e
} // namespace vsomeip

#endif  // VSOMEIP_E2E_PROVIDER_IMPL_HPP

