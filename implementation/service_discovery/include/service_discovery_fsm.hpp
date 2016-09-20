// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#include "../../routing/include/serviceinfo.hpp"


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

    void timer_expired(const boost::system::error_code &_error,
            bool _use_alt_timeout);

    uint32_t initial_delay_;
    uint32_t repetitions_base_delay_;
    uint8_t repetitions_max_;
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

struct offer;
struct find;
struct main: sc::state<main, active, mpl::list<offer, find>> {
    main(my_context _context);
};

struct offer: sc::state<offer, main::orthogonal<0> > {

    offer(my_context _context);

    typedef mpl::list<sc::custom_reaction<ev_timeout>,
            sc::custom_reaction<ev_find_service>,
            sc::custom_reaction<ev_offer_change> > reactions;

    sc::result react(const ev_timeout &_event);
    sc::result react(const ev_find_service &_event);
    sc::result react(const ev_offer_change &_event);

    uint8_t run_;
};

struct idle;
struct find: sc::state<find, main::orthogonal<1>, idle> {

    find(my_context _context);

    uint8_t run_;
};

struct idle: sc::state<idle, find> {
    idle(my_context _context);

    typedef mpl::list<sc::custom_reaction<ev_request_service> >reactions;

    sc::result react(const ev_request_service &_event);
};

struct send: sc::state<send, find> {
    send(my_context _context);

    typedef mpl::list<
            sc::custom_reaction<ev_alt_timeout>,
            sc::custom_reaction<ev_none>
    > reactions;

    sc::result react(const ev_alt_timeout &_event);
    sc::result react(const ev_none &_event);
};

} // namespace _sd

///////////////////////////////////////////////////////////////////////////////
// Interface
///////////////////////////////////////////////////////////////////////////////
class service_discovery_fsm: public std::enable_shared_from_this<
        service_discovery_fsm> {
public:
    service_discovery_fsm(std::shared_ptr<service_discovery> _discovery);

    void start();
    void stop();

    bool send(bool _is_announcing, bool _is_find = false);

    void send_unicast_offer_service(
            const std::shared_ptr<const serviceinfo> &_info,
            service_t _service, instance_t _instance,
            major_version_t _major,
            minor_version_t _minor);

    void send_multicast_offer_service(
            const std::shared_ptr<const serviceinfo> &_info,
            service_t _service, instance_t _instance,
            major_version_t _major,
            minor_version_t _minor);

    inline void process(const sc::event_base &_event) {
        std::lock_guard<std::mutex> its_lock(lock_);
        fsm_->process_event(_event);
    }

    inline uint8_t get_repetition_max() const {
        if (!fsm_)
            return 0;

        return fsm_->repetitions_max_;
    }

    std::chrono::milliseconds get_elapsed_offer_timer();

    bool check_is_multicast_offer();

private:
    std::weak_ptr<service_discovery> discovery_;
    std::shared_ptr<_sd::fsm> fsm_;

    std::mutex lock_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_SERVICE_DISCOVERY_FSM_HPP
