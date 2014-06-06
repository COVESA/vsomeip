//
// factory_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_FACTORY_IMPL_HPP
#define VSOMEIP_INTERNAL_FACTORY_IMPL_HPP

#include <map>
#include <string>

#include <vsomeip/factory.hpp>

namespace vsomeip {

class factory_impl : public factory {
public:
	virtual ~factory_impl();

	static factory * get_instance();

	application * create_application(const std::string &_name) const;

	endpoint * get_endpoint(ip_address _address, ip_port _port, ip_protocol _protocol);
	endpoint * get_endpoint(const uint8_t *_bytes, uint32_t _size);

	message * create_message() const;
	message * create_response(const message *_request) const;

	field * create_field(application *_application, service_id _service, instance_id _instance, event_id _event) const;

private:
	std::map< uint32_t, std::map< std::string, endpoint * > > endpoints_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_FACTORY_IMPL_HPP
