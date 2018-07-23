// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../../../e2e_protection/include/e2e/profile/profile_factory.hpp"

#include "../../../../e2e_protection/include/e2e/profile/profile01/checker.hpp"
#include "../../../../e2e_protection/include/e2e/profile/profile01/profile_01.hpp"
#include "../../../../e2e_protection/include/e2e/profile/profile01/protector.hpp"

#include "../../../../e2e_protection/include/e2e/profile/profile_custom/checker.hpp"
#include "../../../../e2e_protection/include/e2e/profile/profile_custom/profile_custom.hpp"
#include "../../../../e2e_protection/include/e2e/profile/profile_custom/protector.hpp"

#include <sstream>

namespace {

template<typename value_t>
value_t read_value_from_config(std::shared_ptr<vsomeip::cfg::e2e> config,
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


template<typename config_t>
config_t make_e2e_profile_config(std::shared_ptr<vsomeip::cfg::e2e>);

template<>
vsomeip::e2e::profile01::profile_config
make_e2e_profile_config(std::shared_ptr<vsomeip::cfg::e2e> config) {
    uint16_t crc_offset = read_value_from_config<uint16_t>(config, "crc_offset");
    uint16_t data_length = read_value_from_config<uint16_t>(config, "data_length");

    // counter field behind CRC8
    uint16_t counter_offset = read_value_from_config<uint16_t>(config, "counter_offset", 8);

    // data id nibble behind 4 bit counter value
    uint16_t data_id_nibble_offset = read_value_from_config<uint16_t>(config, "data_id_nibble_offset", 12);

    vsomeip::e2e::profile01::p01_data_id_mode data_id_mode =
        static_cast<vsomeip::e2e::profile01::p01_data_id_mode>(
            read_value_from_config<uint8_t>(config, "data_id_mode"));

    return vsomeip::e2e::profile01::profile_config(crc_offset, config->data_id, data_id_mode,
        data_length, counter_offset, data_id_nibble_offset);
}

template<>
vsomeip::e2e::profile_custom::profile_config
make_e2e_profile_config(std::shared_ptr<vsomeip::cfg::e2e> config) {
    uint16_t crc_offset = read_value_from_config<uint16_t>(config, "crc_offset");
    return vsomeip::e2e::profile_custom::profile_config(crc_offset);
}


template<typename config_t, typename checker_t, typename protector_t>
std::tuple<std::shared_ptr<vsomeip::e2e::profile_interface::checker>,
           std::shared_ptr<vsomeip::e2e::profile_interface::protector>>
process_e2e_profile(std::shared_ptr<vsomeip::cfg::e2e> config) {
    config_t profile_config = make_e2e_profile_config<config_t>(config);

    std::shared_ptr<vsomeip::e2e::profile_interface::checker> checker;
    if ((config->variant == "checker") || (config->variant == "both")) {
        checker = std::make_shared<checker_t>(profile_config);
    }

    std::shared_ptr<vsomeip::e2e::profile_interface::protector> protector;
    if ((config->variant == "protector") || (config->variant == "both")) {
        protector = std::make_shared<protector_t>(profile_config);
    }

    return std::make_tuple(checker, protector);
}


} // namespace

VSOMEIP_PLUGIN(vsomeip::e2e::profile_factory)

namespace vsomeip {
namespace e2e {

profile_factory::profile_factory()
    : plugin_impl("vsomeip e2e plugin", 1, plugin_type_e::APPLICATION_PLUGIN)
{
}

profile_factory::~profile_factory()
{
}

bool profile_factory::is_e2e_profile_supported(const std::string& name) const {
    return (name == "CRC8") || (name == "CRC32");
}

std::tuple<std::shared_ptr<profile_interface::checker>, std::shared_ptr<profile_interface::protector>>
profile_factory::process_e2e_config(std::shared_ptr<cfg::e2e> config) const {
    if(config->profile == "CRC8") {
        return process_e2e_profile<profile01::profile_config, profile01::profile_01_checker, profile01::protector>(config);
    }

    if(config->profile == "CRC32") {
        return process_e2e_profile<profile_custom::profile_config,
                                   profile_custom::profile_custom_checker,
                                   profile_custom::protector>(config);
    }

    return std::make_tuple(nullptr, nullptr);
}




} // namespace e2e
} // namespace vsomeip
