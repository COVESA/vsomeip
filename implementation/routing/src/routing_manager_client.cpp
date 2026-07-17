// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if __GNUC__ > 11
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

#if defined(__linux__) || defined(__QNX__)
#include <unistd.h>
#endif

#include <algorithm>
#include <climits>
#include <forward_list>
#include <future>
#include <iomanip>
#include <mutex>
#include <thread>
#include <unordered_set>

#include <boost/asio/post.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/runtime.hpp>
#include <vsomeip/vsomeip_sec.h>

#include "logger_ext.hpp"
#include "../include/event.hpp"
#include "../include/routing_manager_host.hpp"
#include "../include/routing_manager_client.hpp"
#include "../include/routing_client_state_machine.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../endpoints/include/abstract_socket_factory.hpp"
#include "../../endpoints/include/local_server.hpp"
#include "../../endpoints/include/local_endpoint.hpp"
#include "../../message/include/message_impl.hpp"
#include "../../protocol/include/deserialize.hpp"
#include "../../protocol/include/command_types.hpp"
#include "../../protocol/include/serialize.hpp"
#include "../../protocol/include/logging.hpp"
#include "../../service_discovery/include/runtime.hpp"
#include "../../security/include/policy.hpp"
#include "../../security/include/policy_manager_impl.hpp"
#include "../../security/include/security.hpp"
#include "../../utility/include/bithelper.hpp"
#include "../../utility/include/is_value.hpp"
#include "../../utility/include/service_instance_map.hpp"
#include "../../utility/include/utility.hpp"
#include "../../tracing/include/connector_impl.hpp"

#if defined(__QNX__)
#define HAVE_INET_PTON 1
#include <boost/icl/concept/interval_associator.hpp>
#endif
namespace vsomeip_v3 {

#define VSOMEIP_LOG_PREFIX "rmc"

routing_manager_client::routing_manager_client(routing_manager_host* _host, bool _client_side_logging,
                                               const std::set<std::tuple<service_t, instance_t>>& _client_side_logging_filter) :
    host_(_host), io_(host_->get_io()), configuration_(host_->get_configuration()), env_([] {
        char h[1024];
        return gethostname(h, sizeof(h)) == 0 ? std::string(h) : std::string{};
    }()),
    tc_(trace::connector_impl::get()), sender_(nullptr), tcp_receiver_(nullptr), uds_receiver_(nullptr),
    client_side_logging_(_client_side_logging), client_side_logging_filter_(_client_side_logging_filter),
    routing_mode_(configuration_->is_local_routing()           ? routing_mode_e::UDS_ONLY
                          : configuration_->is_uds_preferred() ? routing_mode_e::UDS_AND_TCP
                                                               : routing_mode_e::TCP_ONLY),
    request_debounce_timer_running_(false), request_debounce_timer_(io_) {

    ep_mgr_ = std::make_shared<endpoint_manager_base>(*this, io_, configuration_, get_name(), get_client_host());
}

routing_manager_client::~routing_manager_client() { }

void routing_manager_client::init() {
    ep_mgr_->init(shared_from_this());
    sender_debounce_ = timer::create(io_, std::chrono::milliseconds(100), [this, weak_self = weak_from_this()] {
        if (auto self = weak_self.lock(); self) {
            debounce_restart_sender_done();
        }
        return false;
    });
    if (!state_machine_) {
        state_machine_ = std::make_unique<routing_client_state_machine>([this, weak_self = weak_from_this()] {
            if (auto self = weak_self.lock(); self) {
                std::unique_lock lock{sender_mutex_};
                restart_sender(lock);
            }
        });
    }

    if (uint32_t const its_interval = configuration_->get_version_log_interval(host_->get_name(), false); its_interval > 0) {
        version_logger_ = timer::create(io_, std::chrono::milliseconds(its_interval), [weak_self = weak_from_this()] {
            if (auto self = weak_self.lock(); self) {
                self->log_version();
                return true; // repeat
            }
            return false;
        });
    }
    if (uint32_t const its_interval = configuration_->get_status_log_interval(host_->get_name(), false); its_interval > 0) {
        status_logger_ = timer::create(io_, std::chrono::milliseconds(its_interval), [weak_self = weak_from_this()] {
            if (auto self = weak_self.lock(); self) {
                self->log_status();
                return true;
            }
            return false;
        });
    }
}

void routing_manager_client::start() {
    state_machine_->target_running();
    ep_mgr_->start();
    {
        std::scoped_lock its_receiver_lock(receiver_mutex_);
        // NOTE: order matters, `create_local_server` must done first
        // with TCP, following `create_local_client` will use whatever port is established there
        if (routing_mode_ != routing_mode_e::UDS_ONLY && !tcp_receiver_) {
            tcp_receiver_ = ep_mgr_->create_local_server(transport_protocol_e::TCP);
            VSOMEIP_INFO << "Created local TCP server for routing manager client";
        }
    }
    std::unique_lock lock{sender_mutex_};
    assert(!on_sender_stopped_);
    on_sender_stopped_ = {};
    restart_sender(lock);
    if (status_logger_) {
        status_logger_->start();
        log_status();
    }
    if (version_logger_) {
        version_logger_->start();
        log_version();
    }
}

void routing_manager_client::log_status() {
    VSOMEIP_INFO_P << " ";
    ep_mgr_->print_status();
    {
        std::scoped_lock its_lock{consumer_mutex_};
        VSOMEIP_INFO_P << "status local consumer endpoints: " << consumer_.size();
        for (const auto& [client, data] : consumer_) {
            if (data.ep_) {
                data.ep_->print_status();
            } else {
                VSOMEIP_INFO_P << "No consumer connection to client 0x" << hex4(client);
            }
        }
    }
}

void routing_manager_client::log_version() {
    VSOMEIP_INFO << "vSomeIP " << VSOMEIP_VERSION << " (" << VSOMEIP_GIT_COMMIT << ") | ";
    utility::log_network_state(configuration_, true, false);
}

async::hook routing_manager_client::stop() {
    state_machine_->target_shutdown();
    async::hook when_sender_stopped;
    // Transition to ST_DEREGISTERED so that a subsequent start() finds a clean state.
    // The error handler is suppressed because shall_run_ = false (target_shutdown was called above).
    {
        std::scoped_lock its_sender_lock{sender_mutex_};
        assert(!on_sender_stopped_);
        on_sender_stopped_ = async::trigger(io_);
        when_sender_stopped = on_sender_stopped_.get_hook();
        // transition the state under the sender mutex, as this is the one protecting the restart sequence
        state_machine_->deregistered();
        if (sender_) {
            VSOMEIP_INFO_P << "starting to flush the sender";
            sender_->start_flushing();
        } else {
            VSOMEIP_INFO_P << "sender already cleared";
            on_sender_stopped_.fire();
            on_sender_stopped_ = {};
        }
    }
    auto when_provider_eps_flushed = ep_mgr_->stop();

    {
        std::scoped_lock its_lock{consumer_mutex_};
        request_debounce_timer_.cancel();
    }

    {
        std::scoped_lock its_receiver_lock(receiver_mutex_);
        auto stop_and_clear = [](auto& receiver) {
            if (receiver) {
                receiver->stop();
                receiver = nullptr;
            }
        };
        stop_and_clear(tcp_receiver_);
        stop_and_clear(uds_receiver_);
    }

    if (version_logger_) {
        version_logger_->stop();
    }
    auto when_no_provider_eps = when_provider_eps_flushed.when_not_within(std::chrono::milliseconds(500), [weak_self = weak_from_this()] {
        VSOMEIP_WARNING << "rmc::stop: producer endpoints where not flushed within time. Enforcing stop";
        if (auto self = weak_self.lock(); self) {
            self->ep_mgr_->force_stop();
        }
    });
    auto when_consumer_eps_flushed = flush_consumer();
    auto when_no_consumer_eps = when_consumer_eps_flushed.when_not_within(std::chrono::milliseconds(500), [weak_self = weak_from_this()] {
        VSOMEIP_WARNING << "rmc::stop: consumer endpoints where not flushed within time. Enforcing stop";
        if (auto self = weak_self.lock(); self) {
            self->cleanup_consumer();
        }
    });
    auto when_no_sender = when_sender_stopped.when_not_within(std::chrono::milliseconds(500), [weak_self = weak_from_this(), this] {
        VSOMEIP_WARNING << "rmc::stop: sender was not flushed within time. Enforcing stop";
        if (auto self = weak_self.lock(); self) {
            std::scoped_lock its_sender_lock{sender_mutex_};
            if (sender_) {
                sender_->stop(true);
                sender_ = nullptr;
            }
            // hook is no longer required to be kept around
            if (on_sender_stopped_) {
                on_sender_stopped_ = {};
            }
        }
    });
    auto when_no_eps = async::when_all(when_no_provider_eps, when_no_consumer_eps);
    auto when_all_stopped = async::when_all(when_no_eps, when_no_sender);
    return when_all_stopped.then([weak_self = weak_from_this()] {
        if (auto self = weak_self.lock(); self) {
            VSOMEIP_INFO << "rmc::stop: All endpoints cleared. Finishing the shutdown";
            // once all endpoints have been cleared -> this function is going to be invoked
            self->finish_shutdown();
        }
    });
}

std::shared_ptr<configuration> routing_manager_client::get_configuration() const {
    return host_->get_configuration();
}

bool routing_manager_client::offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                           minor_version_t _minor) {

    std::scoped_lock its_lock(provider_mutex_);
    if (auto its_info = offered_services_.find(protocol::service_data{_service, _instance, _major, _minor}); its_info) {
        if (its_info->major_version_ != _major || its_info->minor_version_ != _minor) {
            VSOMEIP_ERROR_P << "Service property mismatch (" << hex4(_client) << "): " << *its_info
                            << " passed: " << static_cast<std::uint32_t>(_major) << ":" << _minor;
            return false;
        }
        return true; // we are already offering this service -> no need to do anything else!
    }

    // Set major version for all registered events of this service and instance
    const auto search = provided_events_.find(service_instance_t{_service, _instance});

    if (search != provided_events_.end()) {
        for (const auto& [event_id, event_ptr] : search->second) {
            event_ptr->set_version(_major);
        }
    }

    // order matters:
    // 1. Ensure that it is part of the pending_offers set
    // 2. check for the state
    // otherwise:
    // state might be not be registered, but turn registered just after the check,
    // rushing ahead sending the pending offers that do not contain this offer yet.
    protocol::service_data offer_data{.service_ = _service, .instance_ = _instance, .major_version_ = _major, .minor_version_ = _minor};
    offered_services_.insert(offer_data);
    if (state_machine_->state() == routing_client_state_e::ST_REGISTERED) {
        send_offer_service(offer_data);
    }

    return true;
}

bool routing_manager_client::send_offer_service(protocol::service_data const& _data) {

    std::scoped_lock its_sender_lock{sender_mutex_};
    if (sender_
        && sender_->send(protocol::create_offer_service_cmd(get_client(), _data.service_, _data.instance_, _data.major_version_,
                                                            _data.minor_version_))) {
        return true;
    }

    VSOMEIP_ERROR_P << "Failure offering service " << _data;
    return false;
}

void routing_manager_client::stop_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                minor_version_t _minor) {

    (void)_client;
    std::scoped_lock its_lock(provider_mutex_);
    stop_offer_service_base(_client, _service, _instance, _major, _minor, its_lock);
    clear_remote_subscriber_count(_service, _instance, its_lock);

    // order matters:
    // 1. Remove the offer from the set,
    // 2. Send the removal if we are registered
    // otherwise it might happen that we don't send the stop offer,
    // but have not removed the offer when REGISTERED is entered
    offered_services_.remove(
            protocol::service_data{.service_ = _service, .instance_ = _instance, .major_version_ = _major, .minor_version_ = _minor});
    if (state_machine_->state() == routing_client_state_e::ST_REGISTERED) {
        std::scoped_lock its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(protocol::create_stop_offer_service_cmd(get_client(), _service, _instance, _major, _minor));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    }
}

void routing_manager_client::request_service([[maybe_unused]] client_t _client, service_t _service, instance_t _instance,
                                             major_version_t _major, minor_version_t _minor) {

    {
        size_t request_debouncing_time = configuration_->get_request_debounce_time(host_->get_name());
        protocol::service_data request = {.service_ = _service, .instance_ = _instance, .major_version_ = _major, .minor_version_ = _minor};
        std::scoped_lock its_lock{consumer_mutex_};
        if (requests_.contains(request) || requests_to_debounce_.contains(request)) {
            return;
        }
        if (!request_debouncing_time) {
            // order matters:
            // 1. Ensure that the set contains this request
            // 2. Try to send if we are registered
            // If exchanged the sending after the registration is going to race with
            // the subsequent logic.
            requests_.insert(request);
            if (state_machine_->state() == routing_client_state_e::ST_REGISTERED) {
                local_service_table requests;
                requests.insert(request);
                send_request_services(requests.view());
            }
        } else {
            requests_to_debounce_.insert(request);
            if (!request_debounce_timer_running_) {
                request_debounce_timer_running_ = true;
                request_debounce_timer_.expires_after(std::chrono::milliseconds(request_debouncing_time));
                request_debounce_timer_.async_wait(std::bind(&routing_manager_client::request_debounce_timeout_cbk,
                                                             std::dynamic_pointer_cast<routing_manager_client>(shared_from_this()),
                                                             std::placeholders::_1));
            }
        }
    }
}

void routing_manager_client::release_service(client_t _client, service_t _service, instance_t _instance) {
    bool already_requested(false);
    {
        std::scoped_lock its_service_guard(consumer_mutex_);
        remove_pending_subscription(_service, _instance, 0xFFFF, ANY_EVENT, its_service_guard);
        // Clear subscription bookkeeping from consumed_events_ entries; erase orphan entries
        // (null event_ created by subscribe() before register_event() was called).
        if (auto its_si = consumed_events_.find({_service, _instance}); its_si != consumed_events_.end()) {
            for (auto& [ev_id, entry] : its_si->second) {
                entry.subscriptions_.clear();
            }
            std::erase_if(its_si->second, [](const auto& kv) { return !kv.second.event_; });
        }
        protocol::service_data request{
                .service_ = _service, .instance_ = _instance, .major_version_ = ANY_MAJOR, .minor_version_ = ANY_MINOR};
        requests_to_debounce_.remove(request);
        already_requested = requests_.remove(request);
    }
    if (already_requested && state_machine_->state() == routing_client_state_e::ST_REGISTERED) {
        std::scoped_lock its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(protocol::create_release_service_cmd(_client, _service, _instance));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    }
}

void routing_manager_client::register_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                            const std::set<eventgroup_t>& _eventgroups, const event_type_e _type,
                                            reliability_type_e _reliability, std::chrono::milliseconds _cycle, bool _change_resets_cycle,
                                            bool _update_on_change, epsilon_change_func_t _epsilon_change_func, bool _is_provided) {

    bool is_cyclic(_cycle != std::chrono::milliseconds::zero());

    const protocol::register_event_data reg_event_data{.service_ = _service,
                                                       .instance_ = _instance,
                                                       .event_ = _notifier,
                                                       .event_type_ = _type,
                                                       .is_provided_ = _is_provided,
                                                       .reliability_ = _reliability,
                                                       .is_cyclic_ = is_cyclic,
                                                       .eventgroups_ = {_eventgroups.begin(), _eventgroups.end()}};
    bool new_registration(false);
    auto is_new = [&reg_event_data](std::vector<protocol::register_event_data> const& _regs) {
        return std::none_of(_regs.begin(), _regs.end(),
                            [&reg_event_data](protocol::register_event_data const& _reg) { return _reg == reg_event_data; });
    };
    if (_is_provided) {
        std::scoped_lock its_lock{provider_mutex_};
        new_registration = is_new(pending_provided_event_registrations_);
        if (new_registration) {
            pending_provided_event_registrations_.push_back(reg_event_data);
            register_provider_event(_client, _service, _instance, _notifier, _eventgroups, _type, _reliability, _cycle,
                                    _change_resets_cycle, _update_on_change, _epsilon_change_func, false, its_lock);
        }
    } else {
        std::scoped_lock its_lock{consumer_mutex_};
        new_registration = is_new(pending_consumed_event_registrations_);
        if (new_registration) {
            pending_consumed_event_registrations_.push_back(reg_event_data);
            register_consumer_event(_client, _service, _instance, _notifier, _eventgroups, _type, _reliability, _cycle,
                                    _change_resets_cycle, _update_on_change, _epsilon_change_func, false, its_lock);
        }
    }
    if (state_machine_->state() == routing_client_state_e::ST_REGISTERED && new_registration) {
        send_event_registrations(get_client(), std::span{&reg_event_data, 1});
        if (_is_provided) {
            VSOMEIP_INFO << "REGISTER EVENT(" << hex4(get_client()) << "): [" << hex4(_service) << "." << hex4(_instance) << "."
                         << hex4(_notifier) << ":is_provider=" << std::boolalpha << _is_provided << "]";
        }
    }
}

void routing_manager_client::unregister_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                              bool _is_provided) {

    unregister_event_base(_client, _service, _instance, _notifier, _is_provided);

    // order matters:
    // 1. Remove the event from the pending events
    // 2. Try to send the unregister command
    // Otherwise we might not send the command, but request it when entering REGISTERED
    {
        auto erase_from = [&](std::vector<protocol::register_event_data>& _regs) {
            auto its_reg = std::find_if(_regs.begin(), _regs.end(), [&](protocol::register_event_data const& _reg) {
                return _reg.service_ == _service && _reg.instance_ == _instance && _reg.event_ == _notifier;
            });
            if (its_reg != _regs.end()) {
                _regs.erase(its_reg);
            }
        };
        if (_is_provided) {
            std::scoped_lock its_lock(provider_mutex_);
            erase_from(pending_provided_event_registrations_);
        } else {
            std::scoped_lock its_lock(consumer_mutex_);
            erase_from(pending_consumed_event_registrations_);
        }
    }
    if (state_machine_->state() == routing_client_state_e::ST_REGISTERED) {

        std::scoped_lock its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(protocol::create_unregister_event_cmd(get_client(), _service, _instance, _notifier, _is_provided));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    }
}

void routing_manager_client::subscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                       major_version_t _major, event_t _event, const std::shared_ptr<debounce_filter_impl_t>& _filter) {

    (void)_client;

    bool send{false};
    {
        std::scoped_lock its_lock{consumer_mutex_};

        auto& its_consumed_event = consumed_events_[{_service, _instance}][_event];
        // A concrete event must be registered (register_event) before subscribe(); a null event_ here is a usage error.
        if (_event != ANY_EVENT && !its_consumed_event.event_) {
            VSOMEIP_ERROR_P << "(" << hex4(get_client()) << "): [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(_eventgroup)
                            << ":" << hex4(_event) << "] subscribe called before register_event ~> the event is not registered yet.";
        }
        auto& its_subscriptions = its_consumed_event.subscriptions_;
        auto found_eg = its_subscriptions.find(_eventgroup);

        // Check if initial event was already received for this eventgroup, replay the cached value.
        if (found_eg != its_subscriptions.end() && found_eg->second.initial_notification_received_) {
            if (_event == ANY_EVENT) {
                send_back_cached_eventgroup_unlocked(_service, _instance, _eventgroup, its_lock);
            } else {
                send_back_cached_event_unlocked(_service, _instance, _event, its_lock);
            }
        }

        // Always register in pending_subscriptions_ so we re-subscribe on reconnect
        subscription_data_t subscription = {service_instance_t{_service, _instance}, _eventgroup, _major, _event, _filter};
        pending_subscriptions_.insert(subscription);

        // Check/update subscription state
        if (found_eg != its_subscriptions.end()) {
            if (found_eg->second.state_ == subscription_state_e::SUBSCRIPTION_ACKNOWLEDGED) {
                host_->on_subscription_status(_service, _instance, _eventgroup, _event, 0 /* OK */);
                // ACKNOWLEDGED: already subscribed, do not re-send
            } else if (found_eg->second.state_ == subscription_state_e::SUBSCRIPTION_NOT_ACKNOWLEDGED) {
                // A previous subscription was NACKed: retry it (re-enter IS_SUBSCRIBING and re-send).
                found_eg->second.state_ = subscription_state_e::IS_SUBSCRIBING;
                if (state_machine_->state() == routing_client_state_e::ST_REGISTERED
                    && available_services_.is_available(_service, _instance, _major)) {
                    send = true;
                }
            }
            // IS_SUBSCRIBING: subscribe still in flight, do not re-send
        } else {
            its_subscriptions[_eventgroup].state_ = subscription_state_e::IS_SUBSCRIBING;
            if (state_machine_->state() == routing_client_state_e::ST_REGISTERED
                && available_services_.is_available(_service, _instance, _major)) {
                send = true;
            }
        }
    }

    if (send) {
        send_subscribe(get_client(), _service, _instance, _eventgroup, _major, _event, _filter);
    }
}

void routing_manager_client::send_subscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                            major_version_t _major, event_t _event,
                                            const std::shared_ptr<debounce_filter_impl_t>& _filter) {

    if (_event == ANY_EVENT) {
        auto const sec_client = get_sec_client();
        if (!is_subscribe_to_any_event_allowed(&sec_client, _client, _service, _instance, _eventgroup, false)) {
            VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(_client) << " : routing_manager_proxy::subscribe: "
                          << " isn't allowed to subscribe to service/instance/event " << hex4(_service) << "/" << hex4(_instance)
                          << "/ANY_EVENT which violates the security policy ~> Skip subscribe!";
            return;
        }
    } else {
        auto const sec_client = get_sec_client();
        if (VSOMEIP_SEC_OK
            != configuration_->get_security()->is_client_allowed_to_access_member(&sec_client, _service, _instance, _event)) {
            VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(_client) << " : routing_manager_proxy::subscribe: "
                          << " isn't allowed to subscribe to service/instance/event " << hex4(_service) << "/" << hex4(_instance) << "/"
                          << hex4(_event);
            return;
        }
    }

    auto const cmd = protocol::create_subscribe_cmd(_client, _filter,
                                                    protocol::subscribe_data{.service_ = _service,
                                                                             .instance_ = _instance,
                                                                             .eventgroup_ = _eventgroup,
                                                                             .major_ = _major,
                                                                             .event_ = _event,
                                                                             .pending_id_ = PENDING_SUBSCRIPTION_ID});
    client_t its_target_client = find_local_client(_service, _instance);
    if (its_target_client != VSOMEIP_ROUTING_CLIENT) {
        auto its_target = find_or_create_consumer_ep(its_target_client);
        if (its_target) {
            its_target->send(cmd);
        } else {
            VSOMEIP_WARNING_P << "No target available to send subscription. Client=0x" << hex4(_client) << " service=" << hex4(_service)
                              << "." << hex4(_instance) << "." << hex2(_major) << " event=" << hex4(_event);
        }
    } else {
        std::scoped_lock its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(cmd);
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    }
}

void routing_manager_client::send_subscribe_nack(client_t _subscriber, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                 event_t _event, remote_subscription_id_t _id) {

    auto cmd = protocol::create_subscribe_nack_cmd(get_client(),
                                                   protocol::subscribe_answer_data{.service_ = _service,
                                                                                   .instance_ = _instance,
                                                                                   .eventgroup_ = _eventgroup,
                                                                                   .subscriber_ = _subscriber,
                                                                                   .event_ = _event,
                                                                                   .pending_id_ = _id});
    if (_subscriber != VSOMEIP_ROUTING_CLIENT && _id == PENDING_SUBSCRIPTION_ID) {
        auto its_target = ep_mgr_->find_local_server_endpoint(_subscriber);
        if (its_target) {
            its_target->send(cmd);
            return;
        } else {
            VSOMEIP_WARNING_P << "No target available to send subscription nack. Client=0x" << hex4(_subscriber)
                              << " service=" << hex4(_service) << "." << hex4(_instance) << " event=" << hex4(_event);
        }
    }
    {
        std::scoped_lock its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(cmd);
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    }
}

void routing_manager_client::send_subscribe_ack(client_t _subscriber, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                event_t _event, remote_subscription_id_t _id) {

    auto cmd = protocol::create_subscribe_ack_cmd(get_client(),
                                                  protocol::subscribe_answer_data{.service_ = _service,
                                                                                  .instance_ = _instance,
                                                                                  .eventgroup_ = _eventgroup,
                                                                                  .subscriber_ = _subscriber,
                                                                                  .event_ = _event,
                                                                                  .pending_id_ = _id});
    if (_subscriber != VSOMEIP_ROUTING_CLIENT && _id == PENDING_SUBSCRIPTION_ID) {
        auto its_target = ep_mgr_->find_local_server_endpoint(_subscriber);
        if (its_target) {
            its_target->send(cmd);
            return;
        } else {
            VSOMEIP_WARNING_P << "No target available to send subscription ack. Client=0x" << hex4(_subscriber)
                              << " service=" << hex4(_service) << "." << hex4(_instance) << " event=" << hex4(_event);
        }
    }
    {
        std::scoped_lock its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(cmd);
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    }
}

void routing_manager_client::unsubscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                         event_t _event) {

    (void)_client;

    {
        {
            std::scoped_lock its_service_guard(consumer_mutex_);
            remove_pending_subscription(_service, _instance, _eventgroup, _event, its_service_guard);

            // Cleanup consumed_events_ subscription state and initial notification flags
            auto its_si = consumed_events_.find({_service, _instance});
            if (its_si != consumed_events_.end()) {
                if (_event == ANY_EVENT) {
                    for (auto& [ev_id, entry] : its_si->second) {
                        entry.subscriptions_.erase(_eventgroup);
                    }
                    std::erase_if(its_si->second, [](const auto& kv) { return !kv.second.event_ && kv.second.subscriptions_.empty(); });
                } else {
                    auto found_entry = its_si->second.find(_event);
                    if (found_entry != its_si->second.end()) {
                        auto& entry = found_entry->second;
                        entry.subscriptions_.erase(_eventgroup);
                        if (!entry.event_ && entry.subscriptions_.empty()) {
                            its_si->second.erase(found_entry);
                        }
                    }
                }
            }
        }

        if (state_machine_->state() == routing_client_state_e::ST_REGISTERED) {

            auto cmd = protocol::create_unsubscribe_cmd(_client,
                                                        protocol::subscribe_data{.service_ = _service,
                                                                                 .instance_ = _instance,
                                                                                 .eventgroup_ = _eventgroup,
                                                                                 .major_ = ANY_MAJOR,
                                                                                 .event_ = _event,
                                                                                 .pending_id_ = PENDING_SUBSCRIPTION_ID});
            auto its_target = find_consumer_ep(find_local_client(_service, _instance));
            if (its_target) {
                its_target->send(cmd);
            } else {
                if (_client != VSOMEIP_ROUTING_CLIENT) {
                    VSOMEIP_WARNING_P << "Could not find endpoint for client 0x" << hex4(_client) << ", sending to the router";
                }
                std::scoped_lock its_sender_lock{sender_mutex_};
                if (sender_) {
                    sender_->send(cmd);
                } else {
                    VSOMEIP_ERROR_P << "Failed due to a missing sender";
                }
            }
        }
    }
}

void routing_manager_client::on_message(const byte_t* _data, length_t _size, const local_client_data& _peer_data) {
    protocol::command_header its_header{};
    if (uint32_t parsed_hdr_bytes = protocol::deserialize(its_header, _data, _size); parsed_hdr_bytes) {
        protocol::id_e its_id = its_header.id_;
        client_t const its_client = its_header.client_;

        bool is_from_routing = (_peer_data.id_ == VSOMEIP_ROUTING_CLIENT);

        if (!is_from_routing && _peer_data.id_ != its_client) {
            VSOMEIP_ERROR_P << "Client 0x" << hex4(get_client()) << " received a message with command " << its_id << " from "
                            << hex4(its_client) << " which doesn't match the bound client " << hex4(_peer_data.id_) << " ~> skip message!";
            return;
        }

#ifndef VSOMEIP_DISABLE_SECURITY
        bool is_internal_policy_update = false;
#endif
        switch (its_id) {
        case protocol::id_e::SEND_ID: {
            if (std::shared_ptr<message_impl> its_message;
                protocol::deserialize(its_message, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
                its_message->set_sec_client(_peer_data.sec_client_);
                its_message->set_env(_peer_data.env_);

                bool has_active_subscription{true};

                if (!is_from_routing) {
                    if (utility::is_request(its_message->get_message_type())) {
                        // NOTE: its_header.client_ and message::get_client are different fields, the extra check is needed
                        if (its_message->get_client() != _peer_data.id_) {
                            VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(get_client()) << " received a request from client 0x"
                                          << hex4(its_message->get_client()) << " to service/instance/method "
                                          << hex4(its_message->get_service()) << "/" << hex4(its_message->get_instance()) << "/"
                                          << hex4(its_message->get_method()) << " which doesn't match the bound client 0x"
                                          << hex4(_peer_data.id_) << " ~> skip message!";
                            return;
                        }

                        if (VSOMEIP_SEC_OK
                            != configuration_->get_security()->is_client_allowed_to_access_member(
                                    &_peer_data.sec_client_, its_message->get_service(), its_message->get_instance(),
                                    its_message->get_method())) {
                            VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(its_message->get_client())
                                          << " : routing_manager_client::on_message: " << hex4(its_message->get_client())
                                          << " isn't allowed to send a request to service/instance/method "
                                          << hex4(its_message->get_service()) << "/" << hex4(its_message->get_instance()) << "/"
                                          << hex4(its_message->get_method()) << " ~> Skip message!";
                            return;
                        }
                    } else { // Notification or Response
                        // TODO for external security ports are checked.
                        // Notification and responses were originally send out by the "sender".
                        // With the refactoring towards client-server we have to temporarily "lie"
                        // to security about the port we received the message from...
                        auto sec_client = _peer_data.sec_client_;

                        // If port is VSOMEIP_SEC_PORT_UNUSED (0) the connection is over UDS.
                        // Subtracting 1 from 0 would wrap around to VSOMEIP_SEC_PORT_UNSET
                        // (0xFFFF) which is not a valid registered port
                        if (ntohs(_peer_data.sec_client_.port) != VSOMEIP_SEC_PORT_UNUSED) {
                            sec_client.port = htons(ntohs(_peer_data.sec_client_.port) - 1);
                        }

                        // Verifies security offer rule for messages (notifications and
                        // responses).
                        bool is_offer_access_ok = (VSOMEIP_SEC_OK
                                                   == configuration_->get_security()->is_client_allowed_to_offer(
                                                           &sec_client, its_message->get_service(), its_message->get_instance()));

                        if (!is_offer_access_ok) {
                            VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(get_client())
                                          << " : routing_manager_client::on_message: received a "
                                          << (utility::is_notification(its_message->get_message_type()) ? "notification" : "response")
                                          << " from client 0x" << hex4(_peer_data.id_) << " which does not offer service/instance/method "
                                          << hex4(its_message->get_service()) << "/" << hex4(its_message->get_instance()) << "/"
                                          << hex4(its_message->get_method()) << " ~> Skip message!";
                            return;
                        }

                        const bool is_notification = utility::is_notification(its_message->get_message_type());

                        if (is_notification) {
                            auto const my_sec_client = get_sec_client();
                            const bool is_access_member_ok = (VSOMEIP_SEC_OK
                                                              == configuration_->get_security()->is_client_allowed_to_access_member(
                                                                      &my_sec_client, its_message->get_service(),
                                                                      its_message->get_instance(), its_message->get_method()));

                            if (!is_access_member_ok) {
                                VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(its_message->get_client())
                                              << " : routing_manager_client::on_message: " << hex4(get_client())
                                              << " : routing_manager_client::on_message: isn't allowed to receive a "
                                              << " notification from service/instance/method " << hex4(its_message->get_service()) << "/"
                                              << hex4(its_message->get_instance()) << "/" << hex4(its_message->get_method())
                                              << " respectively from client 0x" << hex4(_peer_data.id_) << " ~> Skip message!";
                                return;
                            }
                            has_active_subscription = cache_event_payload(its_message);
                        }
                    }
                } else {
                    if (!configuration_->is_remote_access_allowed()) {
                        // if the message is from routing manager, check if
                        // policy allows remote requests.
                        VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(get_client())
                                      << " : routing_manager_client::on_message: Security: Remote clients via routing manager with "
                                      << "client ID 0x" << hex4(its_client)
                                      << " are not allowed to communicate with service/instance/method " << hex4(its_message->get_service())
                                      << "/" << hex4(its_message->get_instance()) << "/" << hex4(its_message->get_method())
                                      << " respectively with client 0x" << hex4(get_client()) << " ~> Skip message!";
                        return;
                    } else if (utility::is_notification(its_message->get_message_type())) {
                        // As subscription is sent on eventgroup level, incoming remote event
                        // ID's need to be checked as well if remote clients are allowed and the
                        // local policy only allows specific events in the eventgroup to be
                        // received.

                        auto const my_sec_client = get_sec_client();
                        if (VSOMEIP_SEC_OK
                            != configuration_->get_security()->is_client_allowed_to_access_member(
                                    &my_sec_client, its_message->get_service(), its_message->get_instance(), its_message->get_method())) {
                            VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(get_client())
                                          << " : routing_manager_client::on_message: "
                                          << " isn't allowed to receive a notification from service/instance/event "
                                          << hex4(its_message->get_service()) << "/" << hex4(its_message->get_instance()) << "/"
                                          << hex4(its_message->get_method()) << " ~> Skip message!";
                            return;
                        }
                        has_active_subscription = cache_event_payload(its_message);
                    }
                }

                if (client_side_logging_
                    && (client_side_logging_filter_.empty()
                        || (1 == client_side_logging_filter_.count(std::make_tuple(its_message->get_service(), ANY_INSTANCE)))
                        || (1
                            == client_side_logging_filter_.count(
                                    std::make_tuple(its_message->get_service(), its_message->get_instance()))))) {
                    trace::header its_header;
                    if (its_header.prepare(nullptr, false, its_message->get_instance(), trace::protocol_e::unknown)) {
                        uint32_t offset = parsed_hdr_bytes + protocol::ipc_message_header::wire_size_;
                        if (offset < _size) {
                            tc_->trace(its_header.data_, VSOMEIP_TRACE_HEADER_SIZE, _data + offset, _size - offset);
                        }
                    }
                }

                if (utility::is_notification(its_message->get_message_type()) && !has_active_subscription) {
                    VSOMEIP_WARNING_P << "[" << hex4(its_message->get_service()) << "." << hex4(its_message->get_instance()) << "."
                                      << hex4(its_message->get_method()) << "]: blocked as the subscription is already inactive.";
                    break;
                }

                host_->on_message(std::move(its_message));

            } else {
                VSOMEIP_ERROR_P << "Send command deserialization failed: " << utility::dump(_data, _size);
            }
            break;
        }

        case protocol::id_e::ASSIGN_CLIENT_ACK_ID: {
            if (client_t new_id; protocol::deserialize(new_id, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
                on_client_assign_ack(new_id, !_peer_data.routing_address_.is_unspecified());
            } else {
                VSOMEIP_ERROR_P << "Assign client ack command deserialization failed memory: " << utility::dump(_data, _size);
            }
            break;
        }

        case protocol::id_e::ROUTING_INFO_ID:
            if (is_from_routing) {
                on_routing_info(_data + parsed_hdr_bytes, _size - parsed_hdr_bytes);
            } else {
                VSOMEIP_ERROR_P << "Client 0x" << hex4(get_client()) << " received routing_info from client 0x" << hex4(its_client)
                                << " which is not the router!";
            }
            break;

        case protocol::id_e::PING_ID: {
            VSOMEIP_INFO << "PING(" << hex4(get_client()) << ")";
            send_pong();
            break;
        }

        case protocol::id_e::SUBSCRIBE_ID: {
            if (protocol::subscribe_with_filter_data its_data;
                protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

                auto its_service = its_data.data_.service_;
                auto its_instance = its_data.data_.instance_;
                auto its_eventgroup = its_data.data_.eventgroup_;
                auto its_major = its_data.data_.major_;
                auto its_event = its_data.data_.event_;
                auto its_pending_id = its_data.data_.pending_id_;
                auto its_filter = its_data.filter_;

                if (its_pending_id != PENDING_SUBSCRIPTION_ID) {
                    std::scoped_lock lock{provider_mutex_};
                    if (auto its_info = offered_services_.find({its_service, its_instance, its_major, ANY_MINOR}); its_info) {
                        // Remote subscriber: Notify routing manager initially + count subscribes
                        auto self = shared_from_this();
                        host_->on_subscription(
                                its_service, its_instance, its_eventgroup, its_client, &_peer_data.sec_client_, _peer_data.env_, true,
                                [this, self, its_client, its_service, its_instance, its_eventgroup, its_event, its_filter, its_pending_id,
                                 its_major, its_generation = _peer_data.lc_token_](const bool _subscription_accepted) {
                                    // lock before asking for the token to ensure that an intermediate clean-up is honored
                                    std::scoped_lock its_lock{provider_mutex_};
                                    // remote subscriptions have to go through the router. If there is some lc change in the sender, all
                                    // former subscriptions need to be dropped (the router will re-subscribe)
                                    if (its_generation != lc_count_) {
                                        VSOMEIP_INFO << "SUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "."
                                                     << hex4(its_instance) << "." << hex4(its_eventgroup) << ":" << hex4(its_event)
                                                     << "] router connection superseded, discarding stale continuation.";
                                        return;
                                    }
                                    std::uint32_t its_count(0);
                                    if (_subscription_accepted) {
                                        insert_subscription(its_service, its_instance, its_eventgroup, its_event, its_filter,
                                                            VSOMEIP_ROUTING_CLIENT, its_lock);
                                        // NOTE: order matters, send ACK _after_ inserting the subscription
                                        send_subscribe_ack(its_client, its_service, its_instance, its_eventgroup, its_event,
                                                           its_pending_id);
                                        notify_remote_initially(its_service, its_instance, its_eventgroup, its_lock);

                                        its_count = get_remote_subscriber_count(its_service, its_instance, its_eventgroup, true, its_lock);
                                    } else {
                                        send_subscribe_nack(its_client, its_service, its_instance, its_eventgroup, its_event,
                                                            its_pending_id);
                                    }
                                    VSOMEIP_INFO << "SUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "."
                                                 << hex4(its_instance) << "." << hex4(its_eventgroup) << ":" << hex4(its_event) << ":"
                                                 << static_cast<uint16_t>(its_major) << "] "
                                                 << (_subscription_accepted ? "accepted." : "not accepted.")
                                                 << " id=" << hex4(its_pending_id) << " subscribers=" << its_count;
                                });
                    } else {
                        send_subscribe_nack(its_client, its_service, its_instance, its_eventgroup, its_event, its_pending_id);
                    }
                } else { // local subscription
                    if (!is_from_routing) {
                        if (its_event == ANY_EVENT) {
                            if (!is_subscribe_to_any_event_allowed(&_peer_data.sec_client_, its_client, its_service, its_instance,
                                                                   its_eventgroup, true)) {
                                VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(its_client)
                                              << " : routing_manager_client::on_message: isn't allowed to subscribe to"
                                              << " service/instance/event " << hex4(its_service) << "/" << hex4(its_instance)
                                              << "/ANY_EVENT which violates the security policy ~> Skip subscribe!";
                                return;
                            }
                        } else {
                            if (VSOMEIP_SEC_OK
                                != configuration_->get_security()->is_client_allowed_to_access_member(&_peer_data.sec_client_, its_service,
                                                                                                      its_instance, its_event)) {
                                VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(its_client)
                                              << " : routing_manager_client::on_message: "
                                              << " subscribes to service/instance/event " << hex4(its_service) << "/" << hex4(its_instance)
                                              << "/" << its_event << " which violates the security policy ~> Skip subscribe!";
                                return;
                            }
                        }
                    } else {
                        if (!configuration_->is_remote_access_allowed()) {
                            VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(its_client)
                                          << " : routing_manager_client::on_message: " << hex4(its_client)
                                          << "Routing manager with client ID 0x" << hex4(its_client)
                                          << " isn't allowed to subscribe to service/instance/event " << hex4(its_service) << "/"
                                          << hex4(its_instance) << "/" << its_event << " respectively to client 0x" << hex4(get_client())
                                          << " ~> Skip Subscribe!";
                            return;
                        }
                    }

                    std::scoped_lock lock{provider_mutex_};
                    auto self = shared_from_this();
                    if (auto its_info = offered_services_.find({its_service, its_instance, its_major, ANY_MINOR}); its_info) {
                        host_->on_subscription(
                                its_service, its_instance, its_eventgroup, its_client, &_peer_data.sec_client_, _peer_data.env_, true,
                                [this, self, its_client, its_filter, its_pending_id, its_service, its_instance, its_eventgroup, its_event,
                                 its_major, its_generation = _peer_data.lc_token_](const bool _subscription_accepted) {
                                    // lock before asking for the token to ensure that an intermediate clean-up is honored
                                    std::scoped_lock its_lock{provider_mutex_};
                                    // a peer subscription needs to be bound to the peers connection -> use the corresponding token for
                                    // this client id
                                    if (its_generation != ep_mgr_->provider_connection_token(its_client)) {
                                        VSOMEIP_INFO << "SUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "."
                                                     << hex4(its_instance) << "." << hex4(its_eventgroup) << ":" << hex4(its_event)
                                                     << "] client connection superseded, discarding stale continuation.";
                                        return;
                                    }
                                    if (!_subscription_accepted) {
                                        send_subscribe_nack(its_client, its_service, its_instance, its_eventgroup, its_event,
                                                            PENDING_SUBSCRIPTION_ID);
                                    } else {

                                        insert_subscription(its_service, its_instance, its_eventgroup, its_event, its_filter, its_client,
                                                            its_lock);
                                        // NOTE: order matters, send ACK _after_ inserting
                                        // the subscription
                                        send_subscribe_ack(its_client, its_service, its_instance, its_eventgroup, its_event,
                                                           PENDING_SUBSCRIPTION_ID);
                                        notify_one_current_value(its_client, its_service, its_instance, its_eventgroup, its_event,
                                                                 its_lock);
                                    }

                                    VSOMEIP_INFO << "SUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "."
                                                 << hex4(its_instance) << "." << hex4(its_eventgroup) << ":" << hex4(its_event) << ":"
                                                 << static_cast<uint16_t>(its_major) << "] " << std::boolalpha
                                                 << (its_pending_id != PENDING_SUBSCRIPTION_ID)
                                                 << (_subscription_accepted ? " accepted" : "not accepted");
                                });
                    } else {
                        send_subscribe_nack(its_client, its_service, its_instance, its_eventgroup, its_event, PENDING_SUBSCRIPTION_ID);
                    }
                }
                VSOMEIP_INFO << "SUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_instance) << "."
                             << hex4(its_eventgroup) << ":" << hex4(its_event) << ":" << static_cast<uint16_t>(its_major) << "] "
                             << std::boolalpha << (its_pending_id != PENDING_SUBSCRIPTION_ID);
            } else {
                VSOMEIP_ERROR_P << "Subscribe command deserialization failed:" << utility::dump(_data, _size);
            }
            break;
        }

        case protocol::id_e::UNSUBSCRIBE_ID: {
            if (protocol::subscribe_data its_data; protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

                auto its_service = its_data.service_;
                auto its_instance = its_data.instance_;
                auto its_eventgroup = its_data.eventgroup_;
                auto its_event = its_data.event_;
                auto its_pending_id = its_data.pending_id_;

                // Notify user asynchronously (informational only; unsubscription cannot be rejected)
                host_->on_subscription(
                        its_service, its_instance, its_eventgroup, its_client, &_peer_data.sec_client_, _peer_data.env_, false,
                        [this, self = shared_from_this(), its_pending_id, its_client, its_service, its_instance, its_eventgroup, its_event,
                         its_generation = _peer_data.lc_token_](bool) {
                            // lock before asking for the token to ensure that an intermediate clean-up is honored
                            std::scoped_lock its_lock{provider_mutex_};
                            if (its_generation
                                != (its_pending_id != PENDING_SUBSCRIPTION_ID ? lc_count_.load()
                                                                              : ep_mgr_->provider_connection_token(its_client))) {
                                VSOMEIP_INFO << "UNSUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "."
                                             << hex4(its_instance) << "." << hex4(its_eventgroup) << ":" << hex4(its_event) << "] "
                                             << std::boolalpha << (its_pending_id != PENDING_SUBSCRIPTION_ID)
                                             << " client connection superseded, discarding stale continuation.";
                                return;
                            }
                            std::uint32_t its_remote_subscriber_count(0);
                            if (its_pending_id == PENDING_SUBSCRIPTION_ID) {
                                // Local subscriber: withdraw subscription
                                unsubscribe_base(its_client, its_service, its_instance, its_eventgroup, its_event, its_lock);
                            } else {
                                // Remote subscriber: withdraw subscription only if no more remote subscriber exists
                                its_remote_subscriber_count =
                                        get_remote_subscriber_count(its_service, its_instance, its_eventgroup, false, its_lock);
                                if (!its_remote_subscriber_count) {
                                    unsubscribe_base(VSOMEIP_ROUTING_CLIENT, its_service, its_instance, its_eventgroup, its_event,
                                                     its_lock);
                                }
                                std::scoped_lock its_sender_lock{sender_mutex_};
                                if (sender_) {
                                    sender_->send(protocol::create_unsubscribe_ack_cmd(get_client(), its_service, its_instance,
                                                                                       its_eventgroup, its_pending_id));
                                } else {
                                    VSOMEIP_ERROR_P << "Failed due to a missing sender";
                                }
                            }
                            VSOMEIP_INFO << "UNSUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_instance)
                                         << "." << hex4(its_eventgroup) << "." << hex4(its_event) << "] " << std::boolalpha
                                         << (its_pending_id != PENDING_SUBSCRIPTION_ID) << " subscribers=" << its_remote_subscriber_count;
                        });
                VSOMEIP_INFO << "UNSUBSCRIBE(" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_instance) << "."
                             << hex4(its_eventgroup) << "." << hex4(its_event) << "] " << std::boolalpha
                             << (its_pending_id != PENDING_SUBSCRIPTION_ID);

            } else {
                VSOMEIP_ERROR_P << "Unsubscribe command deserialization failed: " << utility::dump(_data, _size);
            }
            break;
        }

        case protocol::id_e::EXPIRE_ID: {
            if (protocol::subscribe_data its_data; protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

                auto its_service = its_data.service_;
                auto its_instance = its_data.instance_;
                auto its_eventgroup = its_data.eventgroup_;
                auto its_event = its_data.event_;
                auto its_pending_id = its_data.pending_id_;

                host_->on_subscription(
                        its_service, its_instance, its_eventgroup, its_client, &_peer_data.sec_client_, _peer_data.env_, false,
                        [this, self = shared_from_this(), its_pending_id, its_client, its_service, its_instance, its_eventgroup, its_event,
                         lc_token = _peer_data.lc_token_](bool) {
                            // lock before asking for the token to ensure that an intermediate clean-up is honored
                            std::scoped_lock its_lock{provider_mutex_};
                            // expire commands are only send from the stub -> check validity of the "sender_"
                            if (lc_token != lc_count_) {
                                VSOMEIP_INFO << "EXPIRED SUBSCRIPTION(" << hex4(its_client) << "): [" << hex4(its_service) << "."
                                             << hex4(its_instance) << "." << hex4(its_eventgroup) << ":" << hex4(its_event)
                                             << "] router connection superseded, discarding stale continuation.";
                                return;
                            }
                            uint32_t its_remote_subscriber_count{0};
                            if (its_pending_id == PENDING_SUBSCRIPTION_ID) {
                                // Local subscriber: withdraw subscription
                                unsubscribe_base(its_client, its_service, its_instance, its_eventgroup, its_event, its_lock);
                            } else {
                                // Remote subscriber: withdraw subscription only if no more remote subscriber exists
                                its_remote_subscriber_count =
                                        get_remote_subscriber_count(its_service, its_instance, its_eventgroup, false, its_lock);
                                if (!its_remote_subscriber_count) {
                                    unsubscribe_base(VSOMEIP_ROUTING_CLIENT, its_service, its_instance, its_eventgroup, its_event,
                                                     its_lock);
                                }
                            }
                            VSOMEIP_INFO << "EXPIRED SUBSCRIPTION(" << hex4(its_client) << "): [" << hex4(its_service) << "."
                                         << hex4(its_instance) << "." << hex4(its_eventgroup) << "." << hex4(its_event) << "] "
                                         << std::boolalpha << (its_pending_id != PENDING_SUBSCRIPTION_ID) << " "
                                         << its_remote_subscriber_count;
                        });
                VSOMEIP_INFO << "EXPIRED SUBSCRIPTION(" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_instance)
                             << "." << hex4(its_eventgroup) << "." << hex4(its_event) << "] " << std::boolalpha
                             << (its_pending_id != PENDING_SUBSCRIPTION_ID);
            } else {
                VSOMEIP_ERROR_P << "Expire deserialization failed: " << utility::dump(_data, _size);
            }
            break;
        }

        case protocol::id_e::SUBSCRIBE_NACK_ID: {
            if (protocol::subscribe_answer_data its_data;
                protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

                on_subscribe_nack(its_data.subscriber_, its_data.service_, its_data.instance_, its_data.eventgroup_, its_data.event_);
                VSOMEIP_INFO << "SUBSCRIBE NACK(" << hex4(its_client) << "): [" << hex4(its_data.service_) << "."
                             << hex4(its_data.instance_) << "." << hex4(its_data.eventgroup_) << "." << hex4(its_data.event_) << "]";
            } else {
                VSOMEIP_ERROR_P << "Subscribe nack command deserialization failed: " << utility::dump(_data, _size);
            }
            break;
        }

        case protocol::id_e::SUBSCRIBE_ACK_ID: {
            if (protocol::subscribe_answer_data its_data;
                protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

                on_subscribe_ack(its_data.subscriber_, its_data.service_, its_data.instance_, its_data.eventgroup_, its_data.event_);
                VSOMEIP_INFO << "SUBSCRIBE ACK(" << hex4(its_client) << "): [" << hex4(its_data.service_) << "." << hex4(its_data.instance_)
                             << "." << hex4(its_data.eventgroup_) << "." << hex4(its_data.event_) << "]";
            } else {
                VSOMEIP_ERROR_P << "Subscribe ack command deserialization failed: " << utility::dump(_data, _size);
            }
            break;
        }

        case protocol::id_e::OFFERED_SERVICES_RESPONSE_ID: {
            if (std::vector<protocol::service_data> its_services;
                protocol::deserialize(its_services, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
                if (is_from_routing) {
                    on_offered_services_info(its_services);
                } else {
                    VSOMEIP_ERROR_P << "Client 0x" << hex4(get_client()) << " received a offered_services message from client 0x"
                                    << hex4(its_client) << " which is not the router!";
                }
            } else {
                VSOMEIP_ERROR_P << "Offered services response command deserialization failed, memory: " << utility::dump(_data, _size);
            }
            break;
        }
        case protocol::id_e::RESEND_PROVIDED_EVENTS_ID: {
            if (pending_remote_offer_id_t its_remote_offer_id;
                protocol::deserialize(its_remote_offer_id, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
                resend_provided_event_registrations();

                std::scoped_lock its_sender_lock{sender_mutex_};
                if (sender_) {
                    sender_->send(protocol::create_resend_provided_events_cmd(get_client(), its_remote_offer_id));
                    VSOMEIP_INFO << "RESEND_PROVIDED_EVENTS(" << hex4(its_client) << ")";
                } else {
                    VSOMEIP_WARNING_P << "Failed due to a missing sender";
                }
            } else {
                VSOMEIP_ERROR_P << "Resend provided events command deserialization failed, memory: " << utility::dump(_data, _size);
            }
            break;
        }
        case protocol::id_e::SUSPEND_ID: {
            on_suspend(); // cleanup remote subscribers
            break;
        }
#ifndef VSOMEIP_DISABLE_SECURITY
        case protocol::id_e::UPDATE_SECURITY_POLICY_INT_ID:
            is_internal_policy_update = true;
            [[fallthrough]];
        case protocol::id_e::UPDATE_SECURITY_POLICY_ID: {
            if (is_from_routing) {
                if (protocol::update_security_policy_data its_data;
                    protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
                    auto its_policy = std::make_shared<policy>();
                    const byte_t* ptr = its_data.policy_.data();
                    uint32_t size = static_cast<uint32_t>(its_data.policy_.size());
                    if (its_data.policy_.size() == 0 || !its_policy->deserialize(ptr, size)) {
                        VSOMEIP_ERROR << "vSomeIP Security: Policy deserialization failed: " << utility::dump(_data, _size);
                        break;
                    }

                    uid_t its_uid;
                    gid_t its_gid;
                    if (its_policy->get_uid_gid(its_uid, its_gid)) {
                        if (is_internal_policy_update
                            || configuration_->get_policy_manager()->is_policy_update_allowed(its_uid, its_policy)) {
                            configuration_->get_policy_manager()->update_security_policy(its_uid, its_gid, its_policy);
                            std::scoped_lock its_sender_lock{sender_mutex_};
                            if (sender_) {
                                sender_->send(protocol::create_update_security_policy_response_cmd(get_client(), its_data.update_id_));
                            } else {
                                VSOMEIP_ERROR_P << "Failed due to a missing sender";
                            }
                        }
                    } else {
                        VSOMEIP_ERROR << "vSomeIP Security: Policy has no valid uid/gid!";
                    }
                } else {
                    VSOMEIP_ERROR << "vSomeIP Security: Policy deserialization failed: " << utility::dump(_data, _size);
                }
            } else {
                VSOMEIP_ERROR_P << "Client 0x" << hex4(get_client()) << " received a policy update from client 0x" << hex4(its_client)
                                << " which is not the router!";
            }
            break;
        }

        case protocol::id_e::REMOVE_SECURITY_POLICY_ID: {
            if (is_from_routing) {
                if (protocol::remove_security_policy_data its_data;
                    protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

                    uid_t its_uid(its_data.uid_);
                    gid_t its_gid(its_data.gid_);

                    if (configuration_->get_policy_manager()->is_policy_removal_allowed(its_uid)) {
                        configuration_->get_policy_manager()->remove_security_policy(its_uid, its_gid);
                        std::scoped_lock its_sender_lock{sender_mutex_};
                        if (sender_) {
                            sender_->send(protocol::create_remove_security_policy_response_cmd(get_client(), its_data.update_id_));
                        } else {
                            VSOMEIP_ERROR_P << "Failed due to a missing sender";
                        }
                    }
                } else
                    VSOMEIP_ERROR_P << "Remove security policy command deserialization failed, memory: " << utility::dump(_data, _size);
            } else {
                VSOMEIP_ERROR_P << "Client 0x" << hex4(get_client()) << " received a remove_security_policy message from client 0x"
                                << hex4(its_client) << " which is not the router!";
            }
            break;
        }

        case protocol::id_e::DISTRIBUTE_SECURITY_POLICIES_ID: {
            if (is_from_routing) {
                if (std::vector<std::shared_ptr<policy>> its_data;
                    protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
                    for (auto p : its_data) {
                        uid_t its_uid;
                        gid_t its_gid;
                        p->get_uid_gid(its_uid, its_gid);
                        if (configuration_->get_policy_manager()->is_policy_update_allowed(its_uid, p)) {
                            configuration_->get_policy_manager()->update_security_policy(its_uid, its_gid, p);
                        }
                    }
                } else {
                    VSOMEIP_ERROR_P << "Distribute security policies command deserialization failed: " << utility::dump(_data, _size);
                }
            } else {
                VSOMEIP_ERROR_P << "Client 0x" << hex4(get_client()) << " received a distribute_security_policies message from client 0x"
                                << hex4(its_client) << " which is not the router!";
            }
            break;
        }

        case protocol::id_e::UPDATE_SECURITY_CREDENTIALS_ID: {
            if (is_from_routing) {
                if (std::vector<std::pair<uid_t, gid_t>> its_data;
                    protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
                    on_update_security_credentials(its_data);
                } else {
                    VSOMEIP_ERROR_P << "Update security credentials command deserialization failed: " << utility::dump(_data, _size);
                }
            } else {
                VSOMEIP_ERROR_P << "Client 0x" << hex4(get_client()) << " received an update_security_credentials message from client 0x"
                                << hex4(its_client) << " which is not the router!";
            }
            break;
        }
#endif // !VSOMEIP_DISABLE_SECURITY
        case protocol::id_e::CONFIG_ID: {
            std::vector<std::pair<std::string, std::string>> its_configs;
            protocol::deserialize(its_configs, _data + parsed_hdr_bytes, its_header.length_);
            for (auto const& [key, value] : its_configs) {
                if (key == "hostname") {
                    lazy_load(value);
                    break;
                }
            }
            break;
        }
        default:
            break;
        }
    } else {
        VSOMEIP_ERROR_P << "Deserialization of command header failed, memory: " << utility::dump(_data, _size);
    }
}

void routing_manager_client::on_routing_info(const byte_t* _data, uint32_t _size) {
    std::vector<protocol::routing_info_entry_data> its_entries;
    if (protocol::deserialize(its_entries, _data, _size) == 0) {
        VSOMEIP_ERROR_P << "Deserializing routing info command entries failed, memory: " << utility::dump(_data, _size);
        return;
    }

    for (const auto& e : its_entries) {
        auto its_client = e.client_;
        switch (e.type_) {
        case protocol::routing_info_entry_type_e::RIE_ADD_SERVICE_INSTANCE: {
            boost::asio::ip::address its_address = e.address_;
            port_t its_port = e.port_;
            if (!its_address.is_unspecified()) {
                // remove client (and endpoints!) at same address/port
                // as address/port are unique and that definitely means the client no longer exists
                if (client_t old_client = get_client_by_address(its_address, its_port);
                    old_client != VSOMEIP_CLIENT_UNSET && old_client != its_client) {
                    VSOMEIP_INFO_P << "Old client 0x" << hex4(old_client) << " removed due to new client 0x" << hex4(its_client) << " @ "
                                   << its_address.to_string() + ":" << its_port;

                    // Drop old_client's stale consumer entry (this additionally ensures its offered services are re-requested).
                    cleanup_client(old_client, true, connection_role_e::consumer);

                    // Drop old_client's stale provider entry only if that endpoint is still bound to old_client.
                    if (auto its_provider_ep = ep_mgr_->find_local_server_endpoint_by_peer(its_address, static_cast<port_t>(its_port + 1));
                        its_provider_ep && its_provider_ep->connected_client() == old_client) {
                        VSOMEIP_INFO_P << "Dropping stale provider endpoint of old client 0x" << hex4(old_client) << " @ "
                                       << its_address.to_string() << ":" << static_cast<port_t>(its_port + 1);
                        its_provider_ep->trigger_error();
                    }
                }
            }

            std::vector<subscription_data_t> collected_subscriptions;
            {
                std::scoped_lock its_lock(consumer_mutex_);
                auto& data = consumer_[its_client];
                data.address_ = its_address;
                data.port_ = its_port;
                for (const auto& s : e.services_) {
                    const auto its_service(s.service_);
                    const auto its_instance(s.instance_);
                    const auto its_major(s.major_version_);
                    const auto its_minor(s.minor_version_);
                    const bool newly_available = available_services_.add(its_service, its_instance, its_major, its_minor, its_client);
                    if (newly_available) {
                        host_->on_availability(its_service, its_instance, availability_state_e::AS_AVAILABLE, its_major, its_minor);
                        VSOMEIP_INFO << "ON_AVAILABLE(" << hex4(get_client()) << "): [" << hex4(its_service) << "." << hex4(its_instance)
                                     << ":" << int(its_major) << "." << its_minor << "]";
                        collect_pending_subscriptions(its_service, its_instance, its_major, collected_subscriptions, its_lock);
                    } else {
                        VSOMEIP_WARNING << "Received routing_info in client 0x" << hex4(get_client())
                                        << " for an already available service [" << hex4(its_service) << "." << hex4(its_instance) << ":"
                                        << int(its_major) << "." << its_minor << "] ~> ignoring duplicate.";
                    }
                }
            }
            for (auto const& sub : collected_subscriptions) {
                const auto& [its_service, its_instance] = sub.service_instance_;
                send_subscribe(get_client(), its_service, its_instance, sub.eventgroup_, sub.major_, sub.event_, sub.filter_);
            }
            break;
        }

        case protocol::routing_info_entry_type_e::RIE_DELETE_SERVICE_INSTANCE: {
            std::scoped_lock its_lock(consumer_mutex_);
            for (const auto& s : e.services_) {
                const bool was_available = available_services_.remove(s.service_, s.instance_, s.major_version_);
                on_stop_offer_service(s.service_, s.instance_, s.major_version_, s.minor_version_, was_available, its_lock);
            }
            break;
        }

        default:
            VSOMEIP_ERROR_P << "Unknown routing info entry type (" << static_cast<int>(e.type_) << ")";
            break;
        }
    }
}

void routing_manager_client::on_offered_services_info(std::vector<protocol::service_data> const& _services) {

    std::vector<std::pair<service_t, instance_t>> its_offered_services_info;
    its_offered_services_info.reserve(_services.size());

    for (const auto& s : _services)
        its_offered_services_info.push_back(std::make_pair(s.service_, s.instance_));

    host_->on_offered_services_info(its_offered_services_info);
}

void routing_manager_client::reconnect() {
    {
        // ensure that no further connections will be added to the list of endpoints
        std::scoped_lock lock(receiver_mutex_);
        if (routing_mode_ != routing_mode_e::UDS_ONLY) {
            // tcp needs to claim a port to ensure that the sender is not
            // blocking a wrong port
            if (tcp_receiver_) {
                // stop accepting connections + stop existing connections,
                // but don't close the socket to not free the claimed port.
                tcp_receiver_->halt();
            } else {
                // TODO this might end up looping forever, if the network is assumed
                // to be down. How to break out of this loop in case of "stop" ?
                tcp_receiver_ = ep_mgr_->create_local_server(transport_protocol_e::TCP);
            }
        }

        if (uds_receiver_) {
            uds_receiver_->stop();
            uds_receiver_ = nullptr;
        }
    }
    // Now the set of endpoints should be fixed:
    // * no new server endpoint because local_server is stopped
    // * no new client endpoint because the statemachine is in the DEREGISTERED state
    //   (-> send or sub will not lead to a find_or_create
    //     + because the sender is down no new routing_info for subscriptions)
    ep_mgr_->stop_all_endpoints();

    // Clean-up Phase
    //
    configuration_->get_policy_manager()->cleanup_client_to_sec_client_mappings();
    {
        std::scoped_lock its_lock(provider_mutex_);
        clear_remote_subscriptions(its_lock);
        cleanup_subscriber(its_lock);
        // by clearing the provider endpoints under the provider lock it is guaranteed
        // that any in-flight subscription continuation will be invalidated
        ep_mgr_->clear_provider_endpoints();
        ++lc_count_;
    }
    // it is not guaranteed that all routing data was used for creating an endpoint
    // therefore it is better to remove the whole set, without a dependency to
    // a client id.
    cleanup_consumer();

    // Restart Phase
    //
    VSOMEIP_INFO_P << "Application/Client 0x" << hex4(get_client()) << ": Reconnecting to routing manager.";
    // inform host about its own registration state changes
    host_->on_state(state_type_e::ST_DEREGISTERED);
    state_machine_->deregistered();
}

void routing_manager_client::register_application(client_t _client, std::unique_lock<std::mutex>& receiver_lock_) {
    auto its_configuration = get_configuration();
    auto const its_routing_host_address = its_configuration->get_routing_host_address();
    // UDS is used only when local routing is configured, or when uds-preferred is on and the routing manager has the same IP.
    // Otherwise TCP is used.
    bool const via_uds = (routing_mode_ == routing_mode_e::UDS_ONLY)
            || (routing_mode_ == routing_mode_e::UDS_AND_TCP && its_routing_host_address == its_configuration->get_routing_guest_address());
    if (via_uds) {
        VSOMEIP_INFO_P << "Client 0x" << hex4(get_client()) << " Registering to routing manager @ " << its_configuration->get_network()
                       << "-0";
    } else {
        VSOMEIP_INFO_P << "Client 0x" << hex4(get_client()) << " Registering to routing manager @ " << its_routing_host_address.to_string()
                       << ":" << its_configuration->get_routing_host_port();
    }

#if defined(__linux__) || defined(__QNX__)
    auto const sec_client = get_sec_client();
    if (!configuration_->get_policy_manager()->check_credentials(get_client(), &sec_client)) {
        VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(get_client())
                      << "isn't allowed to use the client endpoint due to credential check failed!";
        state_machine_->deregistered();
        return;
    }
#endif
    // when changing the state we need to ensure that the debounce timer is not dispatching + altering the request set
    if (std::scoped_lock its_lock{consumer_mutex_, provider_mutex_}; state_machine_->registered(_client)) {
        VSOMEIP_INFO << "Application/Client " << hex4(get_client()) << " (" << host_->get_name() << ") is registered.";

        if (!send_pending_commands(its_lock)) {
            VSOMEIP_WARNING_P << ": Application/Client 0x" << hex4(get_client()) << " (" << host_->get_name()
                              << ") could not send pending offers";
        }
        host_->on_state(state_type_e::ST_REGISTERED);
        return;
    }
    // This code path will only be reached if there was an error in the registration
    VSOMEIP_ERROR << "Application/Client " << hex4(get_client()) << " (" << host_->get_name() << ") failed to register, will reconnect.";
    receiver_lock_.unlock();
    reconnect();
}

void routing_manager_client::send_pong() const {

    if (auto state = state_machine_->state();
        is_value(state).any_of(routing_client_state_e::ST_REGISTERED, routing_client_state_e::ST_REGISTERING)) {
        std::scoped_lock its_sender_lock{sender_mutex_};
        if (sender_) {
            sender_->send(protocol::create_pong_cmd(get_client()));
        } else {
            VSOMEIP_ERROR_P << "Failed due to a missing sender";
        }
    } else {
        VSOMEIP_WARNING_P << "Pong command for Client 0x" << hex4(get_client()) << get_client()
                          << " not dispatched due to unexpected state: " << state;
    }
}

bool routing_manager_client::send_request_services(std::span<protocol::service_data const> _requests) {
    if (!_requests.size()) {
        return true;
    }

    std::scoped_lock its_sender_lock{sender_mutex_};
    if (sender_ && sender_->send(protocol::create_request_service_cmd(get_client(), _requests))) {
        return true;
    }
    VSOMEIP_ERROR_P << "Failed to send requested services";

    return false;
}

bool routing_manager_client::send_event_registrations(client_t _client, std::span<protocol::register_event_data const> _registrations) {
    // Nothing to register: avoid emitting an empty REGISTER_EVENT command
    if (_registrations.empty()) {
        return true;
    }

    std::scoped_lock its_sender_lock{sender_mutex_};
    if (sender_ && sender_->send(protocol::create_register_events_cmd(_client, _registrations))) {
        return true;
    }

    VSOMEIP_ERROR_P << "Failed to send event registrations to host";
    return false;
}

void routing_manager_client::update_subscription_state_and_notify(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                                  event_t _event, uint16_t _error) {
    bool entry_found = false;
    {
        std::scoped_lock its_lock{consumer_mutex_};
        const subscription_state_e new_state =
                _error ? subscription_state_e::SUBSCRIPTION_NOT_ACKNOWLEDGED : subscription_state_e::SUBSCRIPTION_ACKNOWLEDGED;
        auto update_event_entry = [&](const service_instance_t& si, event_t lookup_ev) {
            auto its_si = consumed_events_.find(si);
            if (its_si == consumed_events_.end()) {
                return false;
            }
            auto its_ev = its_si->second.find(lookup_ev);
            if (its_ev == its_si->second.end()) {
                return false;
            }
            auto& its_subscriptions = its_ev->second.subscriptions_;
            auto its_eg = its_subscriptions.find(_eventgroup);
            if (its_eg == its_subscriptions.end()) {
                its_eg = its_subscriptions.find(ANY_EVENTGROUP);
            }
            if (its_eg == its_subscriptions.end()) {
                return false;
            }
            its_eg->second.state_ = new_state;
            return true;
        };
        for (const service_instance_t si : {service_instance_t{_service, _instance}, service_instance_t{ANY_SERVICE, _instance}}) {
            if (update_event_entry(si, _event)) {
                entry_found = true;
            }
            if (update_event_entry(si, ANY_EVENT)) {
                entry_found = true;
            }
            if (entry_found) {
                break;
            }
        }
    }
    if (entry_found) {
        host_->on_subscription_status(_service, _instance, _eventgroup, _event, _error);
    }
}

void routing_manager_client::on_subscribe_ack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                              event_t _event) {
    (void)_client;

    if (_event == ANY_EVENT) {
        auto its_eventgroup = find_consumer_eventgroup(_service, _instance, _eventgroup);
        if (its_eventgroup) {
            for (const auto& its_event : its_eventgroup->get_events()) {
                update_subscription_state_and_notify(_service, _instance, _eventgroup, its_event->get_event(), 0x0 /*OK*/);
            }
        }
    } else {
        update_subscription_state_and_notify(_service, _instance, _eventgroup, _event, 0x0 /*OK*/);
    }
}

void routing_manager_client::on_subscribe_nack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                               event_t _event) {
    (void)_client;

    if (_event == ANY_EVENT) {
        auto its_eventgroup = find_consumer_eventgroup(_service, _instance, _eventgroup);
        if (its_eventgroup) {
            for (const auto& its_event : its_eventgroup->get_events()) {
                update_subscription_state_and_notify(_service, _instance, _eventgroup, its_event->get_event(), 0x7 /*Rejected*/);
            }
        }
    } else {
        update_subscription_state_and_notify(_service, _instance, _eventgroup, _event, 0x7 /*Rejected*/);
    }
}

bool routing_manager_client::cache_event_payload(const std::shared_ptr<message>& _message) {
    const service_t its_service(_message->get_service());
    const instance_t its_instance(_message->get_instance());
    const method_t its_method(_message->get_method());

    std::scoped_lock its_lock(consumer_mutex_);
    auto its_event = find_consumed_event(its_service, its_instance, its_method, its_lock);
    if (!its_event) {
        // we received an event which was not yet requested.
        // create a placeholder until someone requests this event with
        // full information like eventgroup, field or not etc.
        std::set<eventgroup_t> its_eventgroups;
        register_consumer_event(host_->get_client(), its_service, its_instance, its_method, its_eventgroups, event_type_e::ET_UNKNOWN,
                                reliability_type_e::RT_UNKNOWN, std::chrono::milliseconds::zero(), false, true, nullptr, true, its_lock);
        its_event = find_consumed_event(its_service, its_instance, its_method, its_lock);
    } else {
        if (its_event->is_field() || its_event->get_type() == event_type_e::ET_UNKNOWN) {
            its_event->prepare_update_payload(_message->get_payload(), true);
            its_event->update_payload();
        }
    }

    // Only fields (and unresolved placeholders) cache a replayable value, so only those may set the initial-notification flag.
    const bool its_event_caches_value = its_event && (its_event->is_field() || its_event->get_type() == event_type_e::ET_UNKNOWN);

    bool has_active_subscription = false;
    auto found_si = consumed_events_.find({its_service, its_instance});
    if (found_si != consumed_events_.end()) {
        auto found_ev = found_si->second.find(its_method);
        if (found_ev != found_si->second.end() && !found_ev->second.subscriptions_.empty()) {
            has_active_subscription = true;
            if (its_event_caches_value) {
                for (auto& [eg_id, sub] : found_ev->second.subscriptions_) {
                    sub.initial_notification_received_ = true;
                }
            }
        }
        if (!has_active_subscription) {
            auto found_any_ev = found_si->second.find(ANY_EVENT);
            if (found_any_ev != found_si->second.end() && !found_any_ev->second.subscriptions_.empty()) {
                if (its_event) {
                    for (const auto eg : its_event->get_eventgroups()) {
                        if (auto found_eg = found_any_ev->second.subscriptions_.find(eg);
                            found_eg != found_any_ev->second.subscriptions_.end()) {
                            has_active_subscription = true;
                            if (its_event_caches_value) {
                                found_eg->second.initial_notification_received_ = true;
                            }
                        }
                    }
                }
                if (!has_active_subscription && (!its_event || its_event->get_eventgroups().empty())) {
                    has_active_subscription = true;
                    if (its_event_caches_value) {
                        for (auto& [fallback_eg, sub] : found_any_ev->second.subscriptions_) {
                            sub.initial_notification_received_ = true;
                        }
                    }
                }
            }
        }
    }
    return has_active_subscription;
}

void routing_manager_client::on_stop_offer_service(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor,
                                                   bool _was_available,
                                                   [[maybe_unused]] std::scoped_lock<std::mutex> const& _consumer_lock) {
    if (auto its_si = consumed_events_.find(service_instance_t{_service, _instance}); its_si != consumed_events_.end()) {
        for (auto& [event_id, entry] : its_si->second) {
            if (_was_available) {
                // Reset subscription state and initial-received flags for all consumed events of this service.
                for (auto& [its_eg_id, its_subscription] : entry.subscriptions_) {
                    its_subscription.initial_notification_received_ = false;
                    its_subscription.state_ = subscription_state_e::SUBSCRIPTION_NOT_ACKNOWLEDGED;
                }
            }
            if (entry.event_) {
                if (entry.event_->is_set()) {
                    VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(event_id) << "]";
                }
                entry.event_->unset_payload();
            }
        }
    }

    // Notify the application after the internal state has been cleaned up (consistent with the other flows).
    if (_was_available) {
        host_->on_availability(_service, _instance, availability_state_e::AS_UNAVAILABLE, _major, _minor);
    }
    VSOMEIP_INFO << "ON_UNAVAILABLE(" << hex4(get_client()) << "): [" << hex4(_service) << "." << hex4(_instance) << ":"
                 << static_cast<int>(_major) << "." << _minor << "] forwarded=" << std::boolalpha << _was_available;
}

bool routing_manager_client::send_pending_commands(
        [[maybe_unused]] std::scoped_lock<std::mutex, std::mutex> const& _consumer_provider_lock) {
    // The consumer and provider mutexes are held by the caller (via _consumer_provider_lock), so both pending
    // registration sets (and the requests) can be referenced via non-owning spans while the batch is serialized.
    // Order matters: provided events, offer services, request services, consumed events.
    command_batch batch;
    if (!pending_provided_event_registrations_.empty()) {
        batch.add(protocol::create_register_events_cmd(get_client(), pending_provided_event_registrations_));
    }
    for (auto const& po : offered_services_.view()) {
        batch.add(protocol::create_offer_service_cmd(get_client(), po.service_, po.instance_, po.major_version_, po.minor_version_));
    }
    if (auto const its_requests = requests_.view(); !its_requests.empty()) {
        batch.add(protocol::create_request_service_cmd(get_client(), its_requests));
    }
    if (!pending_consumed_event_registrations_.empty()) {
        batch.add(protocol::create_register_events_cmd(get_client(), pending_consumed_event_registrations_));
    }
    if (batch.empty()) {
        return true;
    }

    std::scoped_lock its_sender_lock{sender_mutex_};
    if (!sender_) {
        VSOMEIP_ERROR_P << "Failed to send pending commands due to a missing sender";
        return false;
    }
    return sender_->send(batch);
}

void routing_manager_client::init_receiver_side([[maybe_unused]] std::unique_lock<std::mutex> const& _receive_lock) {
    auto create_receiver = [&](auto& _receiver, transport_protocol_e _protocol) {
        if (_receiver) {
            std::uint16_t its_port = _receiver->get_local_port();
            if (its_port != ILLEGAL_PORT && _protocol == transport_protocol_e::TCP)
                VSOMEIP_INFO << "Reusing local server endpoint @" << its_port << " endpoint: " << _receiver;
            return;
        }
        _receiver = ep_mgr_->create_local_server(_protocol);
    };

    switch (routing_mode_) {
    case routing_mode_e::UDS_AND_TCP:
        create_receiver(tcp_receiver_, transport_protocol_e::TCP);
        create_receiver(uds_receiver_, transport_protocol_e::UDS);
        break;
    case routing_mode_e::UDS_ONLY:
        create_receiver(uds_receiver_, transport_protocol_e::UDS);
        break;
    default: // TCP_ONLY
        create_receiver(tcp_receiver_, transport_protocol_e::TCP);
        break;
    }
}

void routing_manager_client::notify_remote_initially(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                     std::scoped_lock<std::mutex> const& _lock) {
    auto const* service = offered_services_.find({_service, _instance, ANY_MAJOR, ANY_MINOR});
    if (!service) {
        VSOMEIP_ERROR_P << "Failed due to a missing service info: [" << hex4(_service) << "." << hex4(_instance) << ":" << hex4(_eventgroup)
                        << "]";
        return;
    }
    for (auto const& event : find_provided_events_by_group(_service, _instance, _eventgroup, _lock)) {
        if (event->is_field() && event->is_set()) {
            std::shared_ptr<message> its_notification = runtime::get()->create_notification();
            its_notification->set_service(_service);
            its_notification->set_instance(_instance);
            its_notification->set_method(event->get_event());
            its_notification->set_payload(event->get_payload());
            its_notification->set_interface_version(service->major_version_);
            std::scoped_lock its_sender_lock{sender_mutex_};
            if (sender_) {
                sender_->send(protocol::create_send_cmd(protocol::id_e::NOTIFY_ID, get_client(), its_notification, VSOMEIP_ROUTING_CLIENT));
            } else {
                VSOMEIP_ERROR_P << "Failed due to a missing sender";
                return;
            }
        }
    }
}

uint32_t routing_manager_client::get_remote_subscriber_count(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                             bool _increment, [[maybe_unused]] std::scoped_lock<std::mutex> const& _lock) {
    uint32_t count(0);
    bool found(false);

    if (auto found_si = remote_subscriber_count_.find({_service, _instance}); found_si != remote_subscriber_count_.end()) {
        if (auto found_group = found_si->second.find(_eventgroup); found_group != found_si->second.end()) {
            found = true;
            if (_increment) {
                found_group->second = found_group->second + 1;
            } else {
                if (found_group->second > 0) {
                    found_group->second = found_group->second - 1;
                }
            }
            count = found_group->second;
        }
    }
    if (!found) {
        if (_increment) {
            remote_subscriber_count_[{_service, _instance}][_eventgroup] = 1;
            count = 1;
        }
    }
    return count;
}

void routing_manager_client::clear_remote_subscriber_count(service_t _service, instance_t _instance,
                                                           [[maybe_unused]] std::scoped_lock<std::mutex> const& _lock) {
    remote_subscriber_count_.erase({_service, _instance});
}

bool routing_manager_client::create_placeholder_event_and_subscribe(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                                    event_t _notifier,
                                                                    const std::shared_ptr<debounce_filter_impl_t>& _filter,
                                                                    client_t _client, std::scoped_lock<std::mutex> const& _lock) {

    bool is_inserted(false);

    if (offered_services_.find({_service, _instance, ANY_MAJOR, ANY_MINOR})) {
        // We received an event for an existing service which was not yet
        // requested/offered. Create a placeholder field until someone
        // requests/offers this event with full information like eventgroup,
        // field/event, etc.
        std::set<eventgroup_t> its_eventgroups({_eventgroup});
        // routing_manager_client: Always register with own client id and shadow = false
        register_provider_event(host_->get_client(), _service, _instance, _notifier, its_eventgroups, event_type_e::ET_UNKNOWN,
                                reliability_type_e::RT_UNKNOWN, std::chrono::milliseconds::zero(), false, true, nullptr, true, _lock);
        std::shared_ptr<event> its_event = find_provided_event(_service, _instance, _notifier, _lock);
        if (its_event) {
            is_inserted = its_event->add_subscriber(_eventgroup, _filter, _client, false);
        }
    }

    return is_inserted;
}

void routing_manager_client::request_debounce_timeout_cbk(boost::system::error_code const& _error) {
    std::scoped_lock its_lock{consumer_mutex_};
    if (!_error) {
        if (requests_to_debounce_.size()) {
            if (auto state = state_machine_->state(); state == routing_client_state_e::ST_REGISTERED) {
                send_request_services(requests_to_debounce_.view());
                requests_.take(requests_to_debounce_);
            } else {
                request_debounce_timer_.expires_after(
                        std::chrono::milliseconds(configuration_->get_request_debounce_time(host_->get_name())));
                request_debounce_timer_.async_wait(std::bind(&routing_manager_client::request_debounce_timeout_cbk,
                                                             std::dynamic_pointer_cast<routing_manager_client>(shared_from_this()),
                                                             std::placeholders::_1));
                return;
            }
        }
    }
    request_debounce_timer_running_ = false;
}

void routing_manager_client::register_client_error_handler(client_t _client, const std::shared_ptr<local_endpoint>& _endpoint,
                                                           connection_role_e _role) {

    _endpoint->register_cleanup_handler([weak_self = weak_from_this(), _client, _role](bool _due_to_error) {
        if (auto self = weak_self.lock(); self) {
            self->cleanup_client(_client, _due_to_error, _role);
        }
    });
}

// local_endpoint_manager_host
client_t routing_manager_client::get_client_id() {
    return get_client();
}

void routing_manager_client::set_port(port_t _port) {
    set_sec_client_port(_port);
}

void routing_manager_client::register_error_handler(client_t _client, std::shared_ptr<local_endpoint> _ep, connection_role_e _role) {
    register_client_error_handler(_client, _ep, _role);
}

void routing_manager_client::cleanup_client(client_t _client, bool _due_to_error, connection_role_e _role) {

    if (_client != VSOMEIP_ROUTING_CLIENT) {
        VSOMEIP_INFO_P << "self 0x" << hex4(get_client()) << " handles cleanup of client 0x" << hex4(_client) << " ("
                       << (_role == connection_role_e::provider ? "provider" : "consumer") << "), not reconnecting";

        // The two roles ride distinct local sockets, so tear down only the
        // failing role's state; the other role stays intact.
        if (_role == connection_role_e::provider) {
            remove_local_provider(_client, _due_to_error);
        } else {
            // First ensure that the consumer connection is dropped, before
            // enforcing a reconnect from the client. Otherwise a client
            // subscribe might be handled by a partially cleaned-up connection.
            local_service_table requested_services;
            remove_local_consumer(_client, _due_to_error, requested_services);

            // Request the host these services again. Re-requesting peer-offered
            // services is a consumer-only concern;
            if (_due_to_error) {
                if (auto state = state_machine_->state(); state == routing_client_state_e::ST_REGISTERED) {
                    send_request_services(requested_services.view());
                }
            }
        }
    } else {
        {
            std::scoped_lock its_lock{sender_mutex_};
            if (sender_) {
                sender_->stop(_due_to_error);
                sender_ = nullptr;
            }
            if (on_sender_stopped_) {
                on_sender_stopped_.fire();
                on_sender_stopped_ = {};
            }
            if (!_due_to_error) {
                VSOMEIP_INFO_P << "self 0x" << hex4(get_client()) << " handles shutdown. Not reconnecting to host 0x" << hex4(_client);
                return;
            }
        }
        VSOMEIP_INFO_P << "self 0x" << hex4(get_client()) << " handles cleanup of host 0x" << hex4(_client) << ", will reconnect";
        reconnect();
    }
}

void routing_manager_client::send_get_offered_services_info(client_t _client, offer_type_e _offer_type) {
    std::scoped_lock its_sender_lock{sender_mutex_};
    if (sender_) {
        sender_->send(protocol::create_offered_services_request_cmd(_client, _offer_type));
    } else {
        VSOMEIP_ERROR_P << "Failed due to a missing sender";
    }
}

void routing_manager_client::resend_provided_event_registrations() {
    std::scoped_lock its_lock(provider_mutex_);
    for (protocol::register_event_data const& reg : pending_provided_event_registrations_) {
        // The provider lock is held, so the stored entry can be sent directly via a one-element subspan.
        send_event_registrations(get_client(), std::span{&reg, 1});
        VSOMEIP_INFO << "REGISTER EVENT(" << hex4(get_client()) << "): [" << hex4(reg.service_) << "." << hex4(reg.instance_) << "."
                     << hex4(reg.event_) << ":is_provider=" << std::boolalpha << reg.is_provided_ << "]";
    }
}

#ifndef VSOMEIP_DISABLE_SECURITY

void routing_manager_client::on_update_security_credentials(std::vector<std::pair<uid_t, gid_t>> const& _credentials) {
    for (const auto& c : _credentials) {
        std::shared_ptr<policy> its_policy(std::make_shared<policy>());
        boost::icl::interval_set<gid_t> its_gid_set;
        uid_t its_uid(c.first);
        gid_t its_gid(c.second);

        its_gid_set.insert(its_gid);

        its_policy->credentials_ += std::make_pair(boost::icl::interval<uid_t>::closed(its_uid, its_uid), its_gid_set);
        its_policy->allow_who_ = true;
        its_policy->allow_what_ = true;

        configuration_->get_policy_manager()->add_security_credentials(its_uid, its_gid, its_policy, get_client());
    }
}
#endif

void routing_manager_client::on_client_assign_ack(const client_t& _client, bool _is_tcp) {

    if (_client == VSOMEIP_CLIENT_UNSET) {
        VSOMEIP_ERROR_P << "(" << host_->get_name() << ":" << hex4(_client) << ") Invalid clientID";
        return;
    }

    // order matters:
    // 0. call host (while unlocked to avoid lock inversion)
    host_->set_client(_client);

#ifdef __linux__
    auto const sec_client = get_sec_client();
    configuration_->get_policy_manager()->store_client_to_sec_client_mapping(_client, &sec_client);
    configuration_->get_policy_manager()->store_sec_client_to_client_mapping(&sec_client, _client);
    // TODO why is there no logic to remove this mapping
    // when there was some problem with the registration?
#endif

    // order matters:
    // 1. lock the receiver mutex,
    // 2. try to transition the state machine
    // this ensures that th receiver init does counter act the potentially
    // interleaving stopping of the receiver within the ::stop method.
    bool is_started{false};
    std::unique_lock its_lock{receiver_mutex_};

    init_receiver_side(its_lock);
    {
        auto start_receiver = [&](auto& _receiver) {
            if (_receiver) {
                _receiver->set_id(_client);
                _receiver->start();
                is_started = true;
            }
        };

        switch (routing_mode_) {
        case routing_mode_e::UDS_ONLY:
            start_receiver(uds_receiver_);
            break;
        case routing_mode_e::UDS_AND_TCP:
            start_receiver(uds_receiver_);
            start_receiver(tcp_receiver_);
            break;
        default: // TCP_ONLY
            start_receiver(tcp_receiver_);
            break;
        }

        if (is_started) {
            VSOMEIP_INFO_P << "Client 0x" << hex4(get_client()) << " (" << host_->get_name() << ") successfully connected to routing via "
                           << (_is_tcp ? "TCP" : "UDS") << " ~> registering...";
            register_application(_client, its_lock);
        }
    }

    if (!is_started) {
        VSOMEIP_WARNING_P << ": (" << host_->get_name() << ":" << hex4(_client) << ") Receiver not started. Restarting";
        state_machine_->deregistered();
        its_lock.unlock();
        host_->set_client(VSOMEIP_CLIENT_UNSET);
    }
}

void routing_manager_client::on_suspend() {

    VSOMEIP_INFO_P << "Application 0x" << hex4(host_->get_client());
    std::scoped_lock its_lock(provider_mutex_);
    clear_remote_subscriptions(its_lock);
}

void routing_manager_client::clear_remote_subscriptions(std::scoped_lock<std::mutex> const& _provider_lock) {

    // Unsubscribe everything that is left over.
    for (const auto& [si, eventgroups] : remote_subscriber_count_) {
        for (const auto& [eg, _] : eventgroups) {
            unsubscribe_base(VSOMEIP_ROUTING_CLIENT, si.service, si.instance, eg, ANY_EVENT, _provider_lock);
        }
    }

    // Remove all entries.
    remote_subscriber_count_.clear();
}

void routing_manager_client::restart_sender([[maybe_unused]] std::unique_lock<std::mutex> const& _sender_mutex) {
    if (sender_) {
        sender_->stop(true);
        sender_ = nullptr;
    }
    if (sender_debounce_active_) {
        start_sender_after_debounce_ = true;
        VSOMEIP_INFO_P << "The restart of the sender is debounced";
        return;
    }
    start_sender_after_debounce_ = false;
    if (!state_machine_->start_registration()) {
        VSOMEIP_WARNING_P << "(" << hex4(get_client()) << ") Non-Deregistered State Set (" << state_machine_->state() << "). Returning";
        return;
    }
    sender_ = ep_mgr_->create_routing_client();
    if (sender_) {
        // The sender takes the VSOMEIP_ROUTING_CLIENT reconnect path in cleanup_client, so the role is unused here.
        register_error_handler(VSOMEIP_ROUTING_CLIENT, sender_, connection_role_e::consumer);
        // save to read even without acquiring the provider_mutex_, as a new
        // token is only generated during a reconnect or stop, start sequence,
        // which are serialized with the start of the sender.
        sender_->start(lc_count_.load());
        sender_debounce_active_ = true;
        sender_debounce_->start();
    } else {
        VSOMEIP_ERROR_P << "Failed due to a missing sender";
    }
}

void routing_manager_client::debounce_restart_sender_done() {
    std::unique_lock its_sender_lock(sender_mutex_);
    sender_debounce_active_ = false;
    if (start_sender_after_debounce_) {
        restart_sender(its_sender_lock);
    }
}

void routing_manager_client::lazy_load(const std::string& _client_host) {
#if !defined(VSOMEIP_DISABLE_SECURITY) && (defined(__linux__))
    std::scoped_lock lock{lazy_load_mtx_};
    if (configuration_->is_security_enabled() && !configuration_->is_security_external()) {
        configuration_->lazy_load_security(_client_host);
        configuration_->lazy_load_security(get_client_host()); // necessary for lazy loading from inside android container
    }
#endif
    // The routing client has no need to store this data.
    // This data is better kept at the endpoint
}

void routing_manager_client::remove_local_provider(client_t _client, bool _due_to_error) {

    vsomeip_sec_client_t its_sec_client;
    configuration_->get_policy_manager()->get_client_to_sec_client_mapping(_client, its_sec_client);
    auto ep = ep_mgr_->find_local_server_endpoint(_client);
    std::string const env = ep ? ep->get_env() : "";

    {
        std::scoped_lock its_lock(provider_mutex_);
        auto const subscribed_eventgroups = get_subscriptions(_client, its_lock);
        for (auto its_subscription : subscribed_eventgroups) {
            auto [its_service, its_instance, its_eventgroup] = its_subscription;
            // because we are in the remove local function within which the connection token is bumped,
            // any inflight subscription for this client is going to be dropped, therefore adjust the book-keeping
            // immediately.
            unsubscribe_base(_client, its_service, its_instance, its_eventgroup, ANY_EVENT, its_lock);
            VSOMEIP_INFO << "UNSUBSCRIBE(" << hex4(_client) << "): [" << hex4(its_service) << "." << hex4(its_instance) << "."
                         << hex4(its_eventgroup) << "." << hex4(ANY_EVENT) << "]";
            host_->on_subscription(its_service, its_instance, its_eventgroup, _client, &its_sec_client, env, false, [](bool) {
                // no need to execute anything, subscribers are updated already
            });
        }
        // remove the provider endpoint under the provider_mutex_ to ensure that no subscription callback (dispatcher thread)
        // can mess up the book-keeping when checking the endpoint token
        ep_mgr_->remove_provider_endpoint(_client, _due_to_error);
    }
    remove_sec_client_mapping_if_orphaned(_client);
}

void routing_manager_client::remove_local_consumer(client_t _client, bool _due_to_error, local_service_table& _requested_services) {

    {
        std::scoped_lock its_lock(consumer_mutex_);
        auto removed = available_services_.remove_all_for_client(_client);
        for (auto const& [its_service, its_instance, its_major, its_minor, its_client] : removed) {
            // save the removed available services to re-request them from the router
            _requested_services.insert(protocol::service_data{
                    .service_ = its_service, .instance_ = its_instance, .major_version_ = its_major, .minor_version_ = its_minor});
            on_stop_offer_service(its_service, its_instance, its_major, its_minor, true, its_lock);
        }
        remove_consumer(_client, _due_to_error, its_lock);
    }
    remove_sec_client_mapping_if_orphaned(_client);
}

void routing_manager_client::remove_sec_client_mapping_if_orphaned(client_t _client) {
    // No role mutex is held here; each lookup takes and releases its own lock (endpoint-manager mtx_ then consumer_mutex_), never
    // simultaneously.
    const bool has_any_local_connection = ep_mgr_->find_local_server_endpoint(_client) != nullptr // accepted provider endpoint
            || find_consumer_ep(_client) != nullptr; // outbound consumer endpoint
    if (!has_any_local_connection) {
        configuration_->get_policy_manager()->remove_client_to_sec_client_mapping(_client);
    }
}

void routing_manager_client::cleanup_consumer() {
    std::scoped_lock lock(consumer_mutex_);
    auto removed = available_services_.clear();
    for (auto const& [service, instance, major, minor, client] : removed) {
        on_stop_offer_service(service, instance, major, minor, true, lock);
    }
    for (auto const& [client, data] : consumer_) {
        if (data.ep_) {
            data.ep_->stop(true);
        }
    }
    consumer_.clear();
    if (on_consumer_flushed_) {
        on_consumer_flushed_.fire();
        on_consumer_flushed_ = {};
    }
}

void routing_manager_client::cleanup_subscriber(std::scoped_lock<std::mutex> const& _provider_lock) {
    // Tracks unique (service, instance, eventgroup, client) tuples for which
    // the user notification and table invalidation must fire exactly once.
    struct sub_tuple {
        service_t service;
        instance_t instance;
        eventgroup_t eventgroup;
        client_t client;
        auto operator<=>(const sub_tuple&) const = default;
    };
    std::set<sub_tuple> unique_tuples;

    for (auto const& [si, evs] : provided_events_) {
        auto const service = si.service;
        auto const instance = si.instance;
        for (auto const& [event_id, event] : evs) {
            for (auto const group : event->get_eventgroups()) {
                for (auto const client : event->get_subscribers(group)) {
                    // Per-event routing state mutation — fine to call once per event.
                    unsubscribe_base(client, service, instance, group, event_id, _provider_lock);
                    unique_tuples.insert({service, instance, group, client});
                }
            }
        }
    }

    // Per unique (svc, inst, group, client): invalidate all table entries
    // (ANY_EVENT expansion covers both ANY_EVENT and specific-event subscriptions)
    // and notify the user exactly once.
    for (auto const& [service, instance, group, client] : unique_tuples) {
        auto ep = ep_mgr_->find_local_server_endpoint(client);
        auto const env = ep ? ep->get_env() : "";
        auto const sec = ep ? ep->get_sec_client() : vsomeip_sec_client_t{};
        VSOMEIP_INFO << "UNSUBSCRIBE(" << hex4(client) << "): [" << hex4(service) << "." << hex4(instance) << "." << hex4(group) << "."
                     << hex4(ANY_EVENT) << "]";
        host_->on_subscription(service, instance, group, client, &sec, env, false, [](bool) {
            // no need to execute anything, as the endpoint token is invalidated as well
        });
    }
}

client_t routing_manager_client::get_client_by_address(const boost::asio::ip::address& _address, port_t _port) const {
    std::scoped_lock lock{consumer_mutex_};
    auto const it = std::find_if(consumer_.begin(), consumer_.end(), [&_address, &_port](std::pair<client_t, consumer_data> const& p) {
        return p.second.address_ == _address && p.second.port_ == _port;
    });
    return it == consumer_.end() ? VSOMEIP_CLIENT_UNSET : it->first;
}

client_t routing_manager_client::find_local_client(service_t _service, instance_t _instance) const {
    std::scoped_lock its_lock(consumer_mutex_);
    // TODO major version
    return available_services_.find_client(_service, _instance, ANY_MAJOR);
}

bool routing_manager_client::send_event(client_t _client, std::shared_ptr<message> _message, bool _force) {
    return send(_client, _message, _force);
}

bool routing_manager_client::send(client_t _client, std::shared_ptr<message> _message, bool _force) {
    bool is_sent(false);

    instance_t its_instance = _message->get_instance();
    service_t its_service = _message->get_service();
    method_t its_method = _message->get_method();
    session_t its_session = _message->get_session();
    client_t its_client = _message->get_client();
    message_type_e its_message_type = _message->get_message_type();
    major_version_t its_major = _message->get_interface_version();
    if (utility::is_request(_message->get_message_type())) {
        _message->set_client(_client);
        if (!host_->is_routing() && !is_available(its_service, its_instance, its_major)) {
            VSOMEIP_WARNING_P << "this=" << this << "}::send{_client=" << _client << " _message=" << hex4(its_service) << "."
                              << hex4(its_method) << "." << static_cast<int>(its_message_type) << "."
                              << static_cast<int>(_message->get_return_code()) << " _force=" << _force
                              << "}: Service not available. instance=" << hex4(its_instance) << " version=" << hex4(its_major);
            if (!_force) {
                return is_sent;
            }
        }
    }

    bool has_remote_subscribers{false};
    if (auto const state = state_machine_->state(); state != routing_client_state_e::ST_REGISTERED) {
        VSOMEIP_WARNING_P << "(" << hex4(get_client()) << "): Dropping message for client: " << hex4(_client)
                          << ", due to unexpected state: " << state;
        return false;
    }
    if (client_side_logging_) {
        if (client_side_logging_filter_.empty() || (1 == client_side_logging_filter_.count(std::make_tuple(its_service, ANY_INSTANCE)))
            || (1 == client_side_logging_filter_.count(std::make_tuple(its_service, its_instance)))) {
            VSOMEIP_INFO_P << "(" << hex4(get_client()) << "): [" << hex4(its_service) << "." << hex4(its_instance) << "."
                           << hex4(its_method) << ":" << hex4(its_session) << ":" << hex4(its_client) << "] "
                           << "type=" << std::hex << static_cast<std::uint32_t>(its_message_type) << " thread=" << std::hex
                           << std::this_thread::get_id();
        }
    }
    std::shared_ptr<local_endpoint> its_target;
    if (utility::is_request(its_message_type)) {
        // Request
        client_t its_offerer = find_local_client(its_service, its_instance);
        if (its_offerer != VSOMEIP_ROUTING_CLIENT) {
            its_target = find_or_create_consumer_ep(its_offerer);
            if (!its_target) {
                VSOMEIP_WARNING_P << "No endpoint to service to client: " << hex4(its_offerer) << " found or created";
            }
        }
    } else if (!utility::is_notification(its_message_type)) {
        // Response — target is the original requester (from SOME/IP header)
        if (its_client != VSOMEIP_ROUTING_CLIENT) {
            its_target = ep_mgr_->find_local_server_endpoint(its_client);
        }
    } else if (_client == VSOMEIP_ROUTING_CLIENT) {
        // router (remote) will be notified below
        has_remote_subscribers = true;
    } else {
        // notify_one
        its_target = ep_mgr_->find_local_server_endpoint(_client);
        if (its_target) {
            return its_target->send(protocol::create_send_cmd(protocol::id_e::SEND_ID, get_client(), _message, its_client), tc_);
        }
    }
    // If no direct endpoint could be found
    // or for notifications ~> route to routing_manager_stub
    bool message_to_stub(false);
    if (!its_target) {
        std::scoped_lock its_sender_lock{sender_mutex_};
        if (sender_) {
            its_target = sender_;
            message_to_stub = true;
        } else {
            VSOMEIP_WARNING_P << "No connection to router. Message will be dropped";
            return false;
        }
    }

    bool send_to_target(true);
    protocol::id_e its_command(protocol::id_e::SEND_ID);

    if (utility::is_notification(its_message_type)) {
        if (_client != VSOMEIP_ROUTING_CLIENT) {
            its_command = protocol::id_e::NOTIFY_ONE_ID;
        } else {
            its_command = protocol::id_e::NOTIFY_ID;
            // Do we need to deliver a notification to the routing manager?
            // Only for services which already have remote clients subscribed to
            send_to_target = has_remote_subscribers;
        }
    }
    if (send_to_target) {
        // Only trace requests/responses to local endpoints, not notifications or messages routed via stub.
        // For NOTIFY_ONE the addressee is the specific subscriber (_client), which is not encoded in the
        // shared notification message; for everything else it is the message's own client.
        client_t const its_ipc_target = (its_command == protocol::id_e::NOTIFY_ONE_ID) ? _client : get_client();
        is_sent = its_target->send(protocol::create_send_cmd(its_command, get_client(), _message, its_ipc_target),
                                   (!utility::is_notification(its_message_type) && !message_to_stub) ? tc_ : nullptr);
    }
    return is_sent;
}

bool routing_manager_client::is_available(service_t _service, instance_t _instance, major_version_t _major) const {
    std::scoped_lock its_lock(consumer_mutex_);
    return available_services_.is_available(_service, _instance, _major);
}

void routing_manager_client::collect_pending_subscriptions(service_t _service, instance_t _instance, major_version_t _major,
                                                           std::vector<subscription_data_t>& _collected_subscriptions,
                                                           std::scoped_lock<std::mutex> const&) {
    for (auto& ps : pending_subscriptions_) {
        if (ps.service_instance_ == service_instance_t{_service, _instance} && ps.major_ == _major) {
            _collected_subscriptions.push_back(ps);
        }
    }
}

void routing_manager_client::remove_pending_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                                                         std::scoped_lock<std::mutex> const&) {
    if (_eventgroup == 0xFFFF) {
        std::erase_if(pending_subscriptions_, [&_service, &_instance](const subscription_data_t& ps) {
            return ps.service_instance_ == service_instance_t{_service, _instance};
        });
    } else if (_event == ANY_EVENT) {
        std::erase_if(pending_subscriptions_, [&_service, &_instance, &_eventgroup](const subscription_data_t& ps) {
            return ps.service_instance_ == service_instance_t{_service, _instance} && ps.eventgroup_ == _eventgroup;
        });
    } else {
        std::erase_if(pending_subscriptions_, [&_service, &_instance, &_eventgroup, &_event](const subscription_data_t& ps) {
            return ps.service_instance_ == service_instance_t{_service, _instance} && ps.eventgroup_ == _eventgroup && ps.event_ == _event;
        });
    }
}

void routing_manager_client::register_consumer_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                                     const std::set<eventgroup_t>& _eventgroups, const event_type_e _type,
                                                     reliability_type_e _reliability, std::chrono::milliseconds _cycle,
                                                     bool _change_resets_cycle, bool _update_on_change,
                                                     epsilon_change_func_t _epsilon_change_func, bool _is_cache_placeholder,
                                                     std::scoped_lock<std::mutex> const& _lock) {

    auto determine_event_reliability = [this, &_service, &_instance, &_notifier, &_reliability]() {
        reliability_type_e its_reliability = configuration_->get_event_reliability(_service, _instance, _notifier);
        if (its_reliability != reliability_type_e::RT_UNKNOWN) {
            // event was explicitly configured -> overwrite value passed via API
            return its_reliability;
        } else if (_reliability != reliability_type_e::RT_UNKNOWN) {
            // use value provided via API
            return _reliability;
        } else { // automatic mode, user service' reliability
            return configuration_->get_service_reliability(_service, _instance);
        }
    };

    auto its_event = find_consumed_event(_service, _instance, _notifier, _lock);
    bool transfer_subscriptions_from_any_event(false);
    if (its_event) {
        if (!its_event->is_cache_placeholder()) {
            if (_type == its_event->get_type() || its_event->get_type() == event_type_e::ET_UNKNOWN) {
                for (auto eg : _eventgroups) {
                    its_event->add_eventgroup(eg);
                }
                transfer_subscriptions_from_any_event = true;
            } else {
                VSOMEIP_ERROR_P << ": Event registration update failed. Specified arguments do not match existing registration.";
            }
        } else {
            // the found event was a placeholder for caching.
            // update it with the real values
            if (_type != event_type_e::ET_FIELD) {
                // don't cache payload for non-fields
                if (its_event->is_set()) {
                    VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "."
                                   << hex4(its_event->get_event()) << "]";
                }
                its_event->unset_payload(true);
            }
            its_event->set_type(_type);
            its_event->set_reliability(determine_event_reliability());
            its_event->set_provided(false);
            its_event->set_cache_placeholder(false);
            if (_eventgroups.size() == 0) { // No eventgroup specified
                std::set<eventgroup_t> its_eventgroups;
                its_eventgroups.insert(_notifier);
                its_event->set_eventgroups(its_eventgroups);
            } else {
                for (auto eg : _eventgroups) {
                    its_event->add_eventgroup(eg);
                }
            }

            its_event->set_epsilon_change_function(_epsilon_change_func);
            its_event->set_change_resets_cycle(_change_resets_cycle);
            its_event->set_update_cycle(_cycle);
        }
    } else {
        its_event = std::make_shared<event>(io_, *this, false, false);
        its_event->set_service(_service);
        its_event->set_instance(_instance);
        its_event->set_event(_notifier);
        its_event->set_type(_type);
        its_event->set_reliability(determine_event_reliability());
        its_event->set_provided(false);
        its_event->set_cache_placeholder(_is_cache_placeholder);

        if (_eventgroups.size() == 0) { // No eventgroup specified
            std::set<eventgroup_t> its_eventgroups;
            its_eventgroups.insert(_notifier);
            its_event->set_eventgroups(its_eventgroups);
        } else {
            its_event->set_eventgroups(_eventgroups);
        }

        its_event->set_epsilon_change_function(_epsilon_change_func);
        its_event->set_change_resets_cycle(_change_resets_cycle);
        its_event->set_update_cycle(_cycle);
        its_event->set_update_on_change(_update_on_change);
    }

    if (transfer_subscriptions_from_any_event) {
        // check if someone subscribed to ANY_EVENT and the subscription
        // was stored in the cache placeholder. Move the subscribers
        // into new event

        auto its_any_event = find_consumed_event(_service, _instance, ANY_EVENT, _lock);
        if (its_any_event) {
            std::set<eventgroup_t> any_events_eventgroups = its_any_event->get_eventgroups();
            for (eventgroup_t eventgroup : _eventgroups) {
                auto found_eg = any_events_eventgroups.find(eventgroup);
                if (found_eg != any_events_eventgroups.end()) {
                    std::set<client_t> its_any_event_subscribers = its_any_event->get_subscribers(eventgroup);
                    for (const client_t subscriber : its_any_event_subscribers) {
                        its_event->add_subscriber(eventgroup, nullptr, subscriber, true);
                    }
                }
            }
        }
    }
    if (!_is_cache_placeholder) {
        its_event->add_ref(_client, false);
    }

    for (auto eg : _eventgroups) {
        auto its_eventgroupinfo = find_consumer_eventgroup(_service, _instance, eg, _lock);
        if (!its_eventgroupinfo) {
            its_eventgroupinfo = std::make_shared<eventgroupinfo>();
            its_eventgroupinfo->set_service(_service);
            its_eventgroupinfo->set_instance(_instance);
            its_eventgroupinfo->set_eventgroup(eg);
            its_eventgroupinfo->set_max_remote_subscribers(configuration_->get_max_remote_subscribers());
            consumed_eventgroups_[service_instance_t{_service, _instance}][eg] = its_eventgroupinfo;
        }
        its_eventgroupinfo->add_event(its_event);
    }
    consumed_events_[service_instance_t{_service, _instance}][_notifier].event_ = its_event;
}

void routing_manager_client::register_provider_event(client_t _client, service_t _service, instance_t _instance, event_t _notifier,
                                                     const std::set<eventgroup_t>& _eventgroups, const event_type_e _type,
                                                     reliability_type_e _reliability, std::chrono::milliseconds _cycle,
                                                     bool _change_resets_cycle, bool _update_on_change,
                                                     epsilon_change_func_t _epsilon_change_func, bool _is_cache_placeholder,
                                                     std::scoped_lock<std::mutex> const& _lock) {

    if (auto const* service = offered_services_.find({_service, _instance, ANY_MAJOR, ANY_MINOR}); service) {
        VSOMEIP_ERROR_P << "Application [" << hex4(get_client()) << ", '" << host_->get_name() << "', uid " << host_->get_sec_client_uid()
                        << "] registering events for [" << hex4(_service) << "." << hex4(_instance)
                        << "], after already offering the service is wrong behavior! Potentially missing major version on "
                           "following event messages";
    }

    auto determine_event_reliability = [this, &_service, &_instance, &_notifier, &_reliability]() {
        reliability_type_e its_reliability = configuration_->get_event_reliability(_service, _instance, _notifier);
        if (its_reliability != reliability_type_e::RT_UNKNOWN) {
            // event was explicitly configured -> overwrite value passed via API
            return its_reliability;
        } else if (_reliability != reliability_type_e::RT_UNKNOWN) {
            // use value provided via API
            return _reliability;
        } else { // automatic mode, user service' reliability
            return configuration_->get_service_reliability(_service, _instance);
        }
    };

    auto its_event = find_provided_event(_service, _instance, _notifier, _lock);
    bool transfer_subscriptions_from_any_event(false);
    if (its_event) {
        if (!its_event->is_cache_placeholder()) {
            if (_type == its_event->get_type() || its_event->get_type() == event_type_e::ET_UNKNOWN) {
                its_event->set_provided(true);
                its_event->set_reliability(determine_event_reliability());
                if (_client == host_->get_client()) {
                    its_event->set_update_on_change(_update_on_change);
                }
                for (auto eg : _eventgroups) {
                    its_event->add_eventgroup(eg);
                }
                transfer_subscriptions_from_any_event = true;
            } else {
                VSOMEIP_ERROR_P << ": Event registration update failed. Specified arguments do not match existing registration.";
            }
        } else {
            // the found event was a placeholder for caching.
            // update it with the real values
            if (_type != event_type_e::ET_FIELD) {
                // don't cache payload for non-fields
                if (its_event->is_set()) {
                    VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "."
                                   << hex4(its_event->get_event()) << "]";
                }
                its_event->unset_payload(true);
            }
            if (_client == host_->get_client()) {
                its_event->set_update_on_change(_update_on_change);
            }
            its_event->set_type(_type);
            its_event->set_reliability(determine_event_reliability());
            its_event->set_provided(true);
            its_event->set_cache_placeholder(false);

            if (auto const* its_service = offered_services_.find({_service, _instance, ANY_MAJOR, ANY_MINOR}); its_service) {
                its_event->set_version(its_service->major_version_);
            }
            if (_eventgroups.size() == 0) { // No eventgroup specified
                std::set<eventgroup_t> its_eventgroups;
                its_eventgroups.insert(_notifier);
                its_event->set_eventgroups(its_eventgroups);
            } else {
                for (auto eg : _eventgroups) {
                    its_event->add_eventgroup(eg);
                }
            }

            its_event->set_epsilon_change_function(_epsilon_change_func);
            its_event->set_change_resets_cycle(_change_resets_cycle);
            its_event->set_update_cycle(_cycle);
        }
    } else {
        its_event = std::make_shared<event>(io_, *this, false, false);
        its_event->set_service(_service);
        its_event->set_instance(_instance);
        its_event->set_event(_notifier);
        its_event->set_type(_type);
        its_event->set_reliability(determine_event_reliability());
        its_event->set_provided(true);
        its_event->set_cache_placeholder(_is_cache_placeholder);

        if (auto const* its_service = offered_services_.find({_service, _instance, ANY_MAJOR, ANY_MINOR}); its_service) {
            // TODO this is obviously wrong as soon as a client would like to offer multiple major versions
            its_event->set_version(its_service->major_version_);
        }

        if (_eventgroups.size() == 0) { // No eventgroup specified
            std::set<eventgroup_t> its_eventgroups;
            its_eventgroups.insert(_notifier);
            its_event->set_eventgroups(its_eventgroups);
        } else {
            its_event->set_eventgroups(_eventgroups);
        }

        its_event->set_epsilon_change_function(_epsilon_change_func);
        its_event->set_change_resets_cycle(_change_resets_cycle);
        its_event->set_update_cycle(_cycle);
        its_event->set_update_on_change(_update_on_change);
        transfer_subscriptions_from_any_event = true;
    }

    if (transfer_subscriptions_from_any_event) {
        // check if someone subscribed to ANY_EVENT and the subscription
        // was stored in the cache placeholder. Move the subscribers
        // into new event
        auto its_any_event = find_provided_event(_service, _instance, ANY_EVENT, _lock);
        if (its_any_event) {
            std::set<eventgroup_t> any_events_eventgroups = its_any_event->get_eventgroups();
            for (eventgroup_t eventgroup : _eventgroups) {
                auto found_eg = any_events_eventgroups.find(eventgroup);
                if (found_eg != any_events_eventgroups.end()) {
                    std::set<client_t> its_any_event_subscribers = its_any_event->get_subscribers(eventgroup);
                    for (const client_t subscriber : its_any_event_subscribers) {
                        its_event->add_subscriber(eventgroup, nullptr, subscriber, true);
                    }
                }
            }
        }
    }
    if (!_is_cache_placeholder) {
        its_event->add_ref(_client, true);
    }

    provided_events_[service_instance_t{_service, _instance}][_notifier] = its_event;
}

void routing_manager_client::unregister_event_base(client_t _client, service_t _service, instance_t _instance, event_t _event,
                                                   bool _is_provided) {
    std::shared_ptr<event> its_unrefed_event;

    if (_is_provided) {
        auto remove_client_ref = [_client, _service, _instance, _event, _is_provided, &its_unrefed_event](auto& events_map) {
            auto it_search = events_map.find(service_instance_t{_service, _instance});
            if (it_search == events_map.end()) {
                return;
            }
            const auto found_event = it_search->second.find(_event);
            if (found_event != it_search->second.end()) {
                auto its_event = found_event->second;
                its_event->remove_ref(_client, _is_provided);
                if (!its_event->has_ref()) {
                    its_unrefed_event = its_event;
                    it_search->second.erase(found_event);
                } else if (_is_provided) {
                    its_event->set_provided(false);
                }
            }
        };
        std::scoped_lock its_lock(provider_mutex_);
        remove_client_ref(provided_events_);
    } else {
        std::scoped_lock its_lock(consumer_mutex_);
        auto it_search = consumed_events_.find(service_instance_t{_service, _instance});
        if (it_search != consumed_events_.end()) {
            auto found_entry = it_search->second.find(_event);
            if (found_entry != it_search->second.end()) {
                auto& entry = found_entry->second;
                if (entry.event_) {
                    entry.event_->remove_ref(_client, false);
                    if (!entry.event_->has_ref()) {
                        its_unrefed_event = entry.event_;
                        entry.event_ = nullptr;
                        if (entry.subscriptions_.empty()) {
                            it_search->second.erase(found_entry);
                        }
                    }
                }
            }
        }
    }

    if (its_unrefed_event && !_is_provided) {
        auto its_eventgroups = its_unrefed_event->get_eventgroups();
        for (auto eg : its_eventgroups) {
            auto its_eventgroup_info = find_consumer_eventgroup(_service, _instance, eg);
            if (its_eventgroup_info) {
                its_eventgroup_info->remove_event(its_unrefed_event);
                if (0 == its_eventgroup_info->get_events().size()) {
                    remove_consumer_eventgroup_info(_service, _instance, eg);
                }
            }
        }
    }
}

void routing_manager_client::remove_consumer_eventgroup_info(service_t _service, instance_t _instance, eventgroup_t _eventgroup) {
    std::scoped_lock lck(consumer_mutex_);
    const auto search = consumed_eventgroups_.find(service_instance_t{_service, _instance});
    if (search != consumed_eventgroups_.end()) {
        const auto found_eventgroup = search->second.find(_eventgroup);
        if (found_eventgroup != search->second.end()) {
            search->second.erase(found_eventgroup);
        }
    }
}

std::set<std::shared_ptr<event>> routing_manager_client::find_consumed_events(service_t _service, instance_t _instance,
                                                                              eventgroup_t _eventgroup) const {
    std::scoped_lock its_lock(consumer_mutex_);
    const auto search = consumed_eventgroups_.find(service_instance_t{_service, _instance});
    if (search != consumed_eventgroups_.end()) {
        const auto found_eventgroup = search->second.find(_eventgroup);
        if (found_eventgroup != search->second.end()) {
            return found_eventgroup->second->get_events();
        }
    }

    return std::set<std::shared_ptr<event>>();
}

void routing_manager_client::unsubscribe_base(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                              event_t _event, std::scoped_lock<std::mutex> const& _lock) {

    if (_event != ANY_EVENT) {
        auto its_event = find_provided_event(_service, _instance, _event, _lock);
        if (its_event) {
            its_event->remove_subscriber(_eventgroup, _client);
        }
    } else {
        for (auto const& event : find_provided_events_by_group(_service, _instance, _eventgroup, _lock)) {
            event->remove_subscriber(_eventgroup, _client);
        }
    }
}

bool routing_manager_client::insert_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                                                 const std::shared_ptr<debounce_filter_impl_t>& _filter, client_t _client,
                                                 std::scoped_lock<std::mutex> const& _lock) {

    bool is_inserted(false);
    if (_event != ANY_EVENT) { // subscribe to specific event
        std::shared_ptr<event> its_event = find_provided_event(_service, _instance, _event, _lock);
        if (its_event) {
            is_inserted = its_event->add_subscriber(_eventgroup, _filter, _client, false);
        } else {
            VSOMEIP_WARNING_P << "(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(_eventgroup)
                              << "." << hex4(_event) << "] received subscription for unknown (unrequested /unoffered) event. Creating"
                              << " placeholder event holding subscription until event is requested/offered.";
            is_inserted = create_placeholder_event_and_subscribe(_service, _instance, _eventgroup, _event, _filter, _client, _lock);
        }
    } else { // subscribe to all events of the eventgroup
        auto its_events = find_provided_events_by_group(_service, _instance, _eventgroup, _lock);
        if (!its_events.empty()) {
            for (auto const& event : its_events) {
                is_inserted = event->add_subscriber(_eventgroup, _filter, _client, false) || is_inserted;
            }
        } else {
            VSOMEIP_WARNING_P << ":(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(_eventgroup)
                              << "." << hex4(_event) << "] received subscription for unknown (unrequested /unoffered) eventgroup. Creating"
                              << " placeholder event holding subscription until event is requested/offered.";
            is_inserted = create_placeholder_event_and_subscribe(_service, _instance, _eventgroup, _event, _filter, _client, _lock);
        }
    }
    return is_inserted;
}

std::set<std::tuple<service_t, instance_t, eventgroup_t>>
routing_manager_client::get_subscriptions(const client_t _client,
                                          [[maybe_unused]] std::scoped_lock<std::mutex> const& _provider_lock) const {
    std::set<std::tuple<service_t, instance_t, eventgroup_t>> result;
    for (const auto& [key, eventmap] : provided_events_) {
        for (auto [event_id, event] : eventmap) {
            auto its_eventgroups = event->get_eventgroups(_client);
            for (const auto& e : its_eventgroups) {
                result.insert(std::make_tuple(key.service, key.instance, e));
            }
        }
    }

    return result;
}

void routing_manager_client::notify_one(service_t _service, instance_t _instance, event_t _event, std::shared_ptr<payload> _payload,
                                        client_t _client, bool _force) {
    std::scoped_lock its_lock{provider_mutex_};
    if (!offered_services_.contains({_service, _instance, ANY_MAJOR, ANY_MINOR})) {
        // Note that notify_one should really only be used for selective broadcast
        VSOMEIP_ERROR_P << "Attempt to update a event/field for a not provided service [" << hex4(_service) << "." << hex4(_instance) << "."
                        << hex4(_event) << "]";
        return;
    }
    std::shared_ptr<event> its_event = find_provided_event(_service, _instance, _event, its_lock);
    if (its_event) {
        const bool is_local = ep_mgr_->find_local_server_endpoint(_client) != nullptr;
        if (!is_local || its_event->is_subscribed(_client)) {
            its_event->set_payload(_payload, _client, _force);
        } else {
            VSOMEIP_ERROR_P << "Attempt to notify the not-subscribed client: 0x" << hex4(_client) << " about the event/field ["
                            << hex4(_service) << "." << hex4(_instance) << "." << hex4(_event) << "]";
        }
    } else {
        VSOMEIP_ERROR_P << "Attempt to update the undefined event/field [" << hex4(_service) << "." << hex4(_instance) << "."
                        << hex4(_event) << "]";
    }
}

void routing_manager_client::notify_one_current_value(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                      event_t _event, std::scoped_lock<std::mutex> const& _lock) {
    if (_event != ANY_EVENT) {
        std::shared_ptr<event> its_event = find_provided_event(_service, _instance, _event, _lock);
        if (its_event && its_event->is_field())
            its_event->notify_one(_client, false);
    } else {
        for (auto const& event : find_provided_events_by_group(_service, _instance, _eventgroup, _lock)) {
            if (event->is_field()) {
                event->notify_one(_client, false);
            }
        }
    }
}

void routing_manager_client::notify(service_t _service, instance_t _instance, event_t _event, std::shared_ptr<payload> _payload,
                                    bool _force) {

    std::scoped_lock its_lock{provider_mutex_};
    std::shared_ptr<event> its_event = find_provided_event(_service, _instance, _event, its_lock);
    if (its_event) {
        its_event->set_payload(_payload, _force);
    } else {
        VSOMEIP_WARNING_P << "Attempt to update the undefined event/field [" << hex4(_service) << "." << hex4(_instance) << "."
                          << hex4(_event) << "]";
    }
}

bool routing_manager_client::is_subscribe_to_any_event_allowed(const vsomeip_sec_client_t* _sec_client, client_t _client,
                                                               service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                               bool _is_provided) {

    auto const is_allowed = [&](auto const& event) {
        bool const val = VSOMEIP_SEC_OK
                == configuration_->get_security()->is_client_allowed_to_access_member(_sec_client, _service, _instance, event->get_event());
        if (!val) {
            VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(_client)
                          << " : routing_manager_client::is_subscribe_to_any_event_allowed: "
                          << "subscribes to service/instance/event " << hex4(_service) << "/" << hex4(_instance) << "/"
                          << hex4(event->get_event()) << " which violates the security policy!";
        }
        return val;
    };
    if (_is_provided) {
        std::scoped_lock its_lock(provider_mutex_);
        for (auto const& event : find_provided_events_by_group(_service, _instance, _eventgroup, its_lock)) {
            if (!is_allowed(event)) {
                return false;
            }
        }
    } else {
        auto its_eventgroup = find_consumer_eventgroup(_service, _instance, _eventgroup);
        if (its_eventgroup) {
            for (const auto& e : its_eventgroup->get_events()) {
                if (!is_allowed(e)) {
                    return false;
                }
            }
        }
    }

    return true;
}

std::shared_ptr<event> routing_manager_client::find_provided_event(service_t _service, instance_t _instance, event_t _event) const {
    std::scoped_lock its_lock(provider_mutex_);
    return find_provided_event(_service, _instance, _event, its_lock);
}
std::shared_ptr<event> routing_manager_client::find_provided_event(service_t _service, instance_t _instance, event_t _event,
                                                                   [[maybe_unused]] std::scoped_lock<std::mutex> const& _lock) const {
    std::shared_ptr<event> its_event;
    if (const auto search = provided_events_.find(service_instance_t{_service, _instance}); search != provided_events_.end()) {
        const auto found_event = search->second.find(_event);
        if (found_event != search->second.end()) {
            its_event = found_event->second;
        }
    }
    return its_event;
}

std::shared_ptr<event> routing_manager_client::find_consumed_event(service_t _service, instance_t _instance, event_t _event) const {
    std::scoped_lock its_lock(consumer_mutex_);
    return find_consumed_event(_service, _instance, _event, its_lock);
}
std::shared_ptr<event> routing_manager_client::find_consumed_event(service_t _service, instance_t _instance, event_t _event,
                                                                   [[maybe_unused]] std::scoped_lock<std::mutex> const& _lock) const {
    std::shared_ptr<event> its_event;
    if (const auto search = consumed_events_.find(service_instance_t{_service, _instance}); search != consumed_events_.end()) {
        const auto found_event = search->second.find(_event);
        if (found_event != search->second.end()) {
            its_event = found_event->second.event_;
        }
    }
    return its_event;
}

std::shared_ptr<eventgroupinfo> routing_manager_client::find_consumer_eventgroup(service_t _service, instance_t _instance,
                                                                                 eventgroup_t _eventgroup) const {
    return find_consumer_eventgroup(_service, _instance, _eventgroup, std::scoped_lock{consumer_mutex_});
}

std::set<std::shared_ptr<event>>
routing_manager_client::find_provided_events_by_group(service_t _service, instance_t _instance, eventgroup_t _group,
                                                      [[maybe_unused]] std::scoped_lock<std::mutex> const& _provider_lock) const {

    std::set<std::shared_ptr<event>> its_events;
    if (const auto search = provided_events_.find(service_instance_t{_service, _instance}); search != provided_events_.end()) {
        for (auto const& [_, event] : search->second) {
            if (event && event->is_part_of(_group)) {
                its_events.insert(event);
            }
        }
    }
    return its_events;
}

std::shared_ptr<eventgroupinfo> routing_manager_client::find_consumer_eventgroup(service_t _service, instance_t _instance,
                                                                                 eventgroup_t _eventgroup,
                                                                                 std::scoped_lock<std::mutex> const&) const {
    if (const auto search = consumed_eventgroups_.find(service_instance_t{_service, _instance}); search != consumed_eventgroups_.end()) {
        const auto found_eventgroup = search->second.find(_eventgroup);
        if (found_eventgroup != search->second.end()) {
            return found_eventgroup->second;
        }
    }
    return std::shared_ptr<eventgroupinfo>();
}

void routing_manager_client::stop_offer_service_base(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                     minor_version_t _minor, [[maybe_unused]] std::scoped_lock<std::mutex> const& _lock) {
    (void)_client;
    (void)_major;
    (void)_minor;

    const auto search = provided_events_.find(service_instance_t{_service, _instance});
    if (search != provided_events_.end()) {
        for (const auto& [event_id, event_ptr] : search->second) {
            if (event_ptr->is_set()) {
                VSOMEIP_INFO_P << "Unsetting payload for [" << hex4(_service) << "." << hex4(_instance) << "." << hex4(event_id) << "]";
            }
            event_ptr->unset_payload();
            event_ptr->clear_subscribers();
        }
    }
}

session_t routing_manager_client::get_event_session() {
    return host_->get_session(false);
}

bool routing_manager_client::send_event_to(const client_t, const std::shared_ptr<endpoint_definition>&, std::shared_ptr<message>) {
    VSOMEIP_ERROR_P << "Not implemented";
    return false;
}
client_t routing_manager_client::get_client() const {

    return host_->get_client();
}

void routing_manager_client::finish_shutdown() {
    {
        // By now all endpoints have been stopped
        // -> all still queued continuations will be marked "stale"
        std::scoped_lock its_lock(provider_mutex_);
        // also mark boardnet continuations that are in flight as stale
        ++lc_count_;
        // any in-flight continuation is now either "stale" or "done" as it had acquired the provider lock.
        clear_remote_subscriptions(its_lock);
        cleanup_subscriber(its_lock);
    }
    {
        std::scoped_lock lock(consumer_mutex_);
        auto removed = available_services_.clear();
        for (auto const& [service, instance, major, minor, client] : removed) {
            on_stop_offer_service(service, instance, major, minor, true, lock);
        }
    }
    if (status_logger_) {
        status_logger_->stop();
    }

    host_->on_state(state_type_e::ST_DEREGISTERED);
}
std::string const& routing_manager_client::get_name() const {
    return host_->get_name();
}

std::string routing_manager_client::get_client_host() const {
    return env_;
}

vsomeip_sec_client_t routing_manager_client::get_sec_client() const {
    return host_->get_sec_client();
}

void routing_manager_client::set_sec_client_port(port_t _port) {
    host_->set_sec_client_port(_port);
}

bool routing_manager_client::is_available(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor) const {
    std::scoped_lock its_lock{consumer_mutex_};
    return available_services_.has_available(_service, _instance, _major, _minor);
}

void routing_manager_client::register_availability_handler(service_t _service, instance_t _instance, major_version_t _major,
                                                           minor_version_t _minor,
                                                           const std::function<void(bool _is_available)>& _register_handler) {
    std::scoped_lock its_lock{consumer_mutex_};
    if (_service != ANY_SERVICE && _instance != ANY_INSTANCE) {
        // Concrete service/instance: register the handler atomically against the current availability.
        _register_handler(available_services_.has_available(_service, _instance, _major, _minor));
    } else {
        // Wildcards can't resolve to a single state: register as not-available, then replay every matching
        // instance so the handler is invoked for each one currently available (all under consumer_mutex_).
        _register_handler(false);
        available_services_.for_each_available(_service, _instance, _major, _minor, [this](const local_offering_table::entry& e) {
            host_->on_availability(e.service, e.instance, availability_state_e::AS_AVAILABLE, e.major, e.minor);
            return true;
        });
    }
}

bool routing_manager_client::are_available(available_t& _available, service_t _service, instance_t _instance, major_version_t _major,
                                           minor_version_t _minor) const {
    std::scoped_lock its_lock{consumer_mutex_};

    available_services_.for_each_available(_service, _instance, _major, _minor, [&_available](const local_offering_table::entry& e) {
        _available[e.service][e.instance][e.major] = e.minor;
        return true;
    });

    return !_available.empty();
}

void routing_manager_client::send_back_cached_event_unlocked(service_t _service, instance_t _instance, event_t _event,
                                                             std::scoped_lock<std::mutex> const& _consumer_lock) {
    auto its_event = find_consumed_event(_service, _instance, _event, _consumer_lock);
    if (its_event && its_event->is_field() && its_event->is_set()) {
        auto its_message = runtime::get()->create_notification();
        its_message->set_service(_service);
        its_message->set_method(_event);
        its_message->set_instance(_instance);
        its_message->set_payload(its_event->get_payload());
        its_message->set_initial(true);
        host_->on_message(std::move(its_message));
        VSOMEIP_INFO << "Sending back cached event (" << hex4(get_client()) << "): [" << hex4(_service) << "." << hex4(_instance) << "."
                     << hex4(_event) << "]";
    }
}

void routing_manager_client::send_back_cached_eventgroup_unlocked(service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                                                  std::scoped_lock<std::mutex> const&) {
    const auto search = consumed_eventgroups_.find(service_instance_t{_service, _instance});
    if (search == consumed_eventgroups_.end()) {
        return;
    }
    const auto found_eventgroup = search->second.find(_eventgroup);
    if (found_eventgroup == search->second.end()) {
        return;
    }
    for (const auto& e : found_eventgroup->second->get_events()) {
        if (e && e->is_field() && e->is_set()) {
            auto its_message = runtime::get()->create_notification();
            its_message->set_service(_service);
            its_message->set_method(e->get_event());
            its_message->set_instance(_instance);
            its_message->set_payload(e->get_payload());
            its_message->set_initial(true);
            host_->on_message(std::move(its_message));
            VSOMEIP_INFO << "Sending back cached event (" << hex4(get_client()) << "): [" << hex4(_service) << "." << hex4(_instance) << "."
                         << hex4(e->get_event()) << "] from eventgroup " << hex4(_eventgroup);
        }
    }
}

std::shared_ptr<local_endpoint> routing_manager_client::find_or_create_consumer_ep(client_t _client) {
    std::scoped_lock lock{consumer_mutex_};
    auto const it = consumer_.find(_client);
    if (it == consumer_.end()) {
        // because we add unconditionally an entry to this map, upon receiving routing_info
        // -> not finding any entry means we better not connect (would be possible via uds)
        VSOMEIP_WARNING_P << "No consumer entry found for: 0x" << hex4(_client);
        return nullptr;
    }
    if (!it->second.ep_) {
        auto ep = ep_mgr_->create_consumer_endpoint(_client, get_client(), it->second.address_, it->second.port_);
        if (!ep) {
            return nullptr;
        }
        it->second.ep_ = ep;
        // TODO this should be adjusted, but it does imply we need to take a deep look how to delete what security mapping
        register_error_handler(_client, ep, connection_role_e::consumer);
        ep->start();
    }
    return it->second.ep_;
}

std::shared_ptr<local_endpoint> routing_manager_client::find_consumer_ep(client_t _client) {
    std::scoped_lock lock{consumer_mutex_};
    auto const it = consumer_.find(_client);
    return it == consumer_.end() ? nullptr : it->second.ep_;
}

void routing_manager_client::remove_consumer(client_t _client, bool _due_to_error,
                                             [[maybe_unused]] std::scoped_lock<std::mutex> const& _consumer_lock) {
    VSOMEIP_INFO_P << "self 0x" << hex4(get_client_id()) << ", client 0x" << hex4(_client) << ", error " << _due_to_error;
    if (auto const it = consumer_.find(_client); it != consumer_.end()) {
        if (it->second.ep_) {
            it->second.ep_->stop(_due_to_error);
        }
        consumer_.erase(it);
        if (on_consumer_flushed_) {
            for (auto const& [_, data] : consumer_) {
                if (data.ep_) {
                    return;
                }
            }
            // no endpoint remains (note that it would not suffice to check for emptiness, as we might have received routing_info
            on_consumer_flushed_.fire();
            on_consumer_flushed_ = {};
        }
    }
}

async::hook routing_manager_client::flush_consumer() {
    std::scoped_lock its_lock{consumer_mutex_};
    assert(!on_consumer_flushed_);
    on_consumer_flushed_ = async::trigger(io_);
    bool done{true};
    for (auto const& [client, data] : consumer_) {
        if (data.ep_) {
            done = false;
            data.ep_->start_flushing();
        }
    }
    auto ret = on_consumer_flushed_.get_hook();
    if (done) {
        on_consumer_flushed_.fire();
        on_consumer_flushed_ = {};
    }
    return ret;
}

} // namespace vsomeip_v3
