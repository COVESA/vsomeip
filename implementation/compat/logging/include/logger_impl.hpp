// Copyright (C) 2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_COMPAT_LOGGER_IMPL_HPP_
#define VSOMEIP_COMPAT_LOGGER_IMPL_HPP_

#include "logger.hpp"

namespace vsomeip_v3 {
class logger;
} // namespace vsomeip_v3

namespace vsomeip {

class logger_impl
        : public logger {
public:
    static std::shared_ptr<logger> get();

    logger_impl();

    boost::log::sources::severity_logger_mt<
                boost::log::trivial::severity_level> & get_internal();

private:
    std::shared_ptr<vsomeip_v3::logger> impl_;
};

} // namespace vsomeip

#endif // VSOMEIP_COMPAT_LOGGER_IMPL_HPP_
