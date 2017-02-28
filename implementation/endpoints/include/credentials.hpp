// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef ENDPOINTS_INCLUDE_CREDENTIALS_HPP_
#define ENDPOINTS_INCLUDE_CREDENTIALS_HPP_

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class credentials {
public:
    static void activate_credentials(const int _fd);

    static void deactivate_credentials(const int _fd);

    static client_t receive_credentials(const int _fd, uid_t& _uid, gid_t& _gid);

    static void send_credentials(const int _fd, client_t _client);
};

} // namespace vsomeip

#endif /* ENDPOINTS_INCLUDE_CREDENTIALS_HPP_ */
