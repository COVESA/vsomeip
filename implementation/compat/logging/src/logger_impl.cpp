// Copyright (C) 2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/logger_impl.hpp"
#include <vsomeip/internal/logger.hpp>

namespace vsomeip {

std::shared_ptr<logger>
logger_impl::get() {

    static std::shared_ptr<logger_impl> the_logger(
            std::make_shared<logger_impl>()
    );
    return the_logger;
}

logger_impl::logger_impl()
    : impl_(vsomeip_v3::logger::get()) {
}

boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> &
logger_impl::get_internal() {

    return impl_->get_internal();
}

} // namespace vsomeip
