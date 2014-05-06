//
// proxy_owner.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_PROXY_OWNER_HPP
#define VSOMEIP_INTERNAL_PROXY_OWNER_HPP

#include <string>

#include <boost/shared_ptr.hpp>

#include <vsomeip_internal/owner_base.hpp>

namespace vsomeip {

class proxy;

class proxy_owner : virtual public owner_base {
public:
	proxy_owner(const std::string &_name);
	~proxy_owner();

protected:
	boost::shared_ptr< proxy > proxy_;
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_PROXY_OWNER_HPP
