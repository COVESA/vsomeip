// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_APPLICATION_HPP
#define VSOMEIP_APPLICATION_HPP

#include <chrono>
#include <memory>
#include <set>
#include <map>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/function_types.hpp>
#include <vsomeip/constants.hpp>
#include <vsomeip/handler.hpp>

namespace vsomeip {

class configuration;
class event;
class payload;

/**
 * \defgroup Application
 *
 * This is the main API for a VSOMEIP client. All methods regarding sending and receiving data as well as registering
 * callbacks can be found here. The central class is called @ref application.
 *
 * @{
 */

/**
 *
 * \brief This class contains the public API of the VSOMEIP implementation.
 *
 * Due to its heavy resource footprint, it should exist once per client and can be instantiated using the API of @ref runtime.
 * It manages the lifecycle of the VSOMEIP client and allocates all resources needed to communicate.
 *
 *
 */
class application {
public:
    virtual ~application() {}

    /**
     *
     * \brief Returns the name of the application as given during creation
     *
     * \return Application name
     *
     */
    virtual const std::string & get_name() const = 0;

    /**
     *
     * \brief Returns the client ID as configured in vsomeipd.json.
     *
     * \return Client ID of application
     *
     */
    virtual client_t get_client() const = 0;

    virtual void set_configuration(const std::shared_ptr<configuration> _configuration) = 0;

    // Lifecycle

    /**
     *
     * \brief Initializes the application.
     *
     * This includes the following steps:
     * - Loading the configuration from a dynamic module
     * - Loading the configuration from a json file
     * - Determining routing configuration and intialization of the routing itself
     * - Installing signal handlers
     *
     */
    virtual bool init() = 0;

    /**
     *
     * \brief Starts the main loop.
     *
     * This method will block until the message processing is terminated using the @ref stop method or by receiving
     * signals. It processes messages received via the sockets and uses registered callbacks to pass them to the client
     * application.
     *
     */
    virtual void start() = 0;

    /**
     *
     * \brief Ends the main loop.
     *
     * This method ends message processing and @ref start will return.
     *
     */
    virtual void stop() = 0;

    // Provide services
    /**
     *
     * \brief Offers the given service to clients of vsomeipd.
     *
     * The service is offered either internally and/or externally, depending on the service configuration.
     *
     * \param _service Service ID of the offered service interface.
     * \param _instance Instance ID of the offered service instance.
     * \param _major Major service version
     * \param _minor Minor service version
     *
     */
    virtual void offer_service(service_t _service, instance_t _instance,
            major_version_t _major = DEFAULT_MAJOR, minor_version_t _minor =
                    DEFAULT_MINOR) = 0;

    /**
     *
     * \brief Stops offering the given service.
     *
     * \param _service Service ID of the offered service interface.
     * \param _instance Instance ID of the offered service instance.
     * \param _major Major service version.
     * \param _minor Minor service version.
     *
     */
    virtual void stop_offer_service(service_t _service, instance_t _instance,
            major_version_t _major = DEFAULT_MAJOR, minor_version_t _minor =
                    DEFAULT_MINOR) = 0;

    /**
     *
     * \brief Offers the given event to clients of the given service.
     *
     * \param _service Service ID of the service interface the event is part of.
     * \param _instance Instance ID of the service instance the event is part of.
     * \param _event Event ID of the offered event.
     * \param _eventgroups List of event groups the event is part of.
     *
     */
    virtual void offer_event(service_t _service,
            instance_t _instance, event_t _event,
            const std::set<eventgroup_t> &_eventgroups,
            bool _is_field) = 0;

    /**
     *
     * \brief Stops offering a certain event.
     *
     * \param _service Service ID of the service interface the event is part of.
     * \param _instance Instance ID of the service instance the event is part of.
     * \param _event Event ID of the offered event.
     *
     */
    virtual void stop_offer_event(service_t _service,
            instance_t _instance, event_t _event) = 0;

    // Consume services
    /**
     *
     * \brief Requests a service and registers this application as client of the service.
     *
     * If service discovery is turned on, it will be used to find services that were unknown to the system.
     *
     * \param _service Service ID of the offered service interface.
     * \param _instance Instance ID of the offered service instance.
     * \param _major Major service version.
     * \param _minor Minor service version.
     * \param _use_exclusive_proxy TBD / unknown
     *
     */
    virtual void request_service(service_t _service, instance_t _instance,
            major_version_t _major = ANY_MAJOR,
            minor_version_t _minor = ANY_MINOR,
            bool _use_exclusive_proxy = false) = 0;

    /**
     *
     * \brief Unregister service usage.
     *
     * SD is stopped if this was the last application to use the service.
     *
     * \param _service Service ID of the offered service interface.
     * \param _instance Instance ID of the offered service instance.
     *
     */
    virtual void release_service(service_t _service, instance_t _instance) = 0;

    /**
     *
     * \brief Registers a certain event with the vsomeip subsystem.
     *
     * Internally, a ref counting mechanism is used to keep track of the number of
     * times a certain event was requested by the applications.
     *
     * TODO: What is this call good for? When is it needed?
     *
     * \param _service Service ID of the service interface the event belongs to.
     * \param _instance Instance ID of the service instance the event belongs to.
     * \param _event Event ID of the event to be registered.
     * \param _eventgroups Event groups this specific event is part of.
     * \param _is_field true if the registered event is in fact a filed, i.e. has setter and/or getter.
     *
     */
    virtual void request_event(service_t _service, instance_t _instance,
            event_t _event, const std::set<eventgroup_t> &_eventgroups,
            bool _is_field) = 0;
    /**
     *
     * \brief Removes registration of a certain event for this client
     *
     * If the ref count reaches zero, the event gets removed from the list of
     * known events
     *
     * \param _service Service ID of the service interface the event belongs to.
     * \param _instance Instance ID of the service instance the event belongs to.
     * \param _event Event ID of the event to be released
     * .
     */
    virtual void release_event(service_t _service, instance_t _instance,
            event_t _event) = 0;

    /**
     *
     * \brief Subscribes to the given event group.
     *
     * This call leads to an event subscription including the SOME/IP conforming subscription messages sent over the
     * network.
     *
     * If the ECU already received some data, this will also lead to a forged
     * notification per event in the given event group so that the current data
     * is delivered to the application without requiring a real notification or a
     * call to Get in case of a field.
     *
     * \param _service Service ID of the service interface the event group belongs to.
     * \param _instance Instance ID of the service instance the event group belongs to.
     * \param _eventgroup Event group that is to be subscribed. Every event of the event group will then be received by the ECU.
     * \param _major Major version number of the service to be subscribed.
     * \param _subscription_type Hint about the reliability of the requested event group.
     * \param _event Event which is required. This will lead to a lower amount of initial notifications sent, as the vsomeip daemon will omit all other events of the event group.
     *
     */
    virtual void subscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup, major_version_t _major = DEFAULT_MAJOR,
            subscription_type_e _subscription_type = subscription_type_e::SU_RELIABLE_AND_UNRELIABLE,
            event_t _event = ANY_EVENT) = 0;

    /**
     *
     * \brief Unsubscribes from a certain event group.
     *
     * \param _service Service ID of the service interface the event group belongs to.
     * \param _instance Instance ID of the service instance the event group belongs to.
     * \param _eventgroup Event group that is to be unsubscribed.
     *
     */
    virtual void unsubscribe(service_t _service, instance_t _instance,
            eventgroup_t _eventgroup) = 0;

    /**
     *
     * \brief Checks for the availability of a certain service instance.
     *
     * If the version is also given, the result will only be true if the service instance
     * is available in that specific version.
     *
     * \param _service Service ID of the service interface to be checked.
     * \param _instance Instance ID of the service instance to be checked.
     * \param _major Major interface version. Can be set to DEFAULT_MAJOR to ignore the major version number.
     * \param _minor Minor interface version. Can be set to DEFAULT_MINOR to ignore the minor version number.
     *
     */
    virtual bool is_available(service_t _service, instance_t _instance,
            major_version_t _major = DEFAULT_MAJOR, minor_version_t _minor = DEFAULT_MINOR) const = 0;

    // Send a message
    /**
     *
     * \brief This call sends a prepared message.
     *
     * In case of a request, a session ID is generated by setting the client ID and a request ID is generated according
     * to the SOME/IP specification.
     *
     * \param _message Prepared message that is to be sent.
     * \param _flush TODO: What exactly does this parameter mean for the implementation?
     *
     */
    virtual void send(std::shared_ptr<message> _message, bool _flush = true) = 0;

    /**
     *
     * \brief Set a field or fire an event.
     *
     * Prior to using this method, @ref offer_event has to be called by the service provider.
     *
     * \param _service Service ID of the service the event belongs to.
     * \param _instance Instance ID of the service the event belongs to.
     * \param _event Event ID of the event/field to be sent.
     * \param _payload Serialized payload of the event
     *
     */
    virtual void notify(service_t _service, instance_t _instance,
                event_t _event, std::shared_ptr<payload> _payload) const = 0;

    /**
     *
     * \brief Set a field or fire an event to a particular client.
     *
     * Prior to using this method, @ref offer_event has to be called by the service provider.
     *
     * \param _service Service ID of the service the event belongs to.
     * \param _instance Instance ID of the service the event belongs to.
     * \param _event Event ID of the event/field to be sent.
     * \param _payload Serialized payload of the event
     * \param _client Client the notification shall be sent to
     *
     */
    virtual void notify_one(service_t _service, instance_t _instance,
                event_t _event, std::shared_ptr<payload> _payload,
                client_t _client) const = 0;

    // [Un]Register handler for state change events
    /**
     *
     * \brief Register a state handler with the vsomeip runtime.
     *
     * The state handler tells if this client is successfully [de]registered to the central vsomeip
     * application. This is called during the @ref start and @ref stop methods of this class to signal the
     * availability of the vsomeip service.
     *
     * \param _handler Handler function to be called on state change.
     *
     */
    virtual void register_state_handler(state_handler_t _handler) = 0;

    /**
     *
     * \brief Remove a registered state handler.
     *
     */
    virtual void unregister_state_handler() = 0;

    // [Un]Register message handler for a method/an event/field
    /**
     *
     * \brief Registers a handler for the specified service method/event notification.
     *
     * This method allows to specify a callback for all messages aimed at the specified service method/event notification.
     * It is possible to specify wildcard values for all three ID arguments. It is not possible to register more than
     * one handler per combination. However, it is possible for multiple handlers to be called it more than one handler
     * fits to a certain message.
     *
     * \param _service Service ID of the service whose messages are to be handled. Can be set to ANY_SERVICE.
     * \param _instance Instance ID of the service instance whose messages are to be handled. Can be set to ANY_INSTANCE.
     * \param _method Method/Event ID of the method/event whose messages are to be handled. Can be set to ANY_METHOD.
     * \param _handler Callback that is to be called if a message arrives that matches the filter parameters.
     *
     */
    virtual void register_message_handler(service_t _service,
            instance_t _instance, method_t _method,
            message_handler_t _handler) = 0;
    /**
     *
     * \brief Removes the message handler for the specified service method/event notification.
     *
     */
    virtual void unregister_message_handler(service_t _service,
            instance_t _instance, method_t _method) = 0;

    // [Un]Register handler for availability reporting
    /**
     *
     * \brief Register a callback that is notified if service instances become available/become unavailable.
     *
     * This method allows for the registration of callbacks that are called whenever a service apeears or disappears.
     * It is possible to specify wildcards for instance and/or for the version. If a wildcard is specified for the instance,
     * the version must either be DEFAULT_MAJOR/DEFAULT_MINOR or ANY_MAJOR/ANY_MINOR. If a specific instance is given,
     * then version must be either DEFAULT_MAJOR/DEFAULT_MINOR or a specific service version.
     *
     * Only God knows why :)
     *
     * \param _service Service ID whose availability has to be reported.
     * \param _instance Instance ID whose availability has to be reported. Can be ANY_INSTANCE.
     * \param _handler Callback to be called if availability changes.
     * \param _major Major service version. Can be DEFAULT_MAJOR or ANY_MAJOR.
     * \param _minor Minor service version. Can be DEFAULT_MINOR or ANY_MINOR.
     *
     */
    virtual void register_availability_handler(service_t _service,
            instance_t _instance, availability_handler_t _handler,
            major_version_t _major = DEFAULT_MAJOR, minor_version_t _minor = DEFAULT_MINOR) = 0;

    /**
     *
     * \brief Removes a registered availability callback.
     *
     * \param _service Service ID whose availability handler shall be removed.
     * \param _instance Instance ID whose availability handler shall be removed. Can be ANY_INSTANCE.
     * \param _major Major service version. Can be DEFAULT_MAJOR or ANY_MAJOR.
     * \param _minor Minor service version. Can be DEFAULT_MINOR or ANY_MINOR.
     *
     */
    virtual void unregister_availability_handler(service_t _service,
            instance_t _instance,
            major_version_t _major = DEFAULT_MAJOR, minor_version_t _minor = DEFAULT_MINOR) = 0;

    // [Un]Register handler for subscriptions
    /**
     *
     * \brief Allows for the registration of a subscription handler.
     *
     * A subscription handler is called whenever the subscription state of a certain event group changes. The callback
     * is then called with the respective client ID and a boolean that indicates whether the client subscribed
     * or unsubscribed. For this callback, no wildcards are accepted.
     *
     * \param _service Service ID of service whose subscription state is to be monitored.
     * \param _instance Instance ID of service instance whose subscription state is to be monitored.
     * \param _eventgroup Event group ID of event group whose subscription state is to be monitored.
     * \param _handler Callback that shall be called.
     *
     */
    virtual void register_subscription_handler(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            subscription_handler_t _handler) = 0;

    /**
     *
     * \brief Removes a registered subscription state callback.
     *
     * \param _service Service ID of service whose subscription state callback is to be removed.
     * \param _instance Instance ID of service instance whose subscription state callback is to be removed.
     * \param _eventgroup Event group ID of event group whose subscription state callback is to be removed.
     *
     */
    virtual void unregister_subscription_handler(service_t _service,
                instance_t _instance, eventgroup_t _eventgroup) = 0;

    // [Un]Register handler for subscription errors
    /**
     *
     * \brief Allows for the registration of a subscription error handler.
     *
     * This handler is called whenever a subscription request for a certain event group was either accepted or rejected.
     * The respective callback is then called with ether 0x00 (OK) or 0x07 (Rejected).
     *
     * \param _service Service ID of service whose subscription error is to be monitored.
     * \param _instance Instance ID of service instance whose subscription error is to be monitored.
     * \param _eventgroup Event group ID of event group whose subscription error is to be monitored.
     * \param _handler Callback that shall be called.
     *
     */
    virtual void register_subscription_error_handler(service_t _service,
            instance_t _instance, eventgroup_t _eventgroup,
            error_handler_t _handler) = 0;

    /**
     *
     * \brief Removes a registered subscription error callback.
     *
     * \param _service Service ID of service whose subscription error callback is to be removed.
     * \param _instance Instance ID of service instance whose subscription error callback is to be removed.
     * \param _eventgroup Event group ID of event group whose subscription error callback is to be removed.
     *
     */
    virtual void unregister_subscription_error_handler(service_t _service,
                instance_t _instance, eventgroup_t _eventgroup) = 0;

    /**
     *
     * \brief Removes all registered callbacks.
     *
     */
    virtual void clear_all_handler() = 0;

    /**
     *
     * \brief This method tells whether this application is the central routing and service discovery hub in the system.
     *
     * In this case, the application also instantiates the classes routing_manager_impl and service_discovery_impl and
     * all associated classes and maintains the central routing socket.
     *
     * \return true if this is the central routing instance.
     *
     */
    virtual bool is_routing() const = 0;

    /**
     *
     * \brief Offers the given event to clients of the given service.
     *
     * This overload of offer_event offers some additional functionalities:
     * - It is possible to configure a cycle time. The notification message of this event is then resent cyclically.
     *
     *   The parameter _change_resets_cycle is available to control how event notification works in case the data is updated
     *   by the application. If set to true, an update of the data immediately leads to a notification. Otherwise, the updated data
     *   is sent only after the expiration of the cycle time.
     * - It is possible to specify a difference predicate.
     *
     *   Data is then only sent/updated if this predicate considers the data as different, unless the force option is
     *   set in the @ref notify call.
     *
     * \param _service Service ID of the service interface the event is part of.
     * \param _instance Instance ID of the service instance the event is part of.
     * \param _event Event ID of the offered event.
     * \param _eventgroups List of event groups the event is part of.
     * \param _is_field Tells whether this is an event or a field.
     * \param _cycle Sets the cycle time of the event. If nonzero, data is resent cyclically after the cycle time expired. May be std::chrono::milliseconds::zero().
     * \param _change_resets_cycle Tells if a change immediately leads to a notification.
     * \param _epsilon_change_func Predicate that determines if two given payloads are considered different.
     *
     */
    virtual void offer_event(service_t _service,
            instance_t _instance, event_t _event,
            const std::set<eventgroup_t> &_eventgroups,
            bool _is_field,
            std::chrono::milliseconds _cycle,
            bool _change_resets_cycle,
            const epsilon_change_func_t &_epsilon_change_func) = 0;

    /**
     *
     * \brief Set a field or fire an event with the ability to force notification for field
     *
     * Prior to using this method, @ref offer_event has to be called by the service provider.
     *
     * \param _service Service ID of the service the event belongs to.
     * \param _instance Instance ID of the service the event belongs to.
     * \param _event Event ID of the event/field to be sent.
     * \param _payload Serialized payload of the event
     * \param _force Forces the notification to be sent even if the difference between the old and the new value is below the threshold function given in @offer_event
     *
     */
    virtual void notify(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload,
            bool _force) const = 0;

    /**
     *
     * \brief Set a field or fire an event to a particular client with the ability to force notification for field
     *
     * Prior to using this method, @ref offer_event has to be called by the service provider.
     *
     * \param _service Service ID of the service the event belongs to.
     * \param _instance Instance ID of the service the event belongs to.
     * \param _event Event ID of the event/field to be sent.
     * \param _payload Serialized payload of the event
     * \param _client Client the notification shall be sent to
     * \param _force Forces the notification to be sent even if the difference between the old and the new value is below the threshold function given in @offer_event
     *
     */
    virtual void notify_one(service_t _service, instance_t _instance,
                event_t _event, std::shared_ptr<payload> _payload,
                client_t _client, bool _force) const = 0;

    typedef std::map<service_t, std::map<instance_t,  std::map<major_version_t, minor_version_t >>> available_t;
    virtual bool are_available(available_t &_available,
                       service_t _service = ANY_SERVICE, instance_t _instance = ANY_INSTANCE,
                       major_version_t _major = ANY_MAJOR, minor_version_t _minor = ANY_MINOR) const = 0;

    virtual void notify(service_t _service, instance_t _instance,
            event_t _event, std::shared_ptr<payload> _payload,
            bool _force, bool _flush) const = 0;

    virtual void notify_one(service_t _service, instance_t _instance,
                event_t _event, std::shared_ptr<payload> _payload,
                client_t _client, bool _force, bool _flush) const = 0;

    virtual  void set_routing_state(routing_state_e _routing_state) = 0;
};

/** @} */

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_HPP
