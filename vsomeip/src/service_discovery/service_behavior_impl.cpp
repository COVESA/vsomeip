//
// service_behavior_impl.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/back/state_machine.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <vsomeip/service_discovery/internal/service_behavior_impl.hpp>
#include <vsomeip/service_discovery/internal/events.hpp>
#include <vsomeip/service_discovery/internal/timer_service.hpp>

namespace vsomeip {
namespace service_discovery {

namespace msm = boost::msm;
namespace mpl = boost::mpl;
using namespace boost::msm::front;

////////////////////////////////////////////////////////
/////////// Implementation of statechart       /////////
////////////////////////////////////////////////////////

// Top-level state machine
struct service_behavior_def
		: public msm::front::state_machine_def<	service_behavior_def>,
		  public timer_service {

	struct service_behavior_impl *owner_;

	uint32_t run_, max_run_;
	int initial_min_, initial_max_;

	void init(uint32_t _max_run, int _initial_min, int _initial_max) {
		run_ = 0;
		max_run_ = _max_run;
		initial_min_ = _initial_min;
		initial_max_ = _initial_max;
	}

	void timer_expired(const boost::system::error_code &_error) {
		if (!_error)
			owner_->process_event(ev_timer_expired());
	}

	int get_random(int _min, int _max) {
		boost::random::mt19937 gen;
		boost::random::uniform_int_distribution<> dist(_min, _max);
		return dist(gen);
	}

	// guards
	template<class T>
	bool is_ready(T const &_event) {
		return true;
	}

	template<class T>
	bool is_not_ready(T const &_event) {
		return false;
	}

	// actions
	template<class T>
	void send_offer_service(T const &_event) {
		owner_->offer_service();
	}

	template<class T>
	void send_stop_offer_service(T const &_event) {
		owner_->stop_offer_service();
	}

	// states
	struct not_ready
		: public msm::front::state<> {
	};

	struct ready_def
		: public msm::front::state_machine_def<ready_def> {

		struct service_behavior_def *owner_;

		void set_owner(struct service_behavior_def *_owner) {
			owner_ = _owner;
		}

		bool is_repeating(ev_timer_expired const &_event) {
			return (owner_->max_run_ > 0);
		}

		bool is_not_repeating(ev_timer_expired const  &_event) {
			return (owner_->max_run_ == 0);
		}

		// actions
		template<class T>
		void send_offer_service(T const &_event) {
			owner_->send_offer_service(_event);
		}

		template<class T>
		void wait_and_send_offer_service(T const &_event) {
			owner_->run_--;
		}

		template<class Event, class Fsm>
		void on_exit(Event const &, Fsm &_fsm) {
			owner_->stop_timer();
		}

		// states
		struct initializing
			: public msm::front::state<> {

			template<class Event, class Fsm>
			void on_entry(Event const &, Fsm &_fsm) {
				std::cout << "  initializing" << std::endl;
				_fsm.owner_->start_timer(
					_fsm.owner_->get_random(
									_fsm.owner_->initial_min_,
									_fsm.owner_->initial_max_));
			}
		};

		struct repeating
			: public msm::front::state<> {
			template<class Event, class Fsm>
			void on_entry(Event const &, Fsm &_fsm) {
				std::cout << "  repeating" << std::endl;
				_fsm.owner_->start_timer((2 << _fsm.owner_->run_++) * 1000); // TODO: replace 1000 with configured value!
			}
			template<class Event, class Fsm>
			void on_exit(Event const &, Fsm &_fsm) {
				_fsm.owner_->stop_timer();
			}
		};

		struct announcing
			: public msm::front::state<> {
			template<class Event, class Fsm>
			void on_entry(Event const &, Fsm &_fsm) {
				std::cout << "  announcing" << std::endl;
				_fsm.owner_->start_timer(20000); // TODO: replace 20000 with configured value!
			}
			template<class Event, class Fsm>
			void on_exit(Event const &, Fsm &_fsm) {
				_fsm.owner_->stop_timer();
			}
		};

		struct transition_table : mpl::vector<
			  row< initializing, ev_timer_expired, repeating,
			       &ready_def::send_offer_service,
			       &ready_def::is_repeating>,
			  row< initializing, ev_timer_expired, announcing,
				   &ready_def::send_offer_service,
				   &ready_def::is_not_repeating>,
			a_row< repeating, ev_find_service, repeating,
			       &ready_def::wait_and_send_offer_service >,
			  row< repeating, ev_timer_expired, repeating,
			  	   &ready_def::send_offer_service,
			  	   &ready_def::is_repeating >,
			 row< repeating, ev_timer_expired, announcing,
				   &ready_def::send_offer_service,
				   &ready_def::is_not_repeating >,
			a_row< announcing, ev_find_service, announcing,
			       &ready_def::wait_and_send_offer_service >,
			a_row< announcing, ev_timer_expired, announcing,
				   &ready_def::send_offer_service >
		> {};

		typedef initializing initial_state;

		template <class Fsm, class Event>
		void no_transition(Event const &_event, Fsm &_machine, int state) {
			// TODO: log "Received illegal event!"
		}
	};

	// Use back end to make the inner state machine work
	typedef msm::back::state_machine<ready_def> ready;

	struct transition_table : mpl::vector<
		g_row< not_ready, none, ready,
		       &service_behavior_def::is_ready >,
		g_row< not_ready, ev_daemon_status_change, ready,
		       &service_behavior_def::is_ready >,
		g_row< ready, ev_daemon_status_change, not_ready,
		  	   &service_behavior_def::is_not_ready >,
		  row< ready, ev_service_status_change, not_ready,
		  	   &service_behavior_def::send_stop_offer_service,
		  	   &service_behavior_def::is_not_ready >
	> {};

	typedef not_ready initial_state;

	template <class Fsm, class Event>
	void no_transition(Event const &_event, Fsm &_machine, int state) {
		// TODO: log "Received illegal event!"
	}
};

////////////////////////////////////////////////////////
/////////// Implementation of members          /////////
////////////////////////////////////////////////////////
struct service_behavior_impl::state_machine
		: msm::back::state_machine< service_behavior_def > {
};

////////////////////////////////////////////////////////
/////////// Implementation of member functions /////////
////////////////////////////////////////////////////////
service_behavior_impl::service_behavior_impl(boost::asio::io_service& _is)
	: state_machine_(new state_machine) {

	state_machine::ready& ready_state
		= state_machine_->get_state<state_machine::ready&>();
	ready_state.set_owner(state_machine_.get());

	state_machine_->init(3, 1, 10);
	state_machine_->init_timer(_is);
	state_machine_->owner_ = this;
}

void service_behavior_impl::start() {
	state_machine_->start();
}

void service_behavior_impl::stop() {
	state_machine_->stop();
}

template<class Event>
void service_behavior_impl::process_event(const Event &e) {
	state_machine_->process_event(e);
}

void service_behavior_impl::process_event(event_variant e)
{
	boost::apply_visitor(ProcessEvent<service_behavior_impl>(*this), e);
}

} // namespace service_discovery
} // namespace vsomeip
