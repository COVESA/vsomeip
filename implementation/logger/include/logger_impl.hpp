// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_LOGGER_CONFIGURATION_HPP_
#define VSOMEIP_V3_LOGGER_CONFIGURATION_HPP_

#include <memory>
#include <mutex>

#ifdef USE_DLT
#ifndef ANDROID
#include <dlt/dlt.h>
#endif
#endif

#include <vsomeip/internal/logger.hpp>

namespace vsomeip_v3 {

class configuration;

namespace logger {

class logger_impl {
public:
    VSOMEIP_IMPORT_EXPORT static void init(const std::shared_ptr<configuration> &_configuration);
    static std::shared_ptr<logger_impl> get();

    logger_impl() = default;
    ~logger_impl();

    std::shared_ptr<configuration> get_configuration() const;
    void set_configuration(const std::shared_ptr<configuration> &_configuration);

#ifdef USE_DLT
    void log(level_e _level, const char *_data);

private:
    void enable_dlt(const std::string &_application, const std::string &_context);
#endif

private:
    static std::mutex mutex__;

    std::shared_ptr<configuration> configuration_;
    mutable std::mutex configuration_mutex_;

#ifdef USE_DLT
#ifndef ANDROID
    DLT_DECLARE_CONTEXT(dlt_)
#endif
#endif
};

} // namespace logger
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_LOGGER_CONFIGURATION_HPP_
