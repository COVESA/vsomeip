// Copyright (C) 2022 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/security.hpp"
#include "../include/policy_manager_impl.hpp"
#include <vsomeip/internal/logger.hpp>
#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif
#include "../../plugin/include/plugin_manager.hpp"

#include <array>
#include <iomanip>
#include <tuple>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#define VSOMEIP_SEC_POLICY_SYMDEF(sym) symdef_t{\
    "vsomeip_sec_policy_"#sym, nullptr, reinterpret_cast<void**>(&sym) \
}

#define VSOMEIP_SEC_SYMDEF(sym) symdef_t{\
    "vsomeip_sec_"#sym, nullptr, reinterpret_cast<void**>(&sym) \
}

namespace vsomeip_v3 {

bool
security::load() {
    using symdef_t = std::tuple<const char*, void*, void**>;
    std::array<symdef_t, 6> symbol_table{
        VSOMEIP_SEC_POLICY_SYMDEF(initialize),
        VSOMEIP_SEC_POLICY_SYMDEF(authenticate_router),
        VSOMEIP_SEC_POLICY_SYMDEF(is_client_allowed_to_offer),
        VSOMEIP_SEC_POLICY_SYMDEF(is_client_allowed_to_request),
        VSOMEIP_SEC_POLICY_SYMDEF(is_client_allowed_to_access_member),
        VSOMEIP_SEC_SYMDEF(sync_client)
    };

    if (auto manager = plugin_manager::get()) {
        if (auto lib = manager->load_library(VSOMEIP_SEC_LIBRARY)) {
            // First we load the symbols into the 2nd tuple element
            for (auto& symdef : symbol_table) {
                auto name = std::get<0>(symdef);
                auto& symbol = std::get<1>(symdef);
                if (!(symbol = manager->load_symbol(lib, name))) {
                    VSOMEIP_ERROR << __func__
                                  << ": security library misses "
                                  << std::quoted(name)
                                  << " function.";
                    manager->unload_library(lib);
                    return false;
                }
            }

            // Now that we have all symbols loaded,
            // assign the 2nd tuple element to the 3rd
            for (auto& symdef : symbol_table) {
                auto symbol = std::get<1>(symdef);
                auto& stub = std::get<2>(symdef);
                *stub = symbol;
            }

            // Symbol loading complete, success!
            return true;
        } else {
#ifdef _WIN32
            VSOMEIP_ERROR << "vSomeIP Security: Loading " << VSOMEIP_SEC_LIBRARY << " failed.";
#else
            VSOMEIP_ERROR << "vSomeIP Security: " << dlerror();
#endif
        }
    }

    return false;
}

decltype(security::initialize)
security::initialize = security::default_initialize;

decltype(security::authenticate_router)
security::authenticate_router = security::default_authenticate_router;

decltype(security::is_client_allowed_to_offer)
security::is_client_allowed_to_offer = security::default_is_client_allowed_to_offer;

decltype(security::is_client_allowed_to_request)
security::is_client_allowed_to_request = security::default_is_client_allowed_to_request;

decltype(security::is_client_allowed_to_access_member)
security::is_client_allowed_to_access_member = security::default_is_client_allowed_to_access_member;

decltype(security::sync_client)
security::sync_client = security::default_sync_client;

//
// Default interface implementation
//
vsomeip_sec_policy_result_t
security::default_initialize(void) {
    return VSOMEIP_SEC_POLICY_OK;
}

vsomeip_sec_acl_result_t
security::default_authenticate_router(
        const vsomeip_sec_client_t *_server) {
    if (_server && _server->port != VSOMEIP_SEC_PORT_UNUSED)
        return VSOMEIP_SEC_OK;

    if (policy_manager_impl::get()->check_routing_credentials(_server))
        return VSOMEIP_SEC_OK;
    else
        return VSOMEIP_SEC_PERM_DENIED;
}

vsomeip_sec_acl_result_t
security::default_is_client_allowed_to_offer(
        const vsomeip_sec_client_t *_client,
        vsomeip_sec_service_id_t _service,
        vsomeip_sec_instance_id_t _instance) {
    if (_client && _client->port != VSOMEIP_SEC_PORT_UNUSED)
        return VSOMEIP_SEC_OK;

    if (policy_manager_impl::get()->is_offer_allowed(_client, _service, _instance))
        return VSOMEIP_SEC_OK;
    else
        return VSOMEIP_SEC_PERM_DENIED;
}

vsomeip_sec_acl_result_t
security::default_is_client_allowed_to_request(
        const vsomeip_sec_client_t *_client,
        vsomeip_sec_service_id_t _service,
        vsomeip_sec_instance_id_t _instance) {
    if (_client && _client->port != VSOMEIP_SEC_PORT_UNUSED)
        return VSOMEIP_SEC_OK;

    if (policy_manager_impl::get()->is_client_allowed(_client, _service, _instance, 0x00, true))
        return VSOMEIP_SEC_OK;
    else
        return VSOMEIP_SEC_PERM_DENIED;
}

vsomeip_sec_acl_result_t
security::default_is_client_allowed_to_access_member(
        const vsomeip_sec_client_t *_client,
        vsomeip_sec_service_id_t _service,
        vsomeip_sec_instance_id_t _instance,
        vsomeip_sec_member_id_t _member) {
    if (_client && _client->port != VSOMEIP_SEC_PORT_UNUSED)
        return VSOMEIP_SEC_OK;

    if (policy_manager_impl::get()->is_client_allowed(_client, _service, _instance, _member, false))
        return VSOMEIP_SEC_OK;
    else
        return VSOMEIP_SEC_PERM_DENIED;
}

void
security::default_sync_client(vsomeip_sec_client_t *_client) {

    (void)_client;
}

} // namespace vsomeip_v3
