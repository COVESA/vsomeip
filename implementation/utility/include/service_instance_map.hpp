// Copyright (C) 2016-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_SERVICEINSTANCEMAP_HPP
#define VSOMEIP_V3_SERVICEINSTANCEMAP_HPP

#include <boost/functional/hash.hpp>
#include <unordered_map>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {

struct service_instance_t {

    service_instance_t() = delete;

    constexpr service_instance_t(service_t _service, instance_t _instance)
        : service_{_service}, instance_{_instance} {}

    constexpr bool operator==(const service_instance_t& other) const {
        return service_ == other.service_ && instance_ == other.instance_;
    }

    constexpr bool operator!=(const service_instance_t& other) const {
        return (!(*this == other));
    }

    service_t service() const {
        return service_;
    }

    instance_t instance() const {
        return instance_;
    }

private:
    service_t service_;
    instance_t instance_;
};

template<class T>
using service_instance_map = std::unordered_map<service_instance_t, T>;

} // namespace vsomeip_v3

namespace std {
    template <>
    struct hash<vsomeip_v3::service_instance_t> {
        std::size_t operator()(const vsomeip_v3::service_instance_t& k) const {
            std::size_t seed = 0;
            boost::hash_combine(seed, k.service());
            boost::hash_combine(seed, k.instance());
            return seed;
        }
    };
} // namespace std

#endif // VSOMEIP_V3_SERVICE_INSTANCE_MAP_HPP
