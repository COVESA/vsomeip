// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <climits>
#include <ctime>
#include <span>

#ifdef USE_DLT
#include <dlt/dlt.h>
#include "logger_ext.hpp"
#endif

#include <vsomeip/constants.hpp>
#include "vsomeip/defines.hpp"
#include <vsomeip/runtime.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/channel_impl.hpp"
#include "../include/connector_impl.hpp"
#include "../include/defines.hpp"
#include "../include/log_context.hpp"
#include "../include/tracing_policy.hpp"
#include "../../configuration/include/trace.hpp"
#include "../../utility/include/bithelper.hpp"
#include "../../utility/include/utility.hpp"

#if defined(ANDROID) && !defined(ANDROID_CI_BUILD)
#include <utils/Log.h>

#ifdef ALOGI
#undef ALOGI
#endif

#define ALOGI(LOG_TAG, ...) ((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__))
#ifndef LOGI
#define LOGI ALOGI
#endif

#endif

#define VSOMEIP_LOG_PREFIX "connector_impl"

namespace vsomeip_v3::trace {

constexpr const char* VSOMEIP_TC_DEFAULT_CHANNEL_ID = "TC";

connector_impl::connector_impl(const std::shared_ptr<cfg::trace>& _configuration) :
    full_logging_threshold_{VSOMEIP_TC_DEFAULT_FULL_LOGGING_THRESHOLD} {

    add_channel(VSOMEIP_TC_DEFAULT_CHANNEL_ID, VSOMEIP_TC_DEFAULT_CHANNEL_NAME);

    if (_configuration) {
        is_enabled_ = _configuration->is_enabled_;
        is_sd_enabled_ = _configuration->is_sd_enabled_;
        full_logging_threshold_ = _configuration->full_logging_threshold_;
    }

    if (_configuration && is_enabled_) { // No need to create filters if tracing is disabled!
        for (const auto& channel : _configuration->channels_) {
            if (!add_channel(channel->id_, channel->name_)) {
                VSOMEIP_ERROR << "Channel " << channel->id_ << " has multiple definitions.";
            }
        }

        for (const auto& filter : _configuration->filters_) {
            for (const auto& channel : filter->channels_) {
                if (auto p = get_channel(channel)) {
                    if (filter->is_range_) {
                        p->add_filter(filter->matches_[0], filter->matches_[1], filter->ftype_);
                    } else {
                        p->add_filter(filter->matches_, filter->ftype_);
                    }
                }
            }
        }
    }

    VSOMEIP_INFO << "vsomeip tracing " << (is_enabled_ ? "enabled." : "not enabled.") << " vsomeip service discovery tracing "
                 << (is_sd_enabled_ ? "enabled." : "not enabled.") << " full-logging threshold: " << full_logging_threshold_
                 << (full_logging_threshold_ == 0 ? " (disabled, always full)." : " bytes (larger payloads logged header-only).");
}

void connector_impl::set_enabled(bool _enabled) {
    is_enabled_ = _enabled;
}

bool connector_impl::is_enabled() const {
    return is_enabled_;
}

void connector_impl::set_sd_enabled(bool _sd_enabled) {
    is_sd_enabled_ = _sd_enabled;
}

bool connector_impl::is_sd_enabled() const {
    return is_sd_enabled_;
}

bool connector_impl::is_sd_message(const byte_t* _data, uint16_t _data_size) const {
    if (VSOMEIP_METHOD_POS_MAX < _data_size) {
        return (_data[VSOMEIP_SERVICE_POS_MIN] == 0xFF && _data[VSOMEIP_SERVICE_POS_MAX] == 0xFF && _data[VSOMEIP_METHOD_POS_MIN] == 0x81
                && _data[VSOMEIP_METHOD_POS_MAX] == 0x00);
    }
    return false;
}

bool connector_impl::add_channel(const trace_channel_t& _id, const std::string& _description) {
    if (channels_.count(_id) > 0) {
        return false;
    }

    channels_.try_emplace(_id, std::make_shared<channel_impl>(_id, get_log_context(_id.c_str(), _description.c_str())));
    return true;
}

std::shared_ptr<channel_impl> connector_impl::get_channel(const std::string& _id) const {
    if (auto it = channels_.find(_id); it != channels_.end()) {
        return it->second;
    }
    return nullptr;
}

#ifndef USE_DLT

namespace {

// Same per-byte hex lookup as utility::append_hex(), but space-separated to match the
// expected trace format ("id: aa bb cc" rather than a contiguous "aabbcc").
void append_hex_with_spaces(std::string& _out, std::span<const byte_t> _data) {
    _out.reserve(_out.size() + _data.size() * 3);
    for (byte_t b : _data) {
        _out += ' ';
        append_hex_byte(_out, b);
    }
}

} // namespace

#endif

void connector_impl::trace(const byte_t* _header, uint16_t _header_size, const byte_t* _data, uint32_t _full_data_size) const {

    if (!is_enabled_ || _full_data_size == 0) {
        return;
    }

    // Clamp to uint16, as that is the max that libdlt accepts.
    const auto message_size = static_cast<uint16_t>(std::min<uint32_t>(_full_data_size, USHRT_MAX));

    // The service and method ids are read from the SOME/IP header, the instance id from the trace
    // header. Bail out on buffers too short to carry them instead of reading out of bounds.
    if (message_size <= VSOMEIP_METHOD_POS_MAX || _header_size <= VSOMEIP_TC_INSTANCE_POS_MAX) {
        return;
    }

    if (!is_sd_enabled_ && is_sd_message(_data, message_size)) {
        return; // tracing of service discovery messages is disabled!
    }

    const service_t service = bithelper::read_uint16_be(&_data[VSOMEIP_SERVICE_POS_MIN]);
    // Instance is not part of the SOME/IP header, read it from the trace header
    const instance_t instance = bithelper::read_uint16_be(&_header[VSOMEIP_TC_INSTANCE_POS_MIN]);
    const method_t method = bithelper::read_uint16_be(&_data[VSOMEIP_METHOD_POS_MIN]);

    // Forward to channel if the filter set of the channel allows
    for (const auto& [id, channel] : channels_) {
        auto match_result = channel->matches(service, instance, method);
        if (match_result == trace_result_e::DROP) {
            continue;
        }

        // Full payload vs. header only (see should_log_full / full_logging_threshold_).
        const auto log_full = should_log_full(match_result, _full_data_size, full_logging_threshold_);
        const auto traced_size = log_full ? message_size : std::min<uint16_t>(message_size, VSOMEIP_FULL_HEADER_SIZE);

#ifdef USE_DLT
        try {
            if (log_full) {
                DLT_TRACE_NETWORK_SEGMENTED(*(channel->get_log_context()), DLT_NW_TRACE_IPC, _header_size,
                                            static_cast<void*>(const_cast<byte_t*>(_header)), traced_size,
                                            static_cast<void*>(const_cast<byte_t*>(_data)));
            } else {
                DLT_TRACE_NETWORK_TRUNCATED(*(channel->get_log_context()), DLT_NW_TRACE_IPC, _header_size,
                                            static_cast<void*>(const_cast<byte_t*>(_header)), traced_size,
                                            static_cast<void*>(const_cast<byte_t*>(_data)));
            }
        } catch (const std::exception& e) {
            VSOMEIP_INFO_P << "Exception caught when trying to log a trace with DLT. " << e.what();
        }
#else
        std::string message;
#if !defined(ANDROID)
        message += id;
        message += ':';
#elif !defined(ANDROID_CI_BUILD)
        message += "TC:";
#endif
        // Covers both appends below, so the reserve inside append_hex_with_spaces is a no-op.
        message.reserve(message.size() + (static_cast<size_t>(_header_size) + traced_size) * 3);
        append_hex_with_spaces(message, std::span<const byte_t>(_header, _header_size));
        append_hex_with_spaces(message, std::span<const byte_t>(_data, traced_size));
#if defined(ANDROID) && !defined(ANDROID_CI_BUILD)
        std::string app = runtime::get_property("LogApplication");
        ALOGI(app.c_str(), message.c_str());
#else
        VSOMEIP_INFO << message;
#endif
#endif
    }
}

} // namespace vsomeip_v3::trace
