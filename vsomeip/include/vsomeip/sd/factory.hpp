//
// factory.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SD_FACTORY_HPP
#define VSOMEIP_SD_FACTORY_HPP

#include <boost/asio/io_service.hpp>

#define VSOMEIP_SD_LIBRARY					"libvsomeip-sd.so"
#define VSOMEIP_SD_FACTORY_SYMBOL 			VSOMEIP_SD_FACTORY
#define VSOMEIP_SD_FACTORY_SYMBOL_STRING 	"VSOMEIP_SD_FACTORY"

namespace vsomeip {
namespace sd {

class message;
class service_discovery;

class factory {
public:
	virtual ~factory() {};

	static factory * get_instance();

	virtual service_discovery * create_service_discovery(
				boost::asio::io_service &_service) const = 0;

	virtual message * create_message() const = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_FACTORY_HPP
