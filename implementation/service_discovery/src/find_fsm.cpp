// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/find_fsm.hpp"

namespace vsomeip {
namespace sd {

///////////////////////////////////////////////////////////////////////////////
// Machine
///////////////////////////////////////////////////////////////////////////////
namespace _find {

fsm::fsm(find_fsm *_fsm): fsm_(_fsm), fsm_base(_fsm->get_io()) {
}

fsm::~fsm() {
}

void fsm::timer_expired(const boost::system::error_code &_error) {
	if (!_error) {
		process_event(ev_timeout());
	}
}

inactive::inactive(my_context context): sc::state< inactive, fsm >(context) {
}

sc::result inactive::react(const ev_none &_event) {
	return discard_event();
}

sc::result inactive::react(const ev_status_change &_event) {
	return discard_event();
}

} // namespace _find

///////////////////////////////////////////////////////////////////////////////
// Interface
///////////////////////////////////////////////////////////////////////////////
find_fsm::find_fsm(boost::asio::io_service &_io)
	: io_(_io), fsm_(this) {
}

boost::asio::io_service & find_fsm::get_io() {
	return io_;
}

} // namespace sd
} // namespace vsomeip



