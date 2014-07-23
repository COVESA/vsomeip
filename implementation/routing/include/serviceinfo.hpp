// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICEINFO_HPP
#define VSOMEIP_SERVICEINFO_HPP

#include <memory>
#include <set>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;
class servicegroup;

class serviceinfo {
public:
	serviceinfo(major_version_t _major, minor_version_t _minor, ttl_t _ttl);
	~serviceinfo();

	servicegroup * get_group() const;
	void set_group(servicegroup *_group);

	major_version_t get_major() const;
	minor_version_t get_minor() const;

	ttl_t get_ttl() const;
	void set_ttl(ttl_t _ttl);

	std::shared_ptr< endpoint > & get_endpoint(bool _reliable);
	void set_endpoint(std::shared_ptr< endpoint > &_endpoint, bool _reliable);

	void add_client(client_t _client);
	void remove_client(client_t _client);

private:
	servicegroup *group_;

	major_version_t major_;
	minor_version_t minor_;
	ttl_t ttl_;

	std::shared_ptr< endpoint > reliable_;
	std::shared_ptr< endpoint > unreliable_;

	std::set< client_t > requesters_;
};

} // namespace vsomeip

#endif // VSOMEIP_SERVICE_ROUTING_INFO_HPP
