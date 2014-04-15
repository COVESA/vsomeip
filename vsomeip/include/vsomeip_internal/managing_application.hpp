//
// managing_application.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������������������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#ifndef VSOMEIP_INTERNAL_MANAGING_APPLICATION_HPP
#define VSOMEIP_INTERNAL_MANAGING_APPLICATION_HPP

#include <boost/asio/io_service.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>

#include <vsomeip/application.hpp>

namespace vsomeip {

class endpoint;

class managing_application
	: public application {
public:
	virtual ~managing_application() {};

	virtual boost::asio::io_service & get_service() = 0;
	virtual boost::log::sources::severity_logger<
				boost::log::trivial::severity_level > & get_logger() = 0;
	virtual void receive(const uint8_t *_data, uint32_t _size,
						 const endpoint *_source, const endpoint *_target) = 0;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_MANAGING_APPLICATION_HPP
