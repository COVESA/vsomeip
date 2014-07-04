// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_ROUTING_INFO_HPP
#define VSOMEIP_SERVICE_ROUTING_INFO_HPP

#include <memory>
#include <set>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;

class service_info {
public:
	service_info(major_version_t _major, minor_version_t _minor, ttl_t _ttl);
	~service_info();

	major_version_t get_major() const;
	minor_version_t get_minor() const;

	ttl_t get_ttl() const;
	void set_ttl(ttl_t _ttl);

	std::shared_ptr< endpoint > & get_reliable_endpoint();
	void set_reliable_endpoint(std::shared_ptr< endpoint > &_endpoint);

	std::shared_ptr< endpoint > & get_unreliable_endpoint();
	void set_unreliable_endpoint(std::shared_ptr< endpoint > &_endpoint);

	void add_client(client_t _client);
	void remove_client(client_t _client);

private:
	major_version_t major_;
	minor_version_t minor_;
	ttl_t ttl_;

	std::shared_ptr< endpoint > reliable_endpoint_;
	std::shared_ptr< endpoint > unreliable_endpoint_;

	std::set< client_t > requesters_;
};

} // namespace vsomeip

#endif // VSOMEIP_SERVICE_ROUTING_INFO_HPP
