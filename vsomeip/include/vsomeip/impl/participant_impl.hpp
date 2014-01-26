/*
 * participant_impl.hpp
 *
 *  Created on: Jan 25, 2014
 *      Author: lutz
 */

#ifndef PARTICIPANT_IMPL_HPP_
#define PARTICIPANT_IMPL_HPP_

#include <map>
#include <set>

#include <boost/asio/io_service.hpp>

#include <vsomeip/participant.hpp>
#include <vsomeip/impl/statistics_owner_impl.hpp>

namespace vsomeip {

class deserializer;
class receiver;

class participant_impl
	: virtual public participant
#ifdef USE_VSOMEIP_STATISTICS
	, virtual public statistics_owner_impl
#endif
{
public:
	participant_impl(uint32_t _max_message_size);
	virtual ~participant_impl();

	std::size_t poll_one();
	std::size_t poll();
	std::size_t run();

	virtual void register_for(receiver *_receiver,
								 service_id _service_id,
								 method_id _method_id);
	virtual void unregister_for(receiver * receiver,
			 	 	 	 	 	   service_id _service_id,
			 	 	 	 	 	   method_id _method_id);

	void received(boost::system::error_code const &_error_code,
				  std::size_t _transferred_bytes);

protected:
	virtual void send_queued() = 0;
	virtual void restart() = 0;

private:
	// Methods to build an endpoint for replying
	virtual std::string get_remote_address() const = 0;
	virtual uint16_t get_remote_port() const = 0;
	virtual ip_protocol get_protocol() const = 0;
	virtual ip_version get_version() const = 0;

	void receive(const message_base *_message) const;
	virtual const uint8_t * get_received() const = 0;

protected:
	std::map< service_id,
			  std::map< method_id,
			  	  	    std::set< receiver * > > > receiver_registry_;

	serializer * serializer_;
	deserializer * deserializer_;

	uint32_t max_message_size_;

	boost::asio::io_service is_;
};


} // namespace vsomeip

#endif /* PARTICIPANT_IMPL_HPP_ */
