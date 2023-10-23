// Copyright (C) 2022 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_SECURITY_HPP_
#define VSOMEIP_V3_SECURITY_HPP_

#include <vsomeip/export.hpp>
#include <vsomeip/vsomeip_sec.h>

namespace vsomeip_v3 {

class VSOMEIP_IMPORT_EXPORT security {
public:
    static bool load();

    static decltype(&vsomeip_sec_policy_initialize)                         initialize;
    static decltype(&vsomeip_sec_policy_authenticate_router)                authenticate_router;
    static decltype(&vsomeip_sec_policy_is_client_allowed_to_offer)         is_client_allowed_to_offer;
    static decltype(&vsomeip_sec_policy_is_client_allowed_to_request)       is_client_allowed_to_request;
    static decltype(&vsomeip_sec_policy_is_client_allowed_to_access_member) is_client_allowed_to_access_member;
    static decltype(&vsomeip_sec_sync_client)                               sync_client;

private:
    static decltype(vsomeip_sec_policy_initialize)                         default_initialize;
    static decltype(vsomeip_sec_policy_authenticate_router)                default_authenticate_router;
    static decltype(vsomeip_sec_policy_is_client_allowed_to_offer)         default_is_client_allowed_to_offer;
    static decltype(vsomeip_sec_policy_is_client_allowed_to_request)       default_is_client_allowed_to_request;
    static decltype(vsomeip_sec_policy_is_client_allowed_to_access_member) default_is_client_allowed_to_access_member;
    static decltype(vsomeip_sec_sync_client)                               default_sync_client;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_SECURITY_HPP_
