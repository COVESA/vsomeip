// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_EVENTGROUPINFO_HPP
#define VSOMEIP_EVENTGROUPINFO_HPP

#include <memory>
#include <set>

#include <boost/asio/ip/address.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint_definition;
class event;

class eventgroupinfo {
public:
	eventgroupinfo();
	eventgroupinfo(major_version_t _major, ttl_t _ttl);
	~eventgroupinfo();

	major_version_t get_major() const;
	void set_major(major_version_t _major);

	ttl_t get_ttl() const;
	void set_ttl(ttl_t _ttl);

	bool is_multicast() const;
	bool get_multicast(boost::asio::ip::address &_address, uint16_t &_port) const;
	void set_multicast(const boost::asio::ip::address &_address, uint16_t _port);

	const std::set<std::shared_ptr<event> > get_events() const;
	void add_event(std::shared_ptr<event> _event);

	const std::set<std::shared_ptr<endpoint_definition> > get_targets() const;
	void add_target(std::shared_ptr<endpoint_definition> _target);
	void del_target(std::shared_ptr<endpoint_definition> _target);
	void clear_targets();

private:
	major_version_t major_;
	ttl_t ttl_;

	bool is_multicast_;
	boost::asio::ip::address address_;
	uint16_t port_;

	std::set<std::shared_ptr<event> > events_;
	std::set<std::shared_ptr<endpoint_definition> > targets_;
};

} // namespace vsomeip

#endif // VSOMEIP_EVENTGROUPINFO_HPP
