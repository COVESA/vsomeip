// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICEINFO_HPP
#define VSOMEIP_SERVICEINFO_HPP

#include <memory>
#include <set>
#include <string>
#include <chrono>
#include <mutex>

#include <vsomeip/export.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;
class servicegroup;

class serviceinfo {
public:
    VSOMEIP_EXPORT serviceinfo(major_version_t _major, minor_version_t _minor,
            ttl_t _ttl, bool _is_local);
    VSOMEIP_EXPORT ~serviceinfo();

    VSOMEIP_EXPORT servicegroup * get_group() const;
    VSOMEIP_EXPORT void set_group(servicegroup *_group);

    VSOMEIP_EXPORT major_version_t get_major() const;
    VSOMEIP_EXPORT minor_version_t get_minor() const;

    VSOMEIP_EXPORT ttl_t get_ttl() const;
    VSOMEIP_EXPORT void set_ttl(ttl_t _ttl);

    VSOMEIP_EXPORT std::chrono::milliseconds get_precise_ttl() const;
    VSOMEIP_EXPORT void set_precise_ttl(std::chrono::milliseconds _ttl);

    VSOMEIP_EXPORT std::shared_ptr<endpoint> get_endpoint(bool _reliable) const;
    VSOMEIP_EXPORT void set_endpoint(std::shared_ptr<endpoint> _endpoint,
            bool _reliable);

    VSOMEIP_EXPORT void add_client(client_t _client);
    VSOMEIP_EXPORT void remove_client(client_t _client);
    VSOMEIP_EXPORT uint32_t get_requesters_size();

    VSOMEIP_EXPORT bool is_local() const;

    VSOMEIP_EXPORT bool is_in_mainphase() const;
    VSOMEIP_EXPORT void set_is_in_mainphase(bool _in_mainphase);

private:
    servicegroup *group_;

    major_version_t major_;
    minor_version_t minor_;

    mutable std::mutex ttl_mutex_;
    std::chrono::milliseconds ttl_;

    std::shared_ptr<endpoint> reliable_;
    std::shared_ptr<endpoint> unreliable_;

    mutable std::mutex endpoint_mutex_;
    std::mutex requesters_mutex_;
    std::set<client_t> requesters_;

    bool is_local_;
    bool is_in_mainphase_;
};

}  // namespace vsomeip

#endif // VSOMEIP_SERVICEINFO_HPP
