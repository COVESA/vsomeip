// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_IMPL
#define VSOMEIP_SERVICE_DISCOVERY_IMPL

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <forward_list>
#include <atomic>
#include <tuple>

#include <boost/asio/steady_timer.hpp>

#include "service_discovery.hpp"
#include "../../endpoints/include/endpoint_definition.hpp"
#include "../../routing/include/types.hpp"
#include "ip_option_impl.hpp"
#include "ipv4_option_impl.hpp"
#include "ipv6_option_impl.hpp"
#include "deserializer.hpp"
#include "../../configuration/include/configuration.hpp"

namespace vsomeip {

class endpoint;
class serializer;

namespace sd {

class entry_impl;
class eventgroupentry_impl;
class option_impl;
class request;
class serviceentry_impl;
class service_discovery_host;
class subscription;

typedef std::map<service_t, std::map<instance_t, std::shared_ptr<request> > > requests_t;

struct subscriber_t {
    std::shared_ptr<endpoint_definition> subscriber;
    std::shared_ptr<endpoint_definition> target;
    std::shared_ptr<sd_message_identifier_t> response_message_id_;
    std::shared_ptr<eventgroupinfo> eventgroupinfo_;
    vsomeip::ttl_t ttl_;
    vsomeip::major_version_t major_;
    std::uint16_t reserved_;
    std::uint8_t counter_;
};

class service_discovery_impl: public service_discovery,
        public std::enable_shared_from_this<service_discovery_impl> {
public:
    service_discovery_impl(service_discovery_host *_host);
    virtual ~service_discovery_impl();

    std::shared_ptr<configuration> get_configuration() const;
    boost::asio::io_service & get_io();

    void init();
    void start();
    void stop();

    void request_service(service_t _service, instance_t _instance,
            major_version_t _major, minor_version_t _minor, ttl_t _ttl);
    void release_service(service_t _service, instance_t _instance);

    void subscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl, client_t _client,
            subscription_type_e _subscription_type);
    void unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, client_t _client);
    void unsubscribe_all(service_t _service, instance_t _instance);
    void unsubscribe_client(service_t _service, instance_t _instance,
                            client_t _client);

    bool send(bool _is_announcing);

    void on_message(const byte_t *_data, length_t _length,
            const boost::asio::ip::address &_sender,
            const boost::asio::ip::address &_destination);

    void on_endpoint_connected(
            service_t _service, instance_t _instance,
            const std::shared_ptr<const vsomeip::endpoint> &_endpoint);

    void offer_service(service_t _service, instance_t _instance,
                                   std::shared_ptr<serviceinfo> _info);
    void stop_offer_service(service_t _service, instance_t _instance,
                            std::shared_ptr<serviceinfo> _info);

    void set_diagnosis_mode(const bool _activate);

    bool get_diagnosis_mode();

    void remote_subscription_acknowledge(
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            client_t _client, bool _acknowledged,
            const std::shared_ptr<sd_message_identifier_t> &_sd_message_id);
private:
    std::pair<session_t, bool> get_session(const boost::asio::ip::address &_address);
    void increment_session(const boost::asio::ip::address &_address);

    bool is_reboot(const boost::asio::ip::address &_sender,
            const boost::asio::ip::address &_destination,
            bool _reboot_flag, session_t _session);

    void insert_option(std::shared_ptr<message_impl> &_message,
            std::shared_ptr<entry_impl> _entry,
            const boost::asio::ip::address &_address, uint16_t _port,
            bool _is_reliable);
    void insert_find_entries(std::shared_ptr<message_impl> &_message,
                             const requests_t &_requests, uint32_t _start,
                             uint32_t &_size, bool &_done);
    void insert_offer_entries(std::shared_ptr<message_impl> &_message,
                              const services_t &_services, uint32_t &_start,
                              uint32_t _size, bool &_done, bool _ignore_phase);
    bool insert_offer_service(std::shared_ptr<message_impl> _message,
                              service_t _service, instance_t _instance,
                              const std::shared_ptr<const serviceinfo> &_info,
                              uint32_t &_size);
    enum remote_offer_type_e : std::uint8_t {
        RELIABLE_UNRELIABLE,
        RELIABLE,
        UNRELIABLE,
        UNKNOWN = 0xff
    };
    bool insert_subscription(std::shared_ptr<message_impl> &_message,
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            std::shared_ptr<subscription> &_subscription,
            remote_offer_type_e _offer_type);
    bool insert_nack_subscription_on_resubscribe(std::shared_ptr<message_impl> &_message,
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            std::shared_ptr<subscription> &_subscription,
            remote_offer_type_e _offer_type);
    void insert_subscription_ack(std::shared_ptr<message_impl> &_message,
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            const std::shared_ptr<eventgroupinfo> &_info, ttl_t _ttl,
            uint8_t _counter, major_version_t _major, uint16_t _reserved,
            const std::shared_ptr<endpoint_definition> &_target = nullptr);
    void insert_subscription_nack(std::shared_ptr<message_impl> &_message, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            uint8_t _counter, major_version_t _major, uint16_t _reserved);

    void process_serviceentry(std::shared_ptr<serviceentry_impl> &_entry,
            const std::vector<std::shared_ptr<option_impl> > &_options,
            bool _unicast_flag, std::vector<std::pair<std::uint16_t, std::shared_ptr<message_impl>>>* _resubscribes);
    void process_offerservice_serviceentry(
            service_t _service, instance_t _instance, major_version_t _major,
            minor_version_t _minor, ttl_t _ttl,
            const boost::asio::ip::address &_reliable_address,
            uint16_t _reliable_port,
            const boost::asio::ip::address &_unreliable_address,
            uint16_t _unreliable_port, std::vector<std::pair<std::uint16_t, std::shared_ptr<message_impl>>>* _resubscribes);
    void send_offer_service(
            const std::shared_ptr<const serviceinfo> &_info, service_t _service,
            instance_t _instance, major_version_t _major, minor_version_t _minor,
            bool _unicast_flag);

    void process_findservice_serviceentry(service_t _service,
            instance_t _instance,
            major_version_t _major,
            minor_version_t _minor,
            bool _unicast_flag);
    void process_eventgroupentry(
            std::shared_ptr<eventgroupentry_impl> &_entry,
            const std::vector<std::shared_ptr<option_impl> > &_options,
            std::shared_ptr < message_impl > &its_message_response,
            const boost::asio::ip::address &_destination,
            const std::shared_ptr<sd_message_identifier_t> &_message_id,
            bool _is_stop_subscribe_subscribe,
            bool _force_initial_events);
    void handle_eventgroup_subscription(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, ttl_t _ttl, uint8_t _counter, uint16_t _reserved,
            const boost::asio::ip::address &_first_address, uint16_t _first_port,
            bool _is_first_reliable,
            const boost::asio::ip::address &_second_address, uint16_t _second_port,
            bool _is_second_reliable,
            std::shared_ptr < message_impl > &its_message,
            const std::shared_ptr<sd_message_identifier_t> &_message_id,
            bool _is_stop_subscribe_subscribe,
            bool _force_initial_events);
    void handle_eventgroup_subscription_ack(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            major_version_t _major, ttl_t _ttl, uint8_t _counter,
            const boost::asio::ip::address &_address, uint16_t _port);
    void handle_eventgroup_subscription_nack(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, uint8_t _counter);
    void serialize_and_send(std::shared_ptr<message_impl> _message,
            const boost::asio::ip::address &_address);

    bool is_tcp_connected(service_t _service,
            instance_t _instance,
            std::shared_ptr<vsomeip::endpoint_definition> its_endpoint);

    void start_ttl_timer();
    void stop_ttl_timer();

    void check_ttl(const boost::system::error_code &_error);

    void start_subscription_expiration_timer();
    void start_subscription_expiration_timer_unlocked();
    void stop_subscription_expiration_timer();
    void stop_subscription_expiration_timer_unlocked();
    void expire_subscriptions(const boost::system::error_code &_error);

    bool check_ipv4_address(boost::asio::ip::address its_address);

    bool check_static_header_fields(
            const std::shared_ptr<const message> &_message) const;
    bool check_layer_four_protocol(
            const std::shared_ptr<const ip_option_impl> _ip_option) const;
    void get_subscription_endpoints(std::shared_ptr<endpoint>& _unreliable,
                                    std::shared_ptr<endpoint>& _reliable,
                                    boost::asio::ip::address* _address,
                                    bool* _has_address,
                                    service_t _service, instance_t _instance,
                                    client_t _client) const;

    void send_subscriptions(service_t _service, instance_t _instance, client_t _client, bool _reliable);

    template<class Option, typename AddressType>
    std::shared_ptr<option_impl> find_existing_option(
            std::shared_ptr<message_impl> &_message,
            AddressType _address, uint16_t _port,
            layer_four_protocol_e _protocol,
            option_type_e _option_type);
    template<class Option, typename AddressType>
    bool check_message_for_ip_option_and_assign_existing(
            std::shared_ptr<message_impl> &_message,
            std::shared_ptr<entry_impl> _entry, AddressType _address,
            uint16_t _port, layer_four_protocol_e _protocol,
            option_type_e _option_type);
    template<class Option, typename AddressType>
    void assign_ip_option_to_entry(std::shared_ptr<Option> _option,
                                   AddressType _address, uint16_t _port,
                                   layer_four_protocol_e _protocol,
                                   std::shared_ptr<entry_impl> _entry);

    std::shared_ptr<request> find_request(service_t _service, instance_t _instance);

    void start_offer_debounce_timer(bool _first_start);
    void on_offer_debounce_timer_expired(const boost::system::error_code &_error);


    void start_find_debounce_timer(bool _first_start);
    void on_find_debounce_timer_expired(const boost::system::error_code &_error);


    void on_repetition_phase_timer_expired(
            const boost::system::error_code &_error,
            std::shared_ptr<boost::asio::steady_timer> _timer,
            std::uint8_t _repetition, std::uint32_t _last_delay);
    void on_find_repetition_phase_timer_expired(
            const boost::system::error_code &_error,
            std::shared_ptr<boost::asio::steady_timer> _timer,
            std::uint8_t _repetition, std::uint32_t _last_delay);
    void move_offers_into_main_phase(
            const std::shared_ptr<boost::asio::steady_timer> &_timer);

    void fill_message_with_offer_entries(
            std::shared_ptr<runtime> _runtime,
            std::shared_ptr<message_impl> _message,
            std::vector<std::shared_ptr<message_impl>> &_messages,
            const services_t &_offers, bool _ignore_phase);

    void fill_message_with_find_entries(
            std::shared_ptr<runtime> _runtime,
            std::shared_ptr<message_impl> _message,
            std::vector<std::shared_ptr<message_impl>> &_messages,
            const requests_t &_requests);

    bool serialize_and_send_messages(
            const std::vector<std::shared_ptr<message_impl>> &_messages);

    bool send_stop_offer(service_t _service, instance_t _instance,
                         std::shared_ptr<serviceinfo> _info);

    void start_main_phase_timer();
    void on_main_phase_timer_expired(const boost::system::error_code &_error);


    void send_uni_or_multicast_offerservice(
            service_t _service, instance_t _instance, major_version_t _major,
            minor_version_t _minor,
            const std::shared_ptr<const serviceinfo> &_info,
            bool _unicast_flag);
    bool last_offer_shorter_half_offer_delay_ago();
    void send_unicast_offer_service(
            const std::shared_ptr<const serviceinfo> &_info, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor);
    void send_multicast_offer_service(
            const std::shared_ptr<const serviceinfo>& _info, service_t _service,
            instance_t _instance, major_version_t _major,
            minor_version_t _minor);

    bool check_source_address(const boost::asio::ip::address &its_source_address) const;

    void update_subscription_expiration_timer(const std::shared_ptr<message_impl> &_message);

    void remote_subscription_acknowledge_subscriber(
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            const std::shared_ptr<subscriber_t> &_subscriber, bool _acknowledged);

    void remote_subscription_not_acknowledge_subscriber(
            service_t _service, instance_t _instance, eventgroup_t _eventgroup,
            const std::shared_ptr<subscriber_t> &_subscriber, bool _acknowledged);

    void remote_subscription_not_acknowledge_all(service_t _service, instance_t _instance);

    void remote_subscription_not_acknowledge_all();

    bool check_stop_subscribe_subscribe(message_impl::entries_t::const_iterator _iter,
                                        message_impl::entries_t::const_iterator _end,
                                        const message_impl::options_t& _options) const;

    configuration::ttl_factor_t get_ttl_factor(
            service_t _service, instance_t _instance,
            const configuration::ttl_map_t& _ttl_map) const;
    void on_last_msg_received_timer_expired(const boost::system::error_code &_error);
    void stop_last_msg_received_timer();


    remote_offer_type_e get_remote_offer_type(service_t _service, instance_t _instance);
    bool update_remote_offer_type(service_t _service, instance_t _instance,
                                  remote_offer_type_e _offer_type,
                                  const boost::asio::ip::address &_reliable_address,
                                  const boost::asio::ip::address &_unreliable_address);
    void remove_remote_offer_type(service_t _service, instance_t _instance,
                                  const boost::asio::ip::address &_address);
    void remove_remote_offer_type_by_ip(const boost::asio::ip::address &_address);

    std::vector<std::tuple<service_t, instance_t, eventgroup_t,
            std::shared_ptr<endpoint_definition>>>
                get_eventgroups_requiring_initial_events(
                        const std::shared_ptr<message_impl>& _response) const;


private:
    boost::asio::io_service &io_;
    service_discovery_host *host_;

    boost::asio::ip::address unicast_;
    uint16_t port_;
    bool reliable_;
    std::shared_ptr<endpoint> endpoint_;

    std::shared_ptr<serializer> serializer_;
    std::shared_ptr<deserializer> deserializer_;

    requests_t requested_;
    std::mutex requested_mutex_;
    std::map<service_t,
            std::map<instance_t,
                    std::map<eventgroup_t,
                        std::map<client_t,
                            std::shared_ptr<subscription> > > > > subscribed_;
    std::mutex subscribed_mutex_;

    std::mutex serialize_mutex_;

    // Sessions
    std::map<boost::asio::ip::address, std::pair<session_t, bool> > sessions_sent_;
    std::map<boost::asio::ip::address,
        std::tuple<session_t, session_t, bool, bool> > sessions_received_;
    std::mutex sessions_received_mutex_;

    // Runtime
    std::weak_ptr<runtime> runtime_;

    // TTL handling for services offered by other hosts
    std::mutex ttl_timer_mutex_;
    boost::asio::steady_timer ttl_timer_;
    std::chrono::milliseconds ttl_timer_runtime_;
    ttl_t ttl_;

    // TTL handling for subscriptions done by other hosts
    std::mutex subscription_expiration_timer_mutex_;
    boost::asio::steady_timer subscription_expiration_timer_;
    std::chrono::steady_clock::time_point next_subscription_expiration_;

    uint32_t max_message_size_;

    std::chrono::milliseconds initial_delay_;
    std::chrono::milliseconds offer_debounce_time_;
    std::chrono::milliseconds repetitions_base_delay_;
    std::uint8_t repetitions_max_;
    std::chrono::milliseconds cyclic_offer_delay_;
    std::mutex offer_debounce_timer_mutex_;
    boost::asio::steady_timer offer_debounce_timer_;
    // this map is used to collect offers while for offer debouncing
    std::mutex collected_offers_mutex_;
    services_t collected_offers_;

    std::chrono::milliseconds find_debounce_time_;
    std::mutex find_debounce_timer_mutex_;
    boost::asio::steady_timer find_debounce_timer_;
    requests_t collected_finds_;

    // this map contains the offers and their timers currently in repetition phase
    std::mutex repetition_phase_timers_mutex_;
    std::map<std::shared_ptr<boost::asio::steady_timer>,
            services_t> repetition_phase_timers_;

    // this map contains the finds and their timers currently in repetition phase
    std::mutex find_repetition_phase_timers_mutex_;
    std::map<std::shared_ptr<boost::asio::steady_timer>,
            requests_t> find_repetition_phase_timers_;

    std::mutex main_phase_timer_mutex_;
    boost::asio::steady_timer main_phase_timer_;

    std::atomic<bool> is_suspended_;

    std::string sd_multicast_;
    boost::asio::ip::address sd_multicast_address_;

    boost::asio::ip::address current_remote_address_;

    std::atomic<bool> is_diagnosis_;

    std::mutex pending_remote_subscriptions_mutex_;
    std::map<service_t,
            std::map<instance_t,
                    std::map<eventgroup_t,
                            std::map<client_t, std::vector<std::shared_ptr<subscriber_t>>>>>> pending_remote_subscriptions_;
    std::mutex response_mutex_;

    configuration::ttl_map_t ttl_factor_offers_;
    configuration::ttl_map_t ttl_factor_subscriptions_;

    std::mutex last_msg_received_timer_mutex_;
    boost::asio::steady_timer last_msg_received_timer_;
    std::chrono::milliseconds last_msg_received_timer_timeout_;

    std::mutex remote_offer_types_mutex_;
    std::map<std::pair<service_t, instance_t>, remote_offer_type_e> remote_offer_types_;
    std::map<boost::asio::ip::address, std::set<std::pair<service_t, instance_t>>> remote_offers_by_ip_;
};

}  // namespace sd
}  // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_IMPL
