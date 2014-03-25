//
// participant_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_PARTICIPANT_IMPL_HPP
#define VSOMEIP_INTERNAL_PARTICIPANT_IMPL_HPP

#include <map>

#include <boost/asio/io_service.hpp>
#include <boost/asio/system_timer.hpp>

#include <vsomeip_internal/participant.hpp>
#include <vsomeip_internal/statistics_owner_impl.hpp>

namespace vsomeip {

class endpoint;
class managing_application;

template < int MaxBufferSize >
class participant_impl
	: virtual public participant
#ifdef USE_VSOMEIP_STATISTICS
	, virtual public statistics_owner_impl
#endif
{
public: // provided
	participant_impl(managing_application *_owner, const endpoint *_location);
	virtual ~participant_impl();

	void enable_magic_cookies();
	void disable_magic_cookies();

	void open_filter(service_id _service);
	void close_filter(service_id _service);

	void receive_cbk(
			boost::system::error_code const &_error, std::size_t _bytes);

public: // required
	virtual bool is_client() const = 0;
	virtual const uint8_t * get_buffer() const = 0;

	virtual void receive() = 0;
	virtual void restart() = 0;

	virtual ip_address get_remote_address() const = 0;
	virtual ip_port get_remote_port() const = 0;
	virtual ip_protocol get_protocol() const = 0;

private:
	uint32_t get_message_size() const;

	virtual bool is_magic_cookie() const;
	bool resync_on_magic_cookie();

protected:
	// Reference to service context
	boost::asio::io_service &service_;

	// Reference to managing application
	managing_application *owner_;

	// The local endpoint
	const endpoint *location_;

	bool is_supporting_magic_cookies_;
	bool has_enabled_magic_cookies_;

	// Data of the current message
	std::vector< uint8_t > message_;

	// Filter configuration
	std::map< service_id, uint8_t > opened_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_PARTICIPANT_IMPL_HPP
