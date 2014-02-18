//
// client_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_IMPL_HPP

#include <boost/variant.hpp>
#include <boost/asio/io_service.hpp>

#include <vsomeip/client.hpp>
#include <vsomeip/receiver.hpp>
#include <vsomeip/service_discovery/internal/events.hpp>
#include <vsomeip/internal/statistics_owner_impl.hpp>
#include <vsomeip/service_discovery/internal/client_behavior_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class client_impl
	: virtual public client,
	  virtual public client_behavior_impl,
	  virtual public receiver
#ifdef USE_VSOMEIP_STATISTICS
	  , virtual public statistics_owner_impl
#endif
{
public:
	client_impl(service_id _service_id, instance_id _instance_id,
			      boost::asio::io_service& _is);
	~client_impl();

	void start();
	void stop();

	void register_for(receiver *_receiver,
				 	 	service_id _service_id,
				 	 	method_id _method_id);

	void unregister_for(receiver *_receiver,
	 	 				  service_id _service_id,
	 	 				  method_id _method_id);

	void enable_magic_cookies();
	void disable_magic_cookies();

	bool send(const message_base *_message, bool _flush);
	bool send(const uint8_t *_data, uint32_t _length, bool _flush);
	bool flush();

	void receive(const message_base *_message);

protected:
	void find_service();

private:
	vsomeip::client *delegate_;
	vsomeip::client *service_discovery_client_;

	service_id service_id_;
	instance_id instance_id_;
	major_version major_version_;
	time_to_live time_to_live_;

	boost::asio::io_service& is_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_IMPL_HPP

