// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_CONFIGURATION_HPP
#define VSOMEIP_CONFIGURATION_HPP

#include <map>
#include <memory>
#include <set>
#include <string>

#include <boost/asio/ip/address.hpp>
#include <boost/log/trivial.hpp>

#include <vsomeip/export.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/primitive_types.hpp>

#include "internal.hpp"
#include "trace.hpp"

namespace vsomeip {

class event;

class VSOMEIP_EXPORT configuration {
public:
    static std::shared_ptr<configuration> get(
            const std::set<std::string> &_input = std::set<std::string>());
    static void reset();
    virtual ~configuration() {}

    virtual const boost::asio::ip::address & get_unicast_address() const = 0;
    virtual unsigned short get_diagnosis_address() const = 0;
    virtual bool is_v4() const = 0;
    virtual bool is_v6() const = 0;

    virtual bool has_console_log() const = 0;
    virtual bool has_file_log() const = 0;
    virtual bool has_dlt_log() const = 0;
    virtual const std::string & get_logfile() const = 0;
    virtual boost::log::trivial::severity_level get_loglevel() const = 0;

    virtual const std::string & get_routing_host() const = 0;

    virtual std::string get_unicast_address(service_t _service,
            instance_t _instance) const = 0;
    virtual uint16_t get_reliable_port(service_t _service,
            instance_t _instance) const = 0;
    virtual bool has_enabled_magic_cookies(std::string _address,
            uint16_t _port) const = 0;
    virtual uint16_t get_unreliable_port(service_t _service,
            instance_t _instance) const = 0;

    virtual bool is_someip(service_t _service, instance_t _instance) const = 0;

    virtual bool get_client_port(
    		service_t _service, instance_t _instance, bool _reliable,
			std::map<bool, std::set<uint16_t> > &_used, uint16_t &_port) const = 0;

    virtual std::set<std::pair<service_t, instance_t> > get_remote_services() const = 0;

    virtual bool get_multicast(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, std::string &_address, uint16_t &_port) const = 0;

    virtual client_t get_id(const std::string &_name) const = 0;
    virtual bool is_configured_client_id(client_t _id) const = 0;

    virtual std::size_t get_max_dispatchers(const std::string &_name) const = 0;
    virtual std::size_t get_max_dispatch_time(const std::string &_name) const = 0;

    virtual std::uint32_t get_max_message_size_local() const = 0;
    virtual std::uint32_t get_message_size_reliable(const std::string& _address,
                                                    std::uint16_t _port) const = 0;

    virtual bool supports_selective_broadcasts(boost::asio::ip::address _address) const = 0;

    // Service Discovery configuration
    virtual bool is_sd_enabled() const = 0;

    virtual const std::string & get_sd_multicast() const = 0;
    virtual uint16_t get_sd_port() const = 0;
    virtual const std::string & get_sd_protocol() const = 0;

    virtual int32_t get_sd_initial_delay_min() const = 0;
    virtual int32_t get_sd_initial_delay_max() const = 0;
    virtual int32_t get_sd_repetitions_base_delay() const = 0;
    virtual uint8_t get_sd_repetitions_max() const = 0;
    virtual ttl_t get_sd_ttl() const = 0;
    virtual int32_t get_sd_cyclic_offer_delay() const = 0;
    virtual int32_t get_sd_request_response_delay() const = 0;

    // Trace configuration
    virtual std::shared_ptr<cfg::trace> get_trace() const = 0;

    // Watchdog
    virtual bool is_watchdog_enabled() const = 0;
    virtual uint32_t get_watchdog_timeout() const = 0;
    virtual uint32_t get_allowed_missing_pongs() const = 0;

    // File permissions
    virtual std::uint32_t get_umask() const = 0;
    virtual std::uint32_t get_permissions_shm() const = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_CONFIGURATION_HPP
