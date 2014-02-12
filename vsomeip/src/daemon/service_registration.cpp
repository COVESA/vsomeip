//
// state_machine.cpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/back/state_machine.hpp>

#include <vsomeip/service_discovery/internal/events.hpp>
#include <vsomeip/service_discovery/internal/service_registration.hpp>

namespace vsomeip {
namespace service_discovery {

namespace msm = boost::msm;
namespace mpl = boost::mpl;
using namespace boost::msm::front;

////////////////////////////////////////////////////////
/////////// Implementation of statechart       /////////
////////////////////////////////////////////////////////

// Top-level state machine
struct service_registration_state_machine_def
	: public msm::front::state_machine_def<service_registration_state_machine_def> {

	// States
	struct initial: public msm::front::entry_pseudo_state<0> {
		template <class Event, class Fsm>
					void on_entry(Event const &, Fsm &fsm)
					{ std::cout << "Entering Initial" << std::endl; }
		template <class Event, class Fsm>
					void on_exit(Event const &, Fsm &fsm)
					{ std::cout << "Exiting Initial" << std::endl; }
	};
	struct not_ready: public msm::front::state<> {
		template <class Event, class Fsm>
					void on_entry(Event const &, Fsm &fsm)
					{ std::cout << "Entering NotReady" << std::endl; }
		template <class Event, class Fsm>
					void on_exit(Event const &, Fsm &fsm)
					{ std::cout << "Exiting NotReady" << std::endl; }
	};

	struct ready: public msm::front::state_machine_def<ready> {
		template <class Event, class Fsm>
					void on_entry(Event const &, Fsm &fsm)
					{ std::cout << "Entering Ready" << std::endl; }
		template <class Event, class Fsm>
					void on_exit(Event const &, Fsm &fsm)
					{ std::cout << "Exiting Ready" << std::endl; }

		// States
		struct initial: public msm::front::entry_pseudo_state<0> {
		};
		struct waiting: public msm::front::state<> {
		};
		struct repeating: public msm::front::state<> {
		};
		struct announcing: public msm::front::state<> {
		};

		// Determine initial state
		typedef initial initial_state;

		// Members
		uint32_t repetitions_max_;

		// Guards
		bool is_repeating(ev_timer_expired const &_event) {
			return repetitions_max_ > 0;
		}

		bool is_not_repeating(ev_timer_expired const &_event) {
			return repetitions_max_ == 0;
		}

		// Actions
		void send_offer_service(ev_timer_expired const &_event) {
			// TODO: send message
		}

		void send_delayed_offer_service(ev_find_service const &_event) {
			// TODO: check timing and schedule sending of message
		}

		// Transitions
		struct transition_table: mpl::vector<
				row<waiting, ev_timer_expired, announcing,
						&ready::send_offer_service, &ready::is_not_repeating>,
				row<waiting, ev_timer_expired, repeating,
						&ready::send_offer_service, &ready::is_repeating>,
				a_row<repeating, ev_find_service, repeating,
						&ready::send_delayed_offer_service>,
				row<repeating, ev_timer_expired, repeating,
						&ready::send_offer_service, &ready::is_repeating>,
				g_row<repeating, ev_timer_expired, announcing,
						&ready::is_not_repeating>,
				a_row<announcing, ev_timer_expired, announcing,
						&ready::send_offer_service>,
				a_row<announcing, ev_find_service, announcing,
						&ready::send_delayed_offer_service> > {
		};
	};

	// Determine initial state
	typedef initial initial_state;

	// Members
	bool is_daemon_up_;
	bool is_service_up_;
	bool is_requested_;

	// Guards
	bool is_not_ready(none const &_no_event) {
		return (!is_daemon_up_ || !is_service_up_);
	}

	bool is_ready(none const &_no_event) {
		return !(is_not_ready(_no_event));
	}

	bool is_not_ready(ev_service_status_change const &_event) {
		is_service_up_ = _event.is_up_;
		return (!is_daemon_up_ || !is_service_up_);
	}

	bool is_ready(ev_service_status_change const &_event) {
		return !(is_not_ready(_event));
	}

	bool is_not_ready(ev_daemon_status_change const &_event) {
		is_daemon_up_ = _event.is_up_;
		return (!is_daemon_up_ || !is_service_up_);
	}

	bool is_ready(ev_daemon_status_change const &_event) {
		return !(is_not_ready(_event));
	}

	// Actions
	void handle_service_down(ev_daemon_status_change const &_event) {
		// TODO: clear_all_timers();
	}
	;

	void handle_service_down(ev_service_status_change const &_event) {
		if (!is_service_up_) {
			// TODO: send "StopOfferService" message!
		}

		// TODO: clear_all_timers();
	};

	// Transitions
	struct transition_table: mpl::vector<
			g_row<initial, none, ready,
					&service_registration_state_machine_def::is_ready>,
			g_row<initial, none, not_ready,
					&service_registration_state_machine_def::is_not_ready>,
			g_row<not_ready, ev_daemon_status_change, ready,
					&service_registration_state_machine_def::is_ready>,
			g_row<not_ready, ev_service_status_change, ready,
					&service_registration_state_machine_def::is_ready>,
			row<ready, ev_daemon_status_change, not_ready,
					&service_registration_state_machine_def::handle_service_down,
					&service_registration_state_machine_def::is_not_ready>,
			row<ready, ev_service_status_change, not_ready,
					&service_registration_state_machine_def::handle_service_down,
					&service_registration_state_machine_def::is_not_ready> > {
	};

	template <class Fsm, class Event>
	void no_transition(Event const &_event, Fsm &_machine, int state) {
		std::cout << "No transition from state " << state << " on event " << typeid(_event).name() << std::endl;
	}
};

struct service_registration::state_machine
		: msm::back::state_machine< service_registration_state_machine_def > {
};

////////////////////////////////////////////////////////
/////////// Implementation of member functions /////////
////////////////////////////////////////////////////////
service_registration::service_registration()
	: state_machine_(new state_machine) {
	state_machine_->is_daemon_up_ = false;
	state_machine_->is_service_up_ = false;
}

void service_registration::start() {
	state_machine_->start();
}

void service_registration::stop() {
	state_machine_->stop();
}

void service_registration::process() {
	state_machine_->process_event(none());
}

} // namespace service_discovery
} // namespace vsomeip

