//
// log_user.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/log_owner.hpp>
#include <vsomeip_internal/log_user.hpp>

namespace vsomeip {

log_user::log_user(log_owner &_owner)
	: logger_(_owner.logger_) {
}

log_user::log_user(log_user &_other)
	: logger_(_other.logger_) {
}

} // namespace vsomeip



