//
// provider_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_PROVIDER_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_PROVIDER_IMPL_HPP

#include <deque>
#include <map>
#include <vector>

#include <boost/variant.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <vsomeip/provider.hpp>
#include <vsomeip/internal/statistics_owner_impl.hpp>
#include <vsomeip/service_discovery/internal/service_info.hpp>


namespace vsomeip {
namespace service_discovery {

class provider_impl
	: virtual public provider
#ifdef USE_VSOMEIP_STATISTICS
	  , virtual public statistics_owner_impl
#endif
{
public:
	provider_impl(vsomeip::provider *_delegate, boost::asio::io_service &_is);
	virtual ~provider_impl();

	bool register_service(service_id _service, instance_id _instance);
	bool unregister_service(service_id _service, instance_id _instance);

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
	bool send(const uint8_t *_data, uint32_t _length, endpoint *_target, bool _flush);
	bool flush(endpoint *_target);

private:
	vsomeip::provider *delegate_;
	std::map< service_id, service_info > services_;
	std::deque<std::vector<uint8_t>> command_queue_;

	boost::asio::local::stream_protocol::socket socket_;

private:
	void connect();
	void send_command(const uint8_t *_command, uint32_t _size);
	void sent_command(boost::system::error_code const &_error, std::size_t _bytes);
	void announce(service_info &_info);
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_PROVIDER_IMPL_HPP

