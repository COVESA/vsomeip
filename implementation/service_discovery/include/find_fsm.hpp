// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_FIND_FSM_HPP
#define VSOMEIP_SD_FIND_FSM_HPP

#include <boost/mpl/list.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/transition.hpp>

#include "../include/fsm_base.hpp"
#include "../include/fsm_events.hpp"

namespace vsomeip {
namespace sd {

class find_fsm;

///////////////////////////////////////////////////////////////////////////////
// Machine
///////////////////////////////////////////////////////////////////////////////
namespace mpl = boost::mpl;
namespace sc = boost::statechart;

namespace _find {

struct inactive;
struct fsm:
		sc::state_machine< fsm, inactive >, public fsm_base {

	fsm(find_fsm *_fsm);
	virtual ~fsm();

	void timer_expired(const boost::system::error_code &_error);

	find_fsm *fsm_;
};

struct inactive:
		sc::state< inactive, fsm > {

	inactive(my_context context);

	typedef mpl::list<
		sc::custom_reaction< ev_none >,
		sc::custom_reaction< ev_status_change >
	> reactions;

	sc::result react(const ev_none &_event);
	sc::result react(const ev_status_change &_event);
};

} // namespace _find

///////////////////////////////////////////////////////////////////////////////
// Interface
///////////////////////////////////////////////////////////////////////////////
class find_fsm {
public:
	find_fsm(boost::asio::io_service &_io);

	boost::asio::io_service & get_io();

private:
	boost::asio::io_service &io_;
	_find::fsm fsm_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_FIND_FSM_HPP
