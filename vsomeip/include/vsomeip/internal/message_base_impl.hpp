//
// message_base_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_MESSAGE_BASE_IMPL_HPP
#define VSOMEIP_INTERNAL_MESSAGE_BASE_IMPL_HPP

#include <boost/thread.hpp>

#include <vsomeip/message.hpp>
#include <vsomeip/internal/message_header_impl.hpp>

namespace vsomeip {

class message_base_impl : virtual public message_base {
public:
	message_base_impl();
	virtual ~message_base_impl();

	virtual endpoint * get_endpoint() const;
	virtual void set_endpoint(endpoint *_endpoint);

	virtual message_id get_message_id() const;
	virtual void set_message_id(message_id _id);
	virtual service_id get_service_id() const;
	virtual void set_service_id(service_id _id);
	virtual method_id get_method_id() const;
	virtual void set_method_id(method_id _id);
	virtual request_id get_request_id() const;
	virtual void set_request_id(request_id _id);
	virtual client_id get_client_id() const;
	virtual void set_client_id(client_id _id);
	virtual session_id get_session_id() const;
	virtual void set_session_id(session_id _id);
	virtual protocol_version get_protocol_version() const;
	virtual void set_protocol_version(protocol_version _version);
	virtual interface_version get_interface_version() const;
	virtual void set_interface_version(interface_version _version);
	virtual message_type get_message_type() const;
	virtual void set_message_type(message_type _type);
	virtual return_code get_return_code() const;
	virtual void set_return_code(return_code _code);

	message * get_owner() const;
	void set_owner(message *_owner);

protected: // members
	endpoint *endpoint_;
	message_header_impl header_;

private:
	uint32_t message_id_;
	static uint32_t message_count__;
	static boost::mutex lock__;
	static uint32_t get_message_count();
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_MESSAGE_BASE_IMPL_HPP
