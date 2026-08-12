// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <atomic>
#include <memory>
#include <set>
#include <string>
#include <chrono>
#include <mutex>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {

class boardnet_endpoint;

class serviceinfo {
public:
    serviceinfo(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor, ttl_t _ttl, bool _is_local);
    serviceinfo(const serviceinfo& _other);
    ~serviceinfo();

    service_t get_service() const;
    instance_t get_instance() const;

    major_version_t get_major() const;
    minor_version_t get_minor() const;

    ttl_t get_ttl() const;
    void set_ttl(ttl_t _ttl);

    std::chrono::milliseconds get_precise_ttl() const;
    void set_precise_ttl(std::chrono::milliseconds _precise_ttl);

    std::shared_ptr<boardnet_endpoint> get_endpoint(bool _reliable) const;
    void set_endpoint(const std::shared_ptr<boardnet_endpoint>& _endpoint, bool _reliable);

    bool is_local() const;

    bool is_ready_to_offer() const;
    void set_endpoint_requirements(bool _needs_reliable, bool _needs_unreliable);

    bool is_in_mainphase() const;
    void set_is_in_mainphase(bool _in_mainphase);

private:
    mutable std::mutex mutex_;

    const service_t service_;
    const instance_t instance_;

    const major_version_t major_;
    const minor_version_t minor_;

    const bool is_local_;

    std::chrono::milliseconds ttl_;

    std::shared_ptr<boardnet_endpoint> reliable_;
    std::shared_ptr<boardnet_endpoint> unreliable_;

    std::atomic_bool needs_reliable_;
    std::atomic_bool needs_unreliable_;
    std::atomic_bool is_in_mainphase_;
};

} // namespace vsomeip_v3
