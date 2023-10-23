// Copyright (C) 2020-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iostream>

#include <vsomeip/runtime.hpp>

#include "../include/logger_impl.hpp"
#include "../../configuration/include/configuration.hpp"

namespace vsomeip_v3 {
namespace logger {

std::mutex logger_impl::mutex__;

void
logger_impl::init(const std::shared_ptr<configuration> &_configuration) {
    std::lock_guard<std::mutex> its_lock(mutex__);
    auto its_logger = logger_impl::get();
    its_logger->set_configuration(_configuration);

#ifdef USE_DLT
#   define VSOMEIP_LOG_DEFAULT_CONTEXT_ID              "VSIP"
#   define VSOMEIP_LOG_DEFAULT_CONTEXT_NAME            "vSomeIP context"

#ifndef ANDROID
    std::string its_context_id = runtime::get_property("LogContext");
    if (its_context_id == "")
        its_context_id = VSOMEIP_LOG_DEFAULT_CONTEXT_ID;

    DLT_REGISTER_CONTEXT(its_logger->dlt_, its_context_id.c_str(), VSOMEIP_LOG_DEFAULT_CONTEXT_NAME);
#endif
#endif
}

logger_impl::~logger_impl() {
#ifdef USE_DLT
#ifndef ANDROID
    DLT_UNREGISTER_CONTEXT(dlt_);
#endif
#endif
}

std::shared_ptr<configuration>
logger_impl::get_configuration() const {

	std::lock_guard<std::mutex> its_lock(configuration_mutex_);
    return configuration_;
}

void
logger_impl::set_configuration(
		const std::shared_ptr<configuration> &_configuration) {

	std::lock_guard<std::mutex> its_lock(configuration_mutex_);
    configuration_ = _configuration;
}

#ifdef USE_DLT
#ifndef ANDROID
void
logger_impl::log(level_e _level, const char *_data) {

    // Prepare log level
    DltLogLevelType its_level;
    switch (_level) {
    case level_e::LL_FATAL:
        its_level = DLT_LOG_FATAL;
        break;
    case level_e::LL_ERROR:
        its_level = DLT_LOG_ERROR;
        break;
    case level_e::LL_WARNING:
        its_level = DLT_LOG_WARN;
        break;
    case level_e::LL_INFO:
        its_level = DLT_LOG_INFO;
        break;
    case level_e::LL_DEBUG:
        its_level = DLT_LOG_DEBUG;
        break;
    case level_e::LL_VERBOSE:
        its_level = DLT_LOG_VERBOSE;
        break;
    default:
        its_level = DLT_LOG_DEFAULT;
    };

    DLT_LOG_STRING(dlt_, its_level, _data);
}
#endif
#endif

static std::shared_ptr<logger_impl> *the_logger_ptr__(nullptr);
static std::mutex the_logger_mutex__;

std::shared_ptr<logger_impl>
logger_impl::get() {
#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
    std::lock_guard<std::mutex> its_lock(the_logger_mutex__);
#endif
    if (the_logger_ptr__ == nullptr) {
        the_logger_ptr__ = new std::shared_ptr<logger_impl>();
    }
    if (the_logger_ptr__ != nullptr) {
        if (!(*the_logger_ptr__)) {
            *the_logger_ptr__ = std::make_shared<logger_impl>();
        }
        return *the_logger_ptr__;
    }
    return nullptr;
}

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
static void logger_impl_teardown(void) __attribute__((destructor));
static void logger_impl_teardown(void)
{
    // TODO: This mutex is causing a crash due to changes in the way mutexes are defined.
    // Since this function only runs on the main thread, no mutex should be needed. Leaving a
    // comment pending a refactor.
    // std::lock_guard<std::mutex> its_lock(the_logger_mutex__);
    if (the_logger_ptr__ != nullptr) {
        the_logger_ptr__->reset();
        delete the_logger_ptr__;
        the_logger_ptr__ = nullptr;
    }
}
#endif

} // namespace logger
} // namespace vsomeip_v3
