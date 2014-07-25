// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_EVENTGROUPINFO_HPP
#define VSOMEIP_EVENTGROUPINFO_HPP

#include <memory>
#include <set>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;

class eventgroupinfo {
public:
	eventgroupinfo(major_version_t _major, ttl_t _ttl);
	~eventgroupinfo();

	servicegroup * get_group() const;
	void set_group(servicegroup *_group);

	major_version_t get_major() const;

	ttl_t get_ttl() const;
	void set_ttl(ttl_t _ttl);

	std::shared_ptr<endpoint> & get_multicast();
	void set_multicast(std::shared_ptr<endpoint> &_multicast);

	void add_client(client_t _client);
	void remove_client(client_t _client);

private:
	major_version_t major_;
	ttl_t ttl_;

	std::shared_ptr<endpoint> multicast_;
	std::set< client_t > subscribed_;
};

} // namespace vsomeip

#endif // VSOMEIP_EVENTGROUPINFO_HPP
