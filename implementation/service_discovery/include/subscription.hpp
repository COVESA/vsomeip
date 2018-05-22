// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_SUBSCRIPTION_HPP
#define VSOMEIP_SD_SUBSCRIPTION_HPP

#include <memory>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>

namespace vsomeip {

class endpoint;

namespace sd {

class subscription {
public:
    subscription(major_version_t _major, ttl_t _ttl,
            std::shared_ptr<endpoint> _reliable,
            std::shared_ptr<endpoint> _unreliable,
            subscription_type_e _subscription_type,
            uint8_t _counter);
    ~subscription();

    major_version_t get_major() const;
    ttl_t get_ttl() const;
    void set_ttl(ttl_t _ttl);
    std::shared_ptr<endpoint> get_endpoint(bool _reliable) const;
    void set_endpoint(std::shared_ptr<endpoint> _endpoint, bool _reliable);

    bool is_acknowledged() const;
    void set_acknowledged(bool _is_acknowledged);

    bool is_tcp_connection_established() const;
    void set_tcp_connection_established(bool _is_established);

    bool is_udp_connection_established() const;
    void set_udp_connection_established(bool _is_established);

    subscription_type_e get_subscription_type() const;

    uint8_t get_counter() const;

private:
    major_version_t major_;
    ttl_t ttl_;

    std::shared_ptr<endpoint> reliable_;
    std::shared_ptr<endpoint> unreliable_;

    bool is_acknowledged_;
    bool tcp_connection_established_;
    bool udp_connection_established_;

    subscription_type_e subscription_type_;

    uint8_t counter_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_SUBSCRIPTION_HPP
