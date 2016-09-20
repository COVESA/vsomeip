// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include "../include/defines.hpp"
#include "../include/service_discovery.hpp"
#include "../include/service_discovery_fsm.hpp"
#include "../../routing/include/serviceinfo.hpp"

#include "../../configuration/include/configuration.hpp"
#include "../../logging/include/logger.hpp"

namespace vsomeip {
namespace sd {

///////////////////////////////////////////////////////////////////////////////
// Machine
///////////////////////////////////////////////////////////////////////////////
namespace _sd {

fsm::fsm(boost::asio::io_service &_io)
        : fsm_base(_io), is_up_(true) {
}

fsm::~fsm() {
}

void fsm::set_fsm(std::shared_ptr<service_discovery_fsm> _fsm) {
    fsm_ = _fsm;
}

void fsm::timer_expired(const boost::system::error_code &_error,
        bool _use_alt_timeout) {
    if (!_error) {
        std::shared_ptr<service_discovery_fsm> its_fsm = fsm_.lock();
        if (its_fsm) {
            if (_use_alt_timeout) {
                its_fsm->process(ev_alt_timeout());
            } else {
                its_fsm->process(ev_timeout());
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
// State "Inactive"
///////////////////////////////////////////////////////////////////////////////
inactive::inactive(my_context context)
        : sc::state<inactive, fsm>(context) {
    std::shared_ptr < service_discovery_fsm > fsm =
            outermost_context().fsm_.lock();
    if (fsm) {
        VSOMEIP_TRACE << "sd::inactive";
        outermost_context().run_ = 0;
    }
}

sc::result inactive::react(const ev_none &_event) {
    (void)_event;

    if (outermost_context().is_up_) {
        return transit<active>();
    }

    return discard_event();
}

sc::result inactive::react(const ev_status_change &_event) {
    (void)_event;

    outermost_context().is_up_ = _event.is_up_;
    if (outermost_context().is_up_) {
        return transit<active>();
    }

    return discard_event();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active"
///////////////////////////////////////////////////////////////////////////////
active::active(my_context _context)
        : sc::state<active, fsm, initial>(_context) {
    std::shared_ptr < service_discovery_fsm > fsm =
            outermost_context().fsm_.lock();
    if (fsm) {
        VSOMEIP_TRACE << "sd::active";
    }
}

active::~active() {
}

sc::result active::react(const ev_status_change &_event) {
    (void)_event;

    outermost_context().stop_timer();
    outermost_context().is_up_ = _event.is_up_;
    if (!outermost_context().is_up_)
        return transit<inactive>();

    return forward_event();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Initial"
///////////////////////////////////////////////////////////////////////////////
initial::initial(my_context _context)
        : sc::state<initial, active>(_context) {
    std::shared_ptr < service_discovery_fsm > fsm =
            outermost_context().fsm_.lock();
    if (fsm) {
        VSOMEIP_TRACE << "sd::active.initial";
        outermost_context().start_timer(outermost_context().initial_delay_);
    }
}

sc::result initial::react(const ev_timeout &_event) {
    (void)_event;
    return transit<repeat>();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Repeat"
///////////////////////////////////////////////////////////////////////////////
repeat::repeat(my_context _context)
        : sc::state<repeat, active>(_context) {
    std::shared_ptr < service_discovery_fsm > fsm =
            outermost_context().fsm_.lock();
    if (fsm) {
        VSOMEIP_TRACE << "sd::active.repeat";
        uint32_t its_timeout = (outermost_context().repetitions_base_delay_
                << outermost_context().run_);
        outermost_context().run_++;
        (void)fsm->send(false);
        outermost_context().start_timer(its_timeout);
    }
}

sc::result repeat::react(const ev_timeout &_event) {
    (void)_event;
    if (outermost_context().run_ < outermost_context().repetitions_max_) {
        return transit<repeat>();
    }
    return transit<main>();
}

sc::result repeat::react(const ev_find_service &_event) {
    VSOMEIP_TRACE << "sd::active.repeat.react.find";
    std::shared_ptr < service_discovery_fsm > fsm =
            outermost_context().fsm_.lock();
    // Answer Find Service messages with unicast offer in repetition phase
    if (fsm) {
        fsm->send_unicast_offer_service(_event.info_, _event.service_, _event.instance_,
            _event.major_, _event.minor_);
    }
    return forward_event();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Main"
///////////////////////////////////////////////////////////////////////////////
main::main(my_context _context)
    : sc::state<main, active, mpl::list<offer, find> >(_context) {
    VSOMEIP_TRACE << "sd::active.main";
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Main.Offer"
///////////////////////////////////////////////////////////////////////////////
offer::offer(my_context _context)
        : sc::state<offer, main::orthogonal<0> >(_context) {
    std::shared_ptr < service_discovery_fsm > fsm =
            outermost_context().fsm_.lock();
    if (fsm) {
        VSOMEIP_TRACE << "sd::active.main.offer";
        outermost_context().start_timer(
                outermost_context().cyclic_offer_delay_);
        (void)fsm->send(true);
    }
}

sc::result offer::react(const ev_timeout &_event) {
    (void)_event;
    return transit<offer>();
}

sc::result offer::react(const ev_find_service &_event) {
    VSOMEIP_TRACE << "sd::active.main.react.find";
    std::shared_ptr < service_discovery_fsm > fsm =
            outermost_context().fsm_.lock();
    if (fsm) {
        if(_event.unicast_flag_) {
            if( !fsm->check_is_multicast_offer()) { // SIP_SD_89
                fsm->send_unicast_offer_service(_event.info_, _event.service_, _event.instance_,
                        _event.major_, _event.minor_);
            } else { // SIP_SD_90
                fsm->send_multicast_offer_service(_event.info_, _event.service_, _event.instance_,
                        _event.major_, _event.minor_);
            }
        } else { // SIP_SD_91
            fsm->send_multicast_offer_service(_event.info_, _event.service_, _event.instance_,
                    _event.major_, _event.minor_);
        }
    }
    return forward_event();
}

sc::result offer::react(const ev_offer_change &_event) {
    (void)_event;
    return transit<offer>();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Announce.Main.Find"
///////////////////////////////////////////////////////////////////////////////
find::find(my_context _context)
    : sc::state<find, main::orthogonal<1>, idle>(_context) {
    VSOMEIP_TRACE << "sd::active.main.find";
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Announce.Idle"
///////////////////////////////////////////////////////////////////////////////
idle::idle(my_context _context)
    : sc::state<idle, find>(_context) {
    VSOMEIP_TRACE << "sd::active.main.find.idle";
    context<find>().run_ = 0;
}

sc::result idle::react(const ev_request_service &_event) {
    (void)_event;
    return transit<send>();
}

///////////////////////////////////////////////////////////////////////////////
// State "Active.Announce.Main.Find.Send"
///////////////////////////////////////////////////////////////////////////////
send::send(my_context _context)
    : sc::state<send, find>(_context) {
    std::shared_ptr < service_discovery_fsm > fsm =
            outermost_context().fsm_.lock();
    if (fsm) {
        VSOMEIP_TRACE << "sd::active.main.find.send";
        // Increment to the maximum run value (which is repetition_max-1)
        // As new request might be added in the meantime, this will be
        // used to calculate the maximum cycle time.
        if (context<find>().run_ < fsm->get_repetition_max()) {
            context<find>().run_++;
            uint32_t its_timeout = (outermost_context().repetitions_base_delay_
                    << context<find>().run_);
            if (fsm->send(true, true))
                outermost_context().start_timer(its_timeout, true);
        }
        else {
            post_event(ev_none());
        }
    } else {
        post_event(ev_none());
    }
}

sc::result send::react(const ev_alt_timeout &_event) {
    (void)_event;
    return transit<send>();
}

sc::result send::react(const ev_none &_event) {
    (void)_event;
    return transit<idle>();
}

} // namespace _sd

///////////////////////////////////////////////////////////////////////////////
// Interface
///////////////////////////////////////////////////////////////////////////////
service_discovery_fsm::service_discovery_fsm(
        std::shared_ptr<service_discovery> _discovery)
        : discovery_(_discovery), fsm_(
                std::make_shared < _sd::fsm > (_discovery->get_io())) {
    std::shared_ptr < service_discovery > discovery = discovery_.lock();
    if (discovery) {
        std::shared_ptr < configuration > its_configuration =
                discovery->get_configuration();

        int32_t its_initial_delay_min
            = its_configuration->get_sd_initial_delay_min();
        if (its_initial_delay_min < 0)
            its_initial_delay_min = VSOMEIP_SD_DEFAULT_INITIAL_DELAY_MIN;

        int32_t its_initial_delay_max
            = its_configuration->get_sd_initial_delay_max();
        if (its_initial_delay_max <= 0)
            its_initial_delay_max = VSOMEIP_SD_DEFAULT_INITIAL_DELAY_MAX;

        if (its_initial_delay_min > its_initial_delay_max) {
            int32_t tmp_initial_delay = its_initial_delay_min;
            its_initial_delay_min = its_initial_delay_max;
            its_initial_delay_max = tmp_initial_delay;
        }

        VSOMEIP_TRACE << "Inital delay [" << its_initial_delay_min << ", "
                << its_initial_delay_max << "]";

        boost::random::mt19937 its_generator;
        boost::random::uniform_int_distribution<> its_distribution(
                its_initial_delay_min, its_initial_delay_max);
        fsm_->initial_delay_ = its_distribution(its_generator);

        fsm_->repetitions_base_delay_
            = its_configuration->get_sd_repetitions_base_delay();
        if (fsm_->repetitions_base_delay_ <= 0)
                fsm_->repetitions_base_delay_
                    = VSOMEIP_SD_DEFAULT_REPETITIONS_BASE_DELAY;
        fsm_->repetitions_max_
            = its_configuration->get_sd_repetitions_max();
        if (fsm_->repetitions_max_ <= 0)
            fsm_->repetitions_max_ = VSOMEIP_SD_DEFAULT_REPETITIONS_MAX;

        fsm_->cyclic_offer_delay_
            = its_configuration->get_sd_cyclic_offer_delay();
        if (fsm_->cyclic_offer_delay_ <= 0)
            fsm_->cyclic_offer_delay_ = VSOMEIP_SD_DEFAULT_CYCLIC_OFFER_DELAY;

        VSOMEIP_INFO << "SD configuration [" << std::dec << fsm_->initial_delay_ << ":"
                << std::dec << fsm_->repetitions_base_delay_ << ":"
                << std::dec << (int) fsm_->repetitions_max_ << ":"
                << std::dec << fsm_->cyclic_offer_delay_ << "]";
    } else {
        VSOMEIP_ERROR << "SD initialization failed";
    }
}

void service_discovery_fsm::start() {
    fsm_->set_fsm(shared_from_this());
    fsm_->initiate();
}

void service_discovery_fsm::stop() {
}

bool service_discovery_fsm::send(bool _is_announcing, bool _is_find) {
    std::shared_ptr < service_discovery > discovery = discovery_.lock();
    if (discovery) {
        return discovery->send(_is_announcing, _is_find);
    }
    return false;
}

void service_discovery_fsm::send_unicast_offer_service(
        const std::shared_ptr<const serviceinfo> &_info,
        service_t _service, instance_t _instance,
        major_version_t _major,
        minor_version_t _minor) {
    std::shared_ptr < service_discovery > discovery = discovery_.lock();
    if (discovery) {
        discovery->send_unicast_offer_service(_info, _service,
                _instance, _major, _minor );
    }
}

void service_discovery_fsm::send_multicast_offer_service(
        const std::shared_ptr<const serviceinfo> &_info, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor) {
    std::shared_ptr < service_discovery > discovery = discovery_.lock();
    if (discovery) {
        discovery->send_multicast_offer_service(_info, _service,
                _instance, _major, _minor );
    }
}

std::chrono::milliseconds service_discovery_fsm::get_elapsed_offer_timer() {
    //get remaining time to next offer since last offer
    std::chrono::milliseconds remaining =
            std::chrono::milliseconds(fsm_->expired_from_now(false));

    if( std::chrono::milliseconds(0) > remaining) {
        remaining = std::chrono::milliseconds(fsm_->cyclic_offer_delay_);
    }
    return std::chrono::milliseconds(fsm_->cyclic_offer_delay_) - remaining;
}

bool service_discovery_fsm::check_is_multicast_offer() {
    bool is_multicast(false);
    std::chrono::milliseconds elapsed_ = get_elapsed_offer_timer();
    uint32_t half_cyclic_offer_delay_ = fsm_->cyclic_offer_delay_ / 2;

    if( elapsed_ >= std::chrono::milliseconds(half_cyclic_offer_delay_)) {
        // Response must be a multicast offer (SIP_SD_90)
        is_multicast = true;
    }
    return is_multicast;
}

} // namespace sd
} // namespace vsomeip
