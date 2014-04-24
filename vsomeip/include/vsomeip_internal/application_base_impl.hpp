//
// application_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_APPLICATION_BASE_IMPL_HPP
#define VSOMEIP_APPLICATION_BASE_IMPL_HPP

#include <boost/asio/io_service.hpp>

#include <vsomeip/application.hpp>
#include <vsomeip_internal/log_owner.hpp>

namespace vsomeip {

namespace sd {
	class client_manager;
} // namespace sd

class application_base_impl
		: public application,
		  public log_owner {
public:
	application_base_impl(const std::string &_name, boost::asio::io_service &_service);
	virtual ~application_base_impl();

	std::string get_name() const;

	void init(int _options_count, char **_options);
	void start();
	void stop();

	//void on_provide_service(service_id _service, instance_id _instance);
	//void on_withdraw_service(service_id _service, instance_id _instance);
	//void on_start_service(service_id _service, instance_id _instance);
	//void on_stop_service(service_id _service, instance_id _instance);

	//void on_request_service(service_id _service, instance_id _instance);
	//void on_release_service(service_id _service, instance_id _instance);

protected:
	sd::client_manager *client_manager_;

private:
	boost::asio::io_service &service_;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_BASE_IMPL_HPP
