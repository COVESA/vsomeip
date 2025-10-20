// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <common/policy.hpp>
#include <filesystem>
#include <vector>
#include <iostream>

#include <boost/property_tree/json_parser.hpp>
#include "../../../implementation/configuration/include/configuration_element.hpp"
#include "../../../implementation/security/include/policy_manager_impl.hpp"
#include "../../../implementation/utility/include/utility.hpp"

void common::load_policy_data(const std::string& _input, std::vector<vsomeip_v3::configuration_element>& _elements,
                              std::set<std::string>& _failed) {

    boost::property_tree::ptree its_tree;
    try {
        boost::property_tree::json_parser::read_json(_input, its_tree);
        _elements.emplace_back(_input, its_tree);
    } catch (boost::property_tree::json_parser_error& e) {
        _failed.insert(_input);
    }
}

void common::read_data(const std::set<std::string>& _input, std::vector<vsomeip_v3::configuration_element>& _elements,
                       std::set<std::string>& _failed) {

    for (const auto& i : _input) {
        if (vsomeip_v3::utility::is_file(i)) {
            load_policy_data(i, _elements, _failed);
        } else if (vsomeip_v3::utility::is_folder(i)) {
            std::map<std::string, bool> its_names;
            std::filesystem::path its_path(i);
            for (auto j = std::filesystem::directory_iterator(its_path); j != std::filesystem::directory_iterator(); j++) {
                std::string name = j->path().string() + "/vsomeip_security.json";
                if (vsomeip_v3::utility::is_file(name)) {
                    its_names[name] = true;
                }
            }

            for (const auto& n : its_names) {
                load_policy_data(n.first, _elements, _failed);
            }
        }
    }
}

std::set<std::string> common::get_all_files_in_dir(const std::string& _dir_path, const std::vector<std::string>& _dir_skip_list) {

    // Create a vector of string
    std::set<std::string> list_of_files;
    try {
        // Check if given path exists and points to a directory
        if (std::filesystem::exists(_dir_path) && std::filesystem::is_directory(_dir_path)) {
            // Create a Recursive Directory Iterator object and points to the
            // starting of directory
            std::filesystem::recursive_directory_iterator iter(_dir_path);
            // Create a Recursive Directory Iterator object pointing to end.
            std::filesystem::recursive_directory_iterator end;
            // Iterate till end
            while (iter != end) {
                // Check if current entry is a directory and if exists in
                // skip list
                if (std::filesystem::is_directory(iter->path())
                    && (std::find(_dir_skip_list.begin(), _dir_skip_list.end(), iter->path().filename()) != _dir_skip_list.end())) {
                    // Boost Filesystem  API to skip current directory iteration
                    iter.disable_recursion_pending();
                } else {
                    // Add the name in vector
                    list_of_files.insert(iter->path().string());
                }
                std::error_code ec;
                // Increment the iterator to point to next entry in recursive iteration
                iter.increment(ec);
                if (ec) {
                    std::cerr << "Error While Accessing : " << iter->path().string() << " :: " << ec.message() << '\n';
                }
            }
        }
    } catch (std::system_error& e) {
        std::cerr << "Exception :: " << e.what();
    }
    return list_of_files;
}

std::string common::get_policies_path() {
    return std::filesystem::canonical(std::filesystem::current_path()).string() + "/test/common/examples_policies";
}

vsomeip_sec_client_t common::create_uds_client(uid_t user, gid_t group, vsomeip_sec_ip_addr_t host) {
    vsomeip_sec_client_t result{user, group, host, VSOMEIP_SEC_PORT_UNUSED};
    return result;
}

void common::force_check_credentials(std::vector<vsomeip_v3::configuration_element>& _policy_elements, const std::string& _value) {

    for (auto& i : _policy_elements) {
        try {
            boost::property_tree::ptree& security = i.tree_.get_child("security");
            boost::property_tree::ptree& credentials = security.get_child("check_credentials");
            if (credentials.get_value<std::string>() != _value) {
                security.erase("check_credentials");
                security.put("check_credentials", _value);
            }
        } catch (...) { }
    }
}

void common::get_policy_uids(vsomeip_v3::configuration_element& _policy_element, std::vector<vsomeip_v3::uid_t>& _out_uids) {
    try {
        std::vector<std::string> user_ids;
        auto policy_tree = _policy_element.tree_.get_child("security.policies");
        for (auto policy_node : policy_tree) {
            auto optional_credential_node = policy_node.second.get_child_optional("credentials.uid");
            if (optional_credential_node) {
                auto optional_user_id = optional_credential_node.get().get_value_optional<std::string>();
                if (optional_user_id) {
                    user_ids.push_back(optional_user_id.get());
                }
            }
        }
        for (const std::string& uid_string : user_ids) {
            _out_uids.push_back(static_cast<vsomeip_v3::uid_t>(std::strtoul(uid_string.c_str(), nullptr, 0)));
        }
    } catch (...) {
        std::cerr << "Caught exception while reading user ids in policy element \"" << _policy_element.name_ << "\"!" << std::endl;
    }
}

void common::get_policy_services(vsomeip_v3::configuration_element& _policy_element, std::vector<vsomeip_v3::service_t>& _out_services) {
    try {
        std::vector<std::string> services;
        auto policy_tree = _policy_element.tree_.get_child("security.policies");
        for (auto policy_node : policy_tree) {
            // Get allowed request services.
            auto allow_requests = policy_node.second.get_child_optional("allow.requests");
            if (allow_requests) {
                for (auto& request_node : allow_requests.get()) {
                    auto optional_service = request_node.second.get_child("service").get_value_optional<std::string>();
                    if (optional_service) {
                        services.push_back(optional_service.get());
                    }
                }
            }
            // Get denied request services.
            auto deny_requests = policy_node.second.get_child_optional("deny.requests");
            if (deny_requests) {
                for (auto& request_node : deny_requests.get()) {
                    auto optional_service = request_node.second.get_child("service").get_value_optional<std::string>();
                    if (optional_service) {
                        services.push_back(optional_service.get());
                    }
                }
            }
        }
        for (const std::string& service_str : services) {
            _out_services.push_back(static_cast<vsomeip_v3::service_t>(std::strtoul(service_str.c_str(), nullptr, 0)));
        }
    } catch (...) {
        std::cerr << "Caught exception while reading services in policy element \"" << _policy_element.name_ << "\"!" << std::endl;
    }
}

void common::add_security_whitelist(vsomeip_v3::configuration_element& _policy_element, const bool _check_whitelist) {
    std::vector<vsomeip_v3::uid_t> user_ids;
    get_policy_uids(_policy_element, user_ids);

    std::vector<vsomeip_v3::service_t> services;
    get_policy_services(_policy_element, services);

    add_security_whitelist(_policy_element, user_ids, services, _check_whitelist);
}

void common::add_security_whitelist(vsomeip_v3::configuration_element& _policy_element, const std::vector<vsomeip_v3::uid_t>& _user_ids,
                                    const std::vector<vsomeip_v3::service_t>& _services, const bool _check_whitelist) {
    // Add the user ids to the whitelist.
    boost::property_tree::ptree id_array_node;
    for (auto user_id : _user_ids) {
        boost::property_tree::ptree id_node;
        id_node.put("", user_id);
        id_array_node.push_back(std::make_pair("", id_node));
    }
    _policy_element.tree_.add_child("security-update-whitelist.uids", id_array_node);

    // Add the services to the whitelist.
    boost::property_tree::ptree service_array_node;
    for (auto service : _services) {
        boost::property_tree::ptree service_node;
        service_node.put("", service);
        service_array_node.push_back(std::make_pair("", service_node));
    }
    _policy_element.tree_.add_child("security-update-whitelist.services", service_array_node);

    // Update the 'check_whitelist' flag.
    _policy_element.tree_.add<bool>("security-update-whitelist.check-whitelist", _check_whitelist);
}
