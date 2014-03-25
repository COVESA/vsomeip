//
// message_base.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_MESSAGE_BASE_HPP
#define VSOMEIP_MESSAGE_BASE_HPP

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/serializable.hpp>
#include <vsomeip/deserializable.hpp>

namespace vsomeip {

class endpoint;
/// Common base interface for application and service discovery messages. The
/// interface provides set- and get-methods for all elements of a Some/IP 
/// message header and for the endpoint a message is sent to or received from.
class message_base : virtual public serializable, virtual public deserializable {
public:
	virtual ~message_base() {};

	virtual application_id get_sender_id() const = 0;
	virtual void set_sender_id(const application_id _id) = 0;

    virtual const endpoint * get_source() const = 0;
    virtual void set_source(const endpoint *_source) = 0;
    virtual const endpoint * get_target() const = 0;
    virtual void set_target(const endpoint *_target) = 0;

    /// Returns the message identifier of the Some/IP message, which is 
    /// composed from the service identifier and the method identifier.
    /// \returns message identifier of the Some/IP message
	virtual message_id get_message_id() const = 0;
    
    /// Sets service identifier and method identifier. The message identifier
    /// itself is not stored.
    /// \param _id New message identifier
	virtual void set_message_id(message_id _id) = 0;
    
    /// Returns the service identifier of the Some/IP message.
    /// \returns service identifier of the Some/IP message 
	virtual service_id get_service_id() const = 0;
    
    /// Sets the service identifier within the Some/IP message header.
    /// \param _id New service identifier
	virtual void set_service_id(service_id _id) = 0;

    /// Returns the method identifier of the Some/IP message.
    /// \returns method identifier of the Some/IP message 	
    virtual method_id get_method_id() const = 0;

    /// Sets the method identifier within the Some/IP message header.
    /// \param _id New method identifier
	virtual void set_method_id(method_id _id) = 0;
    
    /// Returns the length of the Some/IP message. The returned value is the 
    /// length of the payload plus the static header part (request identifier
    /// and flags).
    /// \returns length of the message
	virtual length get_length() const = 0;
    
    /// Returns the message identifier of the Some/IP message, which is 
    /// composed from the client identifier and the session identifier.
    /// \returns message identifier of the Some/IP message 
    virtual request_id get_request_id() const = 0;
 
    /// Sets client identifier and session identifier. The request identifier
    /// itself is not stored. 
	/// \param _id New request identifier
    virtual void set_request_id(request_id _id) = 0;
    
    /// Returns the client identifier of the Some/IP message.
    /// \returns client identifier of the Some/IP message 
	virtual client_id get_client_id() const = 0;
	
    /// Sets the client identifier within the Some/IP message header.
    /// \param _id New client identifier
	virtual void set_client_id(client_id _id) = 0;
	
    /// Returns the session identifier of the Some/IP message.
    /// \returns session identifier of the Some/IP message 
    virtual session_id get_session_id() const = 0;

    /// Sets the session identifier of a message object.
   /// \param _id New session identifier
	virtual void set_session_id(session_id _id) = 0;
	
    /// Returns the protocol version of the Some/IP message. Defaults to 0x1.
    /// \returns protocol version of the Some/IP message     
    virtual protocol_version get_protocol_version() const = 0;
	
    /// Sets the protocol version within the Some/IP message header.
    /// \param _id New protocol version
    /// \warning There is currently no need to call this method from an
    /// application.
    virtual void set_protocol_version(protocol_version _version) = 0;

    /// Returns the interface version of the Some/IP message header. 
    /// \returns interface version of the Some/IP message         
	virtual interface_version get_interface_version() const = 0;
    
    /// Sets the interface version within the Some/IP message header.
    /// \param _id New interface version
	virtual void set_interface_version(interface_version _version) = 0;

    /// Returns the method type of the Some/IP message header. 
    /// \returns message type of the Some/IP message             
	virtual message_type_enum get_message_type() const = 0;

    /// Sets the message type within the Some/IP message header.
    /// \param _id New message type
	virtual void set_message_type(message_type_enum _type) = 0;

    /// Returns the return code of the Some/IP message header. 
    /// \returns return code of the Some/IP message             
	virtual return_code_enum get_return_code() const = 0;

    /// Sets the return code within the Some/IP message header.
    /// \param _id New return code
	virtual void set_return_code(return_code_enum _code) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_MESSAGE_BASE_HPP
