// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_RUNTIME_IMPL_HPP
#define VSOMEIP_SD_RUNTIME_IMPL_HPP

#include <vsomeip/plugin.hpp>
#include "runtime.hpp"

namespace vsomeip {
namespace sd {

class runtime_impl
        : public runtime,
          public plugin_impl<runtime_impl> {
public:
    runtime_impl();
    virtual ~runtime_impl();

    std::shared_ptr<service_discovery> create_service_discovery(
            service_discovery_host *_host) const;
    std::shared_ptr<message_impl> create_message() const;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_RUNTIME_IMPL_HPP
