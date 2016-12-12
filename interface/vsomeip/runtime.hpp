// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_RUNTIME_HPP
#define VSOMEIP_RUNTIME_HPP

#include <memory>
#include <string>
#include <vector>

#include <vsomeip/export.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class application;
class message;
class payload;

/**
 *
 * \defgroup Runtime
 *
 * The Runtime module contains the public class factory API of VSOMEIP. Its main singleton class is called @ref runtime.
 *
 * @{
 *
 */

/**
 *
 * \brief Singleton class containing all public resource management facilities of VSOMEIP.
 *
 * The methods of this class shall be used to create instances of all the classes needed to facilitate SOMEIP communication.
 * In particular, it is the entry point to create instances of the @ref application class that contains the main public
 * API of the VSOMEIP subsystem.
 *
 */
class VSOMEIP_IMPORT_EXPORT runtime {
public:

    static std::string get_property(const std::string &_name);
    static void set_property(const std::string &_name, const std::string &_value);

    static std::shared_ptr<runtime> get();

    virtual ~runtime() {
    }

    /**
     *
     * \brief Creates a vsomeip client.
     *
     * An "application" manages all service offers and event subscriptions as well as all event callbacks and has to be
     * used to send and receive data. It is identified with a unique name that is also used in (and therefore has to match)
     * the configuration files of vsomeip. If the name is left empty, the application name is taken from the environment
     * variable "VSOMEIP_APPLICATION_NAME"
     *
     * \param _name Name of the applicatoin on the system.
     *
     */
    virtual std::shared_ptr<application> create_application(
            const std::string &_name = "") = 0;

    /**
     *
     * \brief Constructs an empty message object.
     *
     * The message can then be used in the @application::send and @application::notify calls to send data to clients.
     * The message is typeless after this call.
     *
     * \param _reliable Determines whether this message shall be sent over a reliable connection.
     *
     */
    virtual std::shared_ptr<message> create_message(
            bool _reliable = false) const = 0;
    /**
     *
     * \brief Constructs an empty message whose type is set to "request" (i.e. an async method call).
     *
     * This message can then be sent via @application::send. It is not necessary to manage the request ID as this is
     * done by vsomeip.
     *
     * \param _reliable Determines whether this message shall be sent over a reliable connection.
     *
     */
    virtual std::shared_ptr<message> create_request(
            bool _reliable = false) const = 0;
    /**
     *
     * \brief Creates a response to a given request.
     *
     * This message is prefilled with all information so that the remote end can match it to its list of pending changes.
     * After filling the payload, the message can then be sent via @application::send.
     *
     * \param _reliable Determines whether this message shall be sent over a reliable connection.
     *
     */
    virtual std::shared_ptr<message> create_response(
            const std::shared_ptr<message> &_request) const = 0;

    /**
     *
     * \brief Creates a message of type 'notification'.
     *
     * This message can then be sent via @application::notify or @application::notify_one.
     *
     * \param _reliable Determines whether this message shall be sent over a reliable connection.
     *
     */
    virtual std::shared_ptr<message> create_notification(
            bool _reliable = false) const = 0;

    /**
     *
     * \brief Creates an empty payload object.
     *
     */
    virtual std::shared_ptr<payload> create_payload() const = 0;

    /**
     *
     * \brief Creates a payload object filled with the given data.
     *
     * \param _data Bytes to be copied into the payload object.
     * \param _size Number of bytes to be copied into the payload object.
     *
     */
    virtual std::shared_ptr<payload> create_payload(
            const byte_t *_data, uint32_t _size) const = 0;

    /**
     *
     * \brief Creates a payload object filled with the given data.
     *
     * \param _data Bytes to be copied into the payload object.
     *
     */
    virtual std::shared_ptr<payload> create_payload(
            const std::vector<byte_t> &_data) const = 0;

    /**
     *
     * \brief Retrieves the application object for the application with the given name.
     *
     * If no such application is found, an empty shared_ptr is returned (nullptr).
     *
     * \param _name Name of the application to be found.
     *
     */
    virtual std::shared_ptr<application> get_application(
            const std::string &_name) const = 0;

    virtual void remove_application( const std::string &_name) = 0;
};

/** @} */

} // namespace vsomeip

#endif // VSOMEIP_RUNTIME_HPP
