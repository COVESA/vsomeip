/*
 * participant.hpp
 *
 *  Created on: Jan 26, 2014
 *      Author: lutz
 */

#ifndef PARTICIPANT_HPP_
#define PARTICIPANT_HPP_

#include <vsomeip/constants.hpp>

#ifdef USE_VSOMEIP_STATISTICS
#include <vsomeip/statistics_owner.hpp>
#endif

namespace vsomeip {

class message_base;
class receiver;

class participant
#ifdef USE_VSOMEIP_STATISTICS
: virtual public statistics_owner
#endif
{
public:
	virtual void start() = 0;
	virtual void stop() = 0;

	virtual bool send(const message_base *_message, bool _flush = true) = 0;
	virtual void register_for(receiver *_receiver,
			 	 	 	 	 	 service_id _service_id,
			 	 	 	 	 	 method_id _method_id) = 0;
	virtual void unregister_for(receiver * receiver,
								   service_id _service_id,
								   method_id _method_id) = 0;


	virtual std::size_t poll_one() = 0;
	virtual std::size_t poll() = 0;
	virtual std::size_t run() = 0;
};

} // namespace vsomeip

#endif /* PARTICIPANT_HPP_ */
