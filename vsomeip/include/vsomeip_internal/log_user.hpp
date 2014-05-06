//
// log_user.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_LOG_USER_HPP
#define VSOMEIP_INTERNAL_LOG_USER_HPP

#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>

namespace vsomeip {

class log_owner;

class log_user {
public:
	log_user(log_owner &_owner);
	log_user(log_user &_other);

protected:
	boost::log::sources::severity_logger<
		boost::log::trivial::severity_level > &logger_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_LOG_USER_HPP
