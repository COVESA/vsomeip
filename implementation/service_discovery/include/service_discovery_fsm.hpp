// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_SERVICE_DISCOVERY_FSM_HPP
#define VSOMEIP_SD_SERVICE_DISCOVERY_FSM_HPP

#include <mutex>
#include <string>

#include <boost/mpl/list.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/transition.hpp>

#include "../include/fsm_base.hpp"
#include "../include/fsm_events.hpp"

namespace vsomeip {
namespace sd {

class service_discovery_fsm;
class service_discovery;

///////////////////////////////////////////////////////////////////////////////
// Machine
///////////////////////////////////////////////////////////////////////////////
namespace mpl = boost::mpl;
namespace sc = boost::statechart;

namespace _sd {

struct inactive;
struct fsm: sc::state_machine<fsm, inactive>, public fsm_base {

    fsm(boost::asio::io_service &_io);
    virtual ~fsm();

    void set_fsm(std::shared_ptr<service_discovery_fsm> _fsm);

    void timer_expired(const boost::system::error_code &_error);

    uint32_t initial_delay_;
    uint32_t repetition_base_delay_;
    uint8_t repetition_max_;
    uint32_t cyclic_offer_delay_;

    bool is_up_;
    uint8_t run_;

    std::weak_ptr<service_discovery_fsm> fsm_;
};

struct inactive: sc::state<inactive, fsm> {

    inactive(my_context context);

    typedef mpl::list<sc::custom_reaction<ev_none>,
            sc::custom_reaction<ev_status_change> > reactions;

    sc::result react(const ev_none &_event);
    sc::result react(const ev_status_change &_event);
};

struct initial;
struct active: sc::state<active, fsm, initial> {

    active(my_context _context);
    ~active();

    typedef sc::custom_reaction<ev_status_change> reactions;

    sc::result react(const ev_status_change &_event);
};

struct initial: sc::state<initial, active> {

    initial(my_context _context);

    typedef sc::custom_reaction<ev_timeout> reactions;

    sc::result react(const ev_timeout &_event);
};

struct repeat: sc::state<repeat, active> {

    repeat(my_context _context);

    typedef mpl::list<sc::custom_reaction<ev_timeout>,
            sc::custom_reaction<ev_find_service> > reactions;

    sc::result react(const ev_timeout &_event);
    sc::result react(const ev_find_service &_event);
};

struct announce: sc::state<announce, active> {

    announce(my_context _context);

    typedef mpl::list<sc::custom_reaction<ev_timeout>,
            sc::custom_reaction<ev_find_service>,
            sc::custom_reaction<ev_offer_change> > reactions;

    sc::result react(const ev_timeout &_event);
    sc::result react(const ev_find_service &_event);
    sc::result react(const ev_offer_change &_event);
};

} // namespace _offer

///////////////////////////////////////////////////////////////////////////////
// Interface
///////////////////////////////////////////////////////////////////////////////
class service_discovery_fsm: public std::enable_shared_from_this<
        service_discovery_fsm> {
public:
    service_discovery_fsm(const std::string &_name,
            std::shared_ptr<service_discovery> _discovery);

    const std::string & get_name() const;

    void start();
    void stop();

    void send(bool _is_announcing);

    inline void process(const sc::event_base &_event) {
        std::lock_guard<std::mutex> its_lock(lock_);
        fsm_->process_event(_event);
    }

private:
    std::string name_;

    std::weak_ptr<service_discovery> discovery_;
    std::shared_ptr<_sd::fsm> fsm_;

    std::mutex lock_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_SERVICE_DISCOVERY_FSM_HPP
