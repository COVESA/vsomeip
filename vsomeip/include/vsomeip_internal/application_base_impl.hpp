//
// application_base_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_APPLICATION_BASE_IMPL_HPP
#define VSOMEIP_INTERNAL_APPLICATION_BASE_IMPL_HPP

namespace vsomeip {

class application_base_impl
	: public log_owner,
	  public noncopyable {

public:
	void send_error_message8

protected:
  	boost::asio::io_service service_;

private:
	boost::sharef_ptr< serializer > serializer_;
	boost::shared_ptr< deserializer > deserializer_;

	// receiver
	typedef std::map< method_id,
  	   	     std::set< receive_cbk_t > > method_filter_map;
	typedef std::map< service_id,
					   method_filter_map > service_filter_map;

	service_filter_map receive_cbks_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_APPLICATION_BASE_IMPL_HPP
