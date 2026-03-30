// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <vector>

#include <vsomeip/constants.hpp>
#include <vsomeip/primitive_types.hpp>

#include "internal.hpp"

namespace vsomeip_v3 {

class local_service_table {
public:
    struct entry {
        service_t service;
        instance_t instance;
        major_version_t major;
        minor_version_t minor;
        client_t client;
    };

    void add(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor, client_t _client) {
        services_[_service][_instance] = std::make_tuple(_major, _minor, _client);
    }

    bool remove(service_t _service, instance_t _instance) {
        auto found_service = services_.find(_service);
        if (found_service == services_.end()) {
            return false;
        }
        auto found_instance = found_service->second.find(_instance);
        if (found_instance == found_service->second.end()) {
            return false;
        }
        found_service->second.erase(found_instance);
        if (found_service->second.empty()) {
            services_.erase(found_service);
        }
        return true;
    }

    client_t find_client(service_t _service, instance_t _instance) const {
        const auto* t = find_tuple(_service, _instance);
        return t ? std::get<2>(*t) : static_cast<client_t>(VSOMEIP_ROUTING_CLIENT);
    }

    std::optional<entry> find_entry(service_t _service, instance_t _instance) const {
        const auto* t = find_tuple(_service, _instance);
        if (!t) {
            return std::nullopt;
        }
        return entry{_service, _instance, std::get<0>(*t), std::get<1>(*t), std::get<2>(*t)};
    }

    std::set<client_t> find_clients(service_t _service, instance_t _instance) const {
        std::set<client_t> clients;
        auto found_service = services_.find(_service);
        if (found_service == services_.end()) {
            return clients;
        }
        if (_instance == ANY_INSTANCE) {
            for (const auto& [inst, tuple] : found_service->second) {
                clients.insert(std::get<2>(tuple));
            }
        } else {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                clients.insert(std::get<2>(found_instance->second));
            }
        }
        return clients;
    }

    bool is_available(service_t _service, instance_t _instance, major_version_t _major) const {
        auto found_service = services_.find(_service);
        if (found_service == services_.end()) {
            return false;
        }
        if (_instance == ANY_INSTANCE) {
            return true;
        }
        auto found_instance = found_service->second.find(_instance);
        if (found_instance == found_service->second.end()) {
            return false;
        }
        return _major == ANY_MAJOR || _major == DEFAULT_MAJOR || std::get<0>(found_instance->second) == _major;
    }

    [[nodiscard]] std::vector<entry> remove_all_for_client(client_t _client) {
        std::vector<entry> removed;
        for (auto sit = services_.begin(); sit != services_.end();) {
            for (auto iit = sit->second.begin(); iit != sit->second.end();) {
                if (std::get<2>(iit->second) == _client) {
                    removed.push_back({sit->first, iit->first, std::get<0>(iit->second), std::get<1>(iit->second), _client});
                    iit = sit->second.erase(iit);
                } else {
                    ++iit;
                }
            }
            if (sit->second.empty()) {
                sit = services_.erase(sit);
            } else {
                ++sit;
            }
        }
        return removed;
    }
    [[nodiscard]] std::vector<entry> clear() {
        std::vector<entry> removed;
        for (const auto& [service, instances] : services_) {
            for (const auto& [instance, tuple] : instances) {
                removed.push_back({service, instance, std::get<0>(tuple), std::get<1>(tuple), std::get<2>(tuple)});
            }
        }
        services_.clear();
        return removed;
    }

private:
    using instance_map_t = std::map<instance_t, std::tuple<major_version_t, minor_version_t, client_t>>;
    std::map<service_t, instance_map_t> services_;

    const std::tuple<major_version_t, minor_version_t, client_t>* find_tuple(service_t _service, instance_t _instance) const {
        auto found_service = services_.find(_service);
        if (found_service == services_.end()) {
            return nullptr;
        }
        auto found_instance = found_service->second.find(_instance);
        if (found_instance == found_service->second.end()) {
            return nullptr;
        }
        return &found_instance->second;
    }
};

} // namespace vsomeip_v3
