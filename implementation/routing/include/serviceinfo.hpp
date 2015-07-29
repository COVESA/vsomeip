// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICEINFO_HPP
#define VSOMEIP_SERVICEINFO_HPP

#include <memory>
#include <set>
#include <string>

#include <vsomeip/export.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;
class servicegroup;

class serviceinfo {
public:
    VSOMEIP_EXPORT serviceinfo(major_version_t _major, minor_version_t _minor,
            ttl_t _ttl);
    VSOMEIP_EXPORT ~serviceinfo();

    VSOMEIP_EXPORT servicegroup * get_group() const;
    VSOMEIP_EXPORT void set_group(servicegroup *_group);

    VSOMEIP_EXPORT major_version_t get_major() const;
    VSOMEIP_EXPORT minor_version_t get_minor() const;

    VSOMEIP_EXPORT ttl_t get_ttl() const;
    VSOMEIP_EXPORT void set_ttl(ttl_t _ttl);

    VSOMEIP_EXPORT std::shared_ptr<endpoint> get_endpoint(bool _reliable) const;
    VSOMEIP_EXPORT void set_endpoint(std::shared_ptr<endpoint> _endpoint,
            bool _reliable);

    VSOMEIP_EXPORT const std::string & get_multicast_address() const;
    VSOMEIP_EXPORT void set_multicast_address(const std::string &_multicast);

    VSOMEIP_EXPORT uint16_t get_multicast_port() const;
    VSOMEIP_EXPORT void set_multicast_port(uint16_t _port);

    VSOMEIP_EXPORT eventgroup_t get_multicast_group() const;
    VSOMEIP_EXPORT void set_multicast_group(eventgroup_t _multicast_group);

    VSOMEIP_EXPORT void add_client(client_t _client);
    VSOMEIP_EXPORT void remove_client(client_t _client);

private:
    servicegroup *group_;

    major_version_t major_;
    minor_version_t minor_;
    ttl_t ttl_;

    std::shared_ptr<endpoint> reliable_;
    std::shared_ptr<endpoint> unreliable_;

    std::string multicast_address_;
    uint16_t multicast_port_;
    eventgroup_t multicast_group_;

    std::set<client_t> requesters_;
};

}  // namespace vsomeip

#endif // VSOMEIP_SERVICEINFO_HPP
