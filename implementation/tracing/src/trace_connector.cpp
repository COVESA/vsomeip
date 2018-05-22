// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/trace_connector.hpp"

#include <vsomeip/constants.hpp>

#include "../include/defines.hpp"
#include "../../configuration/include/internal.hpp"

namespace vsomeip {
namespace tc {

std::shared_ptr<trace_connector> trace_connector::get() {
    static std::shared_ptr<trace_connector> instance = std::make_shared<trace_connector>();
    return instance;
}

trace_connector::trace_connector() :
    is_enabled_(false),
    is_sd_enabled_(false),
    is_initialized_(false),
    channels_(),
    filter_rules_()
    {
        channels_.insert(std::make_pair(VSOMEIP_TC_DEFAULT_CHANNEL_ID, VSOMEIP_TC_DEFAULT_CHANNEL_NAME));
}

trace_connector::~trace_connector() {
    reset();
}

void trace_connector::init() {
#ifdef USE_DLT
    if(!is_initialized_) {
        // register channels/contexts
        std::lock_guard<std::mutex> lock(dlt_contexts_mutex);
        for(auto it = channels_.begin(); it != channels_.end(); ++it) {
            DltContext *dlt_context = new DltContext();
            dlt_contexts_.insert(std::make_pair(it->first, dlt_context));
            DLT_REGISTER_CONTEXT_LL_TS(*dlt_context, it->first.c_str(), it->second.c_str(), DLT_LOG_INFO, DLT_TRACE_STATUS_ON);
        }
    }
#endif
    is_initialized_ = true;
}

void trace_connector::reset() {
    std::lock_guard<std::mutex> its_lock_channels(channels_mutex_);
    std::lock_guard<std::mutex> its_lock_filter_rules(filter_rules_mutex_);
    std::lock_guard<std::mutex> its_lock_dlt_contexts(dlt_contexts_mutex);

#ifdef USE_DLT
    // unregister channels/contexts
    for(auto it = dlt_contexts_.begin(); it != dlt_contexts_.end(); ++it) {
        DLT_UNREGISTER_CONTEXT(*it->second);
        delete it->second;
    }
    dlt_contexts_.clear();
#endif

    // reset to default

    channels_.clear();
    channels_.insert(std::make_pair(VSOMEIP_TC_DEFAULT_CHANNEL_ID, VSOMEIP_TC_DEFAULT_CHANNEL_NAME));

    filter_rules_.clear();

    is_initialized_ = false;
}

void trace_connector::set_enabled(const bool _enabled) {
    is_enabled_ = _enabled;
}

bool trace_connector::is_enabled() const {
    return is_enabled_;
}

void trace_connector::set_sd_enabled(const bool _sd_enabled) {
    is_sd_enabled_ = _sd_enabled;
}

bool trace_connector::is_sd_enabled() const {
    return is_sd_enabled_;
}

bool trace_connector::is_sd_message(const byte_t *_data, uint16_t _data_size) const {
    if (VSOMEIP_METHOD_POS_MAX < _data_size) {
        return (_data[VSOMEIP_SERVICE_POS_MIN] == 0xFF && _data[VSOMEIP_SERVICE_POS_MAX] == 0xFF &&
                _data[VSOMEIP_METHOD_POS_MIN] == 0x81 && _data[VSOMEIP_METHOD_POS_MAX] == 0x00);
    }
    return false;
}

bool trace_connector::add_channel(const trace_channel_t &_id, const std::string &_name) {
    std::lock_guard<std::mutex> its_lock_channels(channels_mutex_);
    std::lock_guard<std::mutex> its_lock_dlt_contexts(dlt_contexts_mutex);

    bool channel_inserted = false;
    bool dlt_context_registered = false;

    // add channel
    channel_inserted = channels_.insert(std::make_pair(_id, _name)).second;

    // register context
#ifdef USE_DLT
    if(channel_inserted) {
        DltContext *dlt_context = new DltContext();
        dlt_context_registered = dlt_contexts_.insert(std::make_pair(_id, dlt_context)).second;
        DLT_REGISTER_CONTEXT_LL_TS(*dlt_context, _id.c_str(), _name.c_str(), DLT_LOG_INFO, DLT_TRACE_STATUS_ON);
    }
#endif

    return (channel_inserted && dlt_context_registered);
}

bool trace_connector::remove_channel(const trace_channel_t &_id) {

    if(_id == VSOMEIP_TC_DEFAULT_CHANNEL_ID) {
        // the default channel can not be removed
        return false;
    }

    std::lock_guard<std::mutex> its_lock_channels(channels_mutex_);
    std::lock_guard<std::mutex> its_lock_filter_rules(filter_rules_mutex_);
    std::lock_guard<std::mutex> its_lock_dlt_contexts(dlt_contexts_mutex);

    bool channel_removed = false;
    bool dlt_context_unregistered = false;

    // remove channel
    channel_removed = (channels_.erase(_id) == 1);
    if(channel_removed) {

        // unregister context
#ifdef USE_DLT
        auto it = dlt_contexts_.find(_id);
        if(it != dlt_contexts_.end()) {
            DltContext *dlt_context = it->second;
            DLT_UNREGISTER_CONTEXT(*dlt_context);
            dlt_context_unregistered = true;
        }
#endif

        // remove filter
        filter_rules_.erase(_id);
    }
    return (channel_removed && dlt_context_unregistered);
}

bool trace_connector::add_filter_rule(const trace_channel_t &_channel_id,
                                 const filter_rule_t _filter_rule) {
    std::lock_guard<std::mutex> its_lock_filter_rules(filter_rules_mutex_);
    std::lock_guard<std::mutex> its_lock_channels(channels_mutex_);

    // find channel
    auto it = channels_.find(_channel_id);
    if(it != channels_.end()) {
        // add filter rule
        return filter_rules_.insert(std::make_pair(_channel_id, _filter_rule)).second;
    }
    return false;
}

bool trace_connector::add_filter_expression(const trace_channel_t &_channel_id,
                                       const filter_criteria_e _criteria,
                                       const filter_expression_t _expression) {
    std::lock_guard<std::mutex> its_lock_filter_rules(filter_rules_mutex_);

    // find filter rule
    auto it_filter_rules = filter_rules_.find(_channel_id);
    if(it_filter_rules != filter_rules_.end()) {
        filter_rule_t its_filter_rule = it_filter_rules->second;

        // find filter criteria
        auto it_filter_rule_map = its_filter_rule.second.find(_criteria);
        if(it_filter_rule_map != its_filter_rule.second.end()) {
            // add expression
            it_filter_rule_map->second.push_back(_expression);
            return true;
        }
    }
    return false;
}

bool trace_connector::change_filter_expressions(const trace_channel_t &_channel_id,
                                           const filter_criteria_e _criteria,
                                           const filter_expressions_t _expressions) {
    std::lock_guard<std::mutex> its_lock_filter_rules(filter_rules_mutex_);

    // find filter rule
    auto it_filter_rules = filter_rules_.find(_channel_id);
    if(it_filter_rules != filter_rules_.end()) {
        filter_rule_t its_filter_rule = it_filter_rules->second;

        // find filter criteria
        auto it_filter_rule_map = its_filter_rule.second.find(_criteria);
        if(it_filter_rule_map != its_filter_rule.second.end()) {
            // change expressions
            it_filter_rule_map->second = _expressions;
            return true;
        }
    }
    return false;
}

bool trace_connector::remove_filter_rule(const trace_channel_t &_channel_id) {
    std::lock_guard<std::mutex> its_lock_filter_rules(filter_rules_mutex_);

    auto it = filter_rules_.find(_channel_id);
    if(it != filter_rules_.end()) {
        return (filter_rules_.erase(_channel_id) == 1);
    }
    return false;
}

void trace_connector::trace(const byte_t *_header, uint16_t _header_size,
        const byte_t *_data, uint16_t _data_size) {
#ifdef USE_DLT
    if(!is_enabled_)
        return;

    std::lock_guard<std::mutex> its_lock_filter_rules(filter_rules_mutex_);
    std::lock_guard<std::mutex> its_lock_channels(channels_mutex_);
    std::lock_guard<std::mutex> its_lock_dlt_contexts(dlt_contexts_mutex);

    if (_data_size == 0)
        return; // no data

    if (is_sd_message(_data, _data_size) && !is_sd_enabled_)
        return; // tracing of service discovery messages is disabled!

    // check if filter rules match
    std::vector<trace_channel_t> its_channels;
    if (apply_filter_rules(_data, _data_size, its_channels)) {
        // send message over 'its_channels'
        for(auto channel_id : its_channels) {
            //find dlt context
            auto it = dlt_contexts_.find(channel_id);
            if (it != dlt_contexts_.end()) {
                DltContext *dlt_context = it->second;
                DLT_TRACE_NETWORK_SEGMENTED(*dlt_context,
                                            DLT_NW_TRACE_IPC,
                                            _header_size, static_cast<void *>(const_cast<byte_t *>(_header)),
                                            _data_size, static_cast<void *>(const_cast<byte_t *>(_data)));
            }
        }
    }
#else
    (void)_header;
    (void)_header_size;
    (void)_data;
    (void)_data_size;
#endif
}

trace_connector::channels_t trace_connector::get_channels() {
    std::lock_guard<std::mutex>its_lock_channels(channels_mutex_);
    return channels_;
}

trace_connector::filter_rules_t trace_connector::get_filter_rules() {
    std::lock_guard<std::mutex> its_lock_filters(filter_rules_mutex_);
    return filter_rules_;
}

trace_connector::filter_rule_t trace_connector::get_filter_rule(const trace_channel_t &_channel_id) {
    std::lock_guard<std::mutex> its_lock_filters(filter_rules_mutex_);

    // find filter
    auto it = filter_rules_.find(_channel_id);
    if (it != filter_rules_.end()) {
        return filter_rules_[_channel_id];
    } else {
        return filter_rule_t();
    }
}

bool trace_connector::apply_filter_rules(const byte_t *_data,  uint16_t _data_size,
                                    std::vector<trace_channel_t> &_send_msg_over_channels) {
    _send_msg_over_channels.clear();
    if (filter_rules_.size() == 0) {
        // no filter rules -> send message over all channels to DLT
        for(auto it=channels_.begin(); it !=channels_.end(); ++it)
            _send_msg_over_channels.push_back(it->first);
        return true;
    }

    // loop through filter rules
    for(auto it_filter_rules = filter_rules_.begin(); it_filter_rules != filter_rules_.end(); ++it_filter_rules) {
        trace_channel_t its_channel = it_filter_rules->first;
        filter_rule_t its_filter_rule = it_filter_rules->second;

        // apply filter rule
        bool trace_message = true;
        filter_type_e its_filter_type = its_filter_rule.first;
        for(auto it_filter_rule_map = its_filter_rule.second.begin();
            it_filter_rule_map != its_filter_rule.second.end() && trace_message;
            ++it_filter_rule_map) {

            filter_criteria_e its_criteria = it_filter_rule_map->first;
            auto &its_filter_expressions = it_filter_rule_map->second;

            if(its_filter_expressions.size() != 0) {
                // check if filter expressions of filter criteria match
                const bool filter_rule_matches = filter_expressions_match(its_criteria, its_filter_expressions, _data, _data_size);
                trace_message = !(filter_rule_matches && its_filter_type == filter_type_e::NEGATIVE);
            }
        }

        if(trace_message) {
            //filter rule matches -> send message over 'its_channel' to DLT
            _send_msg_over_channels.push_back(its_channel);
        }
    }
    return (_send_msg_over_channels.size() != 0);
}

bool trace_connector::filter_expressions_match(
        const filter_criteria_e _criteria, const filter_expressions_t _expressions,
        const byte_t *_data, uint16_t _data_size) {

    // ignore empty filter expressions
    if (_expressions.size() == 0) {
        return true;
    }

    // extract criteria from message
    bool is_successful(false);
    byte_t first = 0;
    byte_t second = 0;
    switch (_criteria) {
        case filter_criteria_e::SERVICES:
            if (VSOMEIP_SERVICE_POS_MAX < _data_size) {
                first = _data[VSOMEIP_SERVICE_POS_MIN];
                second = _data[VSOMEIP_SERVICE_POS_MAX];
                is_successful = true;
            }
            break;
        case filter_criteria_e::METHODS:
            if (VSOMEIP_SERVICE_POS_MAX < _data_size) {
                first = _data[VSOMEIP_METHOD_POS_MIN];
                second = _data[VSOMEIP_METHOD_POS_MAX];
                is_successful = true;
            }
            break;
        case filter_criteria_e::CLIENTS:
            if (VSOMEIP_CLIENT_POS_MAX < _data_size) {
                first = _data[VSOMEIP_CLIENT_POS_MIN];
                second = _data[VSOMEIP_CLIENT_POS_MAX];
                is_successful = true;
            }
            break;
        default:
            break;
    }

    // if extraction is successful, filter
    if (is_successful) {
        bool filter_expressions_matches = false;
        for (auto it_expressions = _expressions.begin();
             it_expressions != _expressions.end() && !filter_expressions_matches;
             ++it_expressions) {
            filter_expression_t its_filter_expression = *it_expressions;
            uint16_t its_message_value = 0;

            its_message_value = (uint16_t)((its_message_value << 8) + first);
            its_message_value = (uint16_t)((its_message_value << 8) + second);

            filter_expressions_matches = (its_filter_expression == its_message_value);
        }
        return filter_expressions_matches;
    }
    return false;
}

} // namespace tc
} // namespace vsomeip
