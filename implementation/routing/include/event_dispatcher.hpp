// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

namespace vsomeip_v3 {

class event_dispatcher {
public:
    virtual ~event_dispatcher() = default;

    virtual session_t get_event_session() = 0;

    virtual bool send_event(client_t _client, std::shared_ptr<message> _message, bool _force) = 0;

    virtual bool send_event_to(const client_t _client, const std::shared_ptr<endpoint_definition>& _target,
                               std::shared_ptr<message> _message) = 0;
};

} // namespace vsomeip_v3
