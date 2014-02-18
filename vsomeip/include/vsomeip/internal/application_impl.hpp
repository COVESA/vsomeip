/*
 * application_impl.hpp
 *
 *  Created on: Feb 18, 2014
 *      Author: someip
 */

#ifndef VSOMEIP_APPLICATION_IMPL_HPP
#define VSOMEIP_APPLICATION_IMPL_HPP

#include <boost/asio/io_service.hpp>

#include <vsomeip/application.hpp>

namespace vsomeip {

class application_impl: public application {
public:
	virtual ~application_impl();

	client * create_client(const endpoint *_target);
	service * create_service(const endpoint *_source);

	std::size_t poll_one();
	std::size_t poll();
	std::size_t run();

private:
	boost::asio::io_service is_;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_IMPL_HPP
