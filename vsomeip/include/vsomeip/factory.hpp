//
// factory.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_FACTORY_HPP
#define VSOMEIP_FACTORY_HPP

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class message;

class serializer;
class deserializer;

class endpoint;
class client;
class service;

/// The factory class is the main interaction point between vsomeip library
/// and applications that use it. It implements a singleton and provides
/// methods to create/get the main vsomeip-objects, such as clients, services,
/// messages and endpoints. Additionally it can be used to instantiate vsomeip
/// serializers and deserializers, although an application usally does not
/// need to create these on its own as client- and service-instantiation
/// implicitely does.
class factory {
public:
	/// Must be called at least once per application to get the pointer to the
	/// factory instance.
	/// \returns Pointer to factory object.
	/// \warning The returned pointer must not be deleted by the application.
	static factory * get_default_factory();

	/// Intentionally undocumented as it is never(!) called.
	virtual ~factory() {};

	/// Creates a new message object. The caller is responsible for the objects
	/// lifetime, especially for deleting it.
	/// \returns Pointer to new message object.
	virtual message * create_message() const = 0;

	/// Creates a new serializer object. The caller is responsible for the objects
	/// lifetime, especially for deleting it.
	/// \returns Pointer to new serializer object.
	virtual serializer * create_serializer() const = 0;

	/// Creates a new deserializer object. The caller is responsible for the objects
	/// lifetime, especially for deleting it.
	/// \returns Pointer to new deserializer object.
	virtual deserializer * create_deserializer() const = 0;

	/// Get a unique reference for the endpoint with the properties specified
	/// by the methods arguments. Two calls with the same argument are
	/// guaranteed to have the same result, thus pointer comparison can be used
	/// to test equality of two endpoints.
	/// \param _address the (IP) address of the requested endpoint
	/// \param _port the (IP) port of the requested endpoint
	/// \param _protocol the protocol used by the requested endpoint
	/// (currently supported protocols are UDP and TCP)
	/// \param _version the version of the protocol used by the requested
	/// endpoint (currently supported protocol versions are V4 and V6)
	/// \returns Pointer to the requested endpoint
	/// \warning The returned pointer must not be deleted by the application.
	virtual endpoint * get_endpoint(
							ip_address _address, ip_port _port,
							ip_protocol _protocol, ip_version _version) = 0;

	/// Creates a client object. An application needs to create one client
	/// object for each service endpoint it wants to communicate to.
	/// \params _endpoint the endpoint of the service an application wants to
	/// use
	/// \returns Pointer to the created client object
	virtual client * create_client(const endpoint *_endpoint) const = 0;

	/// Creates a service object. An application needs to create one service
	/// object for each endpoints it wants to use to provide a service. Thus,
	/// if an application wants to provide the same service on several
	/// endpoints, it must create and control several service objects.
	/// \params _endpoint the endpoint that the application wants to
	/// use to offer a service
	/// \returns Pointer to the created service object
	virtual service * create_service(const endpoint *_endpoint) const = 0;
};

}; // namespace vsomeip

#endif // VSOMEIP_FACTORY_HPP
