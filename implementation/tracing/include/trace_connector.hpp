// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_TC_TRACE_CONNECTOR_HPP
#define VSOMEIP_TC_TRACE_CONNECTOR_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/export.hpp>
#include <boost/shared_ptr.hpp>
#include <mutex>
#include <vector>
#include <map>

#ifdef USE_DLT
#include <dlt/dlt.h>
#endif

#include "enumeration_types.hpp"
#include "trace_header.hpp"
#include "../../endpoints/include/buffer.hpp"

namespace vsomeip
{
namespace tc
{

class trace_connector {
public:
    typedef uint16_t filter_expression_t;
    typedef std::vector<filter_expression_t> filter_expressions_t;
    typedef std::map<filter_criteria_e, filter_expressions_t> filter_rule_map_t;
    typedef std::pair<filter_type_e, filter_rule_map_t> filter_rule_t;

    typedef std::map<trace_channel_t, std::string> channels_t;
    typedef std::map<trace_channel_t, filter_rule_t> filter_rules_t;

#ifdef USE_DLT
    typedef std::map<trace_channel_t, DltContext*> dlt_contexts_t;
#endif

    VSOMEIP_EXPORT static std::shared_ptr<trace_connector> get();

    VSOMEIP_EXPORT trace_connector();
    VSOMEIP_EXPORT virtual ~trace_connector();

    VSOMEIP_EXPORT void init();
    VSOMEIP_EXPORT void reset();

    VSOMEIP_EXPORT void set_enabled(const bool _enabled);
    VSOMEIP_EXPORT bool is_enabled() const;

    VSOMEIP_EXPORT void set_sd_enabled(const bool _enabled);
    VSOMEIP_EXPORT bool is_sd_enabled() const;

    VSOMEIP_EXPORT bool is_sd_message(const byte_t *_data, uint16_t _data_size) const;

    VSOMEIP_EXPORT bool add_channel(const trace_channel_t &_id,const std::string &_name);
    VSOMEIP_EXPORT bool remove_channel(const trace_channel_t &_id);

    VSOMEIP_EXPORT bool add_filter_rule(const trace_channel_t &_channel_id,
                                   const filter_rule_t _filter_rule);
    VSOMEIP_EXPORT bool add_filter_expression(const trace_channel_t &_channel_id,
                                         const filter_criteria_e _criteria,
                                         const filter_expression_t _expression);
    VSOMEIP_EXPORT bool change_filter_expressions(const trace_channel_t &_channel_id,
                                      const filter_criteria_e _criteria,
                                      const filter_expressions_t _expressions);
    VSOMEIP_EXPORT bool remove_filter_rule(const trace_channel_t &_channel_id);

    VSOMEIP_EXPORT void trace(const byte_t *_header, uint16_t _header_size,
            const byte_t *_data, uint16_t _data_size);

    VSOMEIP_EXPORT channels_t get_channels();
    VSOMEIP_EXPORT filter_rules_t get_filter_rules();
    VSOMEIP_EXPORT filter_rule_t get_filter_rule(const trace_channel_t &_channel_id);

private:

    bool apply_filter_rules(const byte_t *_data, const uint16_t _data_size,
            std::vector<trace_channel_t> &_send_msg_over_channels);

    bool filter_expressions_match(const filter_criteria_e _criteria,
            const filter_expressions_t _expressions,
            const byte_t *_data, const uint16_t _data_size);

    bool is_enabled_;
    bool is_sd_enabled_;
    bool is_initialized_;

    channels_t channels_;
    filter_rules_t filter_rules_;

#ifdef USE_DLT
    dlt_contexts_t dlt_contexts_;
#endif

    std::mutex channels_mutex_;
    std::mutex filter_rules_mutex_;
    std::mutex dlt_contexts_mutex;
};

} // namespace tc
} // namespace vsomeip

#endif // VSOMEIP_TC_TRACE_CONNECTOR_HPP
