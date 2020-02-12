// Copyright (C) 2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_COMPAT_LOGGER_HPP_
#define VSOMEIP_COMPAT_LOGGER_HPP_

#include <memory>

#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>

namespace vsomeip {

class logger {
public:
    static std::shared_ptr<logger> get();

    virtual ~logger() {}

    virtual boost::log::sources::severity_logger_mt<
            boost::log::trivial::severity_level> & get_internal() = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_COMPAT_LOGGER_HPP_
