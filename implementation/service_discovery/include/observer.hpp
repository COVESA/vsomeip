// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/asio/ip/address.hpp>
#include <mutex>
#include <unordered_map>

#if VSOMEIP_BOOST_VERSION < 107600
#include <boost/container_hash/hash.hpp>
#endif

namespace vsomeip_v3 {

#if VSOMEIP_BOOST_VERSION < 107600
// Custom hash function for boost::asio::ip::address.
//
// Used because there is no hash implementation before Boost v1.76.
struct address_hash {
    size_t operator()(const boost::asio::ip::address& addr) const noexcept {
        return addr.is_v4() ? boost::hash<boost::asio::ip::address_v4::bytes_type>()(addr.to_v4().to_bytes())
                            : boost::hash<boost::asio::ip::address_v6::bytes_type>()(addr.to_v6().to_bytes());
    }
};
#endif // VSOMEIP_BOOST_VERSION < 107600

/// Records observation of messages from network hosts.
class observer {
public:
    /// Records that a message has been observed from the given `_host`.
    ///
    /// Returns `true` if the host was already observed before.
    bool observe(const boost::asio::ip::address& _host, bool _is_multicast);

    /// Clears all records of observed hosts.
    void clear();

private:
    /// Record of host observations.
    struct record {
        /// Whether the host was observed through unicast.
        bool unicast;
        /// Whether the host was observed through mulicast.
        bool multicast;
    };

    /// List of hosts from which we have received unicast and multicast messages.
#if VSOMEIP_BOOST_VERSION < 107600
    std::unordered_map<boost::asio::ip::address, record, address_hash> hosts_;
#else
    std::unordered_map<boost::asio::ip::address, record> hosts_;
#endif

    /// Synchronizes access to this object.
    std::mutex mutex_;
};

} // namespace vsomeip_v3
