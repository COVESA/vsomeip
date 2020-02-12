// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../../e2e_protection/include/e2e/profile/e2e_provider_impl.hpp"

#include "../../../../e2e_protection/include/e2e/profile/profile01/checker.hpp"
#include "../../../../e2e_protection/include/e2e/profile/profile01/profile_01.hpp"
#include "../../../../e2e_protection/include/e2e/profile/profile01/protector.hpp"

#include "../../../../e2e_protection/include/e2e/profile/profile_custom/checker.hpp"
#include "../../../../e2e_protection/include/e2e/profile/profile_custom/profile_custom.hpp"
#include "../../../../e2e_protection/include/e2e/profile/profile_custom/protector.hpp"

#include <sstream>

namespace {

template<typename value_t>
value_t read_value_from_config(const std::shared_ptr<vsomeip_v3::cfg::e2e>& config,
                               const std::string& name,
                               value_t default_value = value_t()) {
    if(config && config->custom_parameters.count(name)) {
        value_t value;
        std::stringstream its_converter;
        its_converter << config->custom_parameters[name];
        its_converter >> value;
        return value;
    }

    return default_value;
}

} // namespace


VSOMEIP_PLUGIN(vsomeip_v3::e2e::e2e_provider_impl)

namespace vsomeip_v3 {
namespace e2e {

e2e_provider_impl::e2e_provider_impl()
    : plugin_impl("vsomeip e2e plugin", 1, plugin_type_e::APPLICATION_PLUGIN)
{
}

e2e_provider_impl::~e2e_provider_impl()
{
}

bool e2e_provider_impl::add_configuration(std::shared_ptr<cfg::e2e> config)
{
    if(config->profile == "CRC8") {
        process_e2e_profile<profile01::profile_config, profile01::profile_01_checker, profile01::protector>(config);
        return true;
    }

    if(config->profile == "CRC32") {
        process_e2e_profile<profile_custom::profile_config, profile_custom::profile_custom_checker, profile_custom::protector>(config);
        return true;
    }

    return false;
}

bool e2e_provider_impl::is_protected(e2exf::data_identifier_t id) const
{
    return custom_protectors.count(id) > 0;
}

bool e2e_provider_impl::is_checked(e2exf::data_identifier_t id) const
{
    return custom_checkers.count(id) > 0;
}

void e2e_provider_impl::protect(e2exf::data_identifier_t id, e2e_buffer &_buffer)
{
    auto protector = custom_protectors.find(id);
    if(protector != custom_protectors.end()) {
        protector->second->protect(_buffer);
    }
}

void e2e_provider_impl::check(e2exf::data_identifier_t id, const e2e_buffer &_buffer,
                              profile_interface::check_status_t &_generic_check_status)
{
    auto checker = custom_checkers.find(id);
    if(checker != custom_checkers.end()) {
        checker->second->check(_buffer, _generic_check_status);
    }
}

template<>
vsomeip_v3::e2e::profile01::profile_config
e2e_provider_impl::make_e2e_profile_config(const std::shared_ptr<cfg::e2e>& config) {
    uint16_t crc_offset = read_value_from_config<uint16_t>(config, "crc_offset");
    uint16_t data_length = read_value_from_config<uint16_t>(config, "data_length");

    // counter field behind CRC8
    uint16_t counter_offset = read_value_from_config<uint16_t>(config, "counter_offset", 8);

    // data id nibble behind 4 bit counter value
    uint16_t data_id_nibble_offset = read_value_from_config<uint16_t>(config, "data_id_nibble_offset", 12);

    e2e::profile01::p01_data_id_mode data_id_mode =
        static_cast<e2e::profile01::p01_data_id_mode>(
            read_value_from_config<uint16_t>(config, "data_id_mode"));

    return e2e::profile01::profile_config(crc_offset, config->data_id, data_id_mode,
        data_length, counter_offset, data_id_nibble_offset);
}

template<>
e2e::profile_custom::profile_config
e2e_provider_impl::make_e2e_profile_config(const std::shared_ptr<cfg::e2e>& config) {
    uint16_t crc_offset = read_value_from_config<uint16_t>(config, "crc_offset");
    return e2e::profile_custom::profile_config(crc_offset);
}

} // namespace e2e
} // namespace vsomeip_v3
