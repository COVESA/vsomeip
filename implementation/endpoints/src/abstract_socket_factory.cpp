// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/abstract_socket_factory.hpp"
#include "../include/asio_socket_factory.hpp"

#include <iostream>
namespace vsomeip_v3 {

/**
 * Q: Why is this not a static local variable in the init function itself?
 * A: Because we need the capability of injecting a fake abstract socket factory,
 *    to swap out the asio sockets.
 *
 * Q: Why is there no mutex?
 * A: Because once the abstract_socket_factory::get() has been called for the first
 *    time the used shared_ptr of the running binary will no longer be changed.
 *
 * Q: But what if abstract_socket_factory::get() would be called from multiple threads?
 * A: This is fine, as the init() function will be called exactly once and is awaited,
 *    by secondary calls of ::get(), if the init is not yet done.
 *
 * Q: But what if set_abstract_factory is called from another thread as ::get()?
 * A: This is most likely the case. But this should only happen in test code,
 *    within production code "set_abstract_factory" should not be called.
 *
 * Q: But why would this be fine in test code?
 * A: Because within test code the developer can be requested to ensure that before
 *    the spawning of any thread set had been called already (notice this is only meaningfully done
 *    once per binary).
 *
 * Q: Wouldn't it be more convinient to allow the adjustment of the shared_ptr at any time?
 * A: Maybe. But it would come along the cost of mutual exclusive access, which was considered a
 *non-neglectable cost to pay during production run-time for enabling these tests.
 **/
static std::shared_ptr<abstract_socket_factory> _factory;
static std::shared_ptr<abstract_socket_factory> init() {
    if (!_factory) {
        _factory = std::make_shared<asio_socket_factory>();
    }
    return _factory;
}

void set_abstract_factory(std::shared_ptr<abstract_socket_factory> ptr) {
    _factory = ptr;
}

abstract_socket_factory* abstract_socket_factory::get() {
    static auto const factory = init();
    return factory.get();
}

}
