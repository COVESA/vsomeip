//
// loggable.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_LOGGABLE_HPP
#define VSOMEIP_INTERNAL_LOGGABLE_HPP

#include <boost/log/trivial.hpp>
#include <boost/log/sources/severity_logger.hpp>

namespace vsomeip {

class loggable {
public:
	static void init();

protected:
	boost::log::sources::severity_logger< boost::log::trivial::severity_level > log_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_LOGGABLE_HPP
