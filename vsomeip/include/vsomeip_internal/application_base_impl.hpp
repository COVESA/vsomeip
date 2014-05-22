//
// application_base_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_APPLICATION_BASE_IMPL_HPP
#define VSOMEIP_INTERNAL_APPLICATION_BASE_IMPL_HPP

#include <boost/asio/io_service.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip_internal/deserializer.hpp>
#include <vsomeip_internal/log_owner.hpp>
#include <vsomeip_internal/proxy_owner.hpp>
#include <vsomeip_internal/serializer.hpp>

namespace vsomeip {

class endpoint;
class message;

class application_base_impl :
			virtual public log_owner,
			virtual public proxy_owner {
public:
	application_base_impl(const std::string &_name);
	virtual ~application_base_impl();

	virtual client_id get_id() const = 0;
	virtual void set_id(client_id _id) = 0;

	virtual std::string get_name() const = 0;
	virtual void set_name(const std::string &_name) = 0;

	virtual bool is_managing() const = 0;

	virtual boost::asio::io_service & get_sender_service() = 0;
	virtual boost::asio::io_service & get_receiver_service() = 0;

	virtual boost::shared_ptr< serializer > & get_serializer() = 0;
	virtual boost::shared_ptr< deserializer > & get_deserializer() = 0;

	virtual void catch_up_registrations() = 0;
	virtual void handle_message(const message *_message) = 0;
	virtual void handle_service_availability(service_id _service, instance_id _instance, const endpoint *_location, bool _is_available) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_APPLICATION_BASE_IMPL_HPP
