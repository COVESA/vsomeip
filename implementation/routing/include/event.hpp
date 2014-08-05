// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_EVENT_IMPL_HPP
#define VSOMEIP_EVENT_IMPL_HPP

#include <map>
#include <memory>
#include <mutex>
#include <set>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/system_timer.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;
class endpoint_definition;
class message;
class payload;
class routing_manager;

class event : public std::enable_shared_from_this<event> {
 public:
  event(routing_manager *_routing);

  service_t get_service() const;
  void set_service(service_t _service);

  instance_t get_instance() const;
  void set_instance(instance_t _instance);

  event_t get_event() const;
  void set_event(event_t _event);

  const std::shared_ptr<payload> get_payload() const;
  void set_payload(std::shared_ptr<payload> _payload);

  bool is_field() const;
  void set_field(bool _is_field);

  // SIP_RPC_357
  void set_update_cycle(std::chrono::milliseconds &_cycle);

  // SIP_RPC_358
  void set_update_on_change(bool _is_on);

  // SIP_RPC_359 (epsilon change) is not supported!


  const std::set<eventgroup_t> & get_eventgroups() const;
  void add_eventgroup(eventgroup_t _eventgroup);
  void set_eventgroups(const std::set<eventgroup_t> &_eventgroups);

  void notify_one(const std::shared_ptr<endpoint_definition> &_target);

 private:
  void update_cbk(boost::system::error_code const &_error);
  void notify();

 private:
  routing_manager *routing_;
  std::mutex mutex_;
  std::shared_ptr<message> message_;

  bool is_field_;

  boost::asio::system_timer cycle_timer_;
  std::chrono::milliseconds cycle_;

  bool is_updating_on_change_;

  std::set<eventgroup_t> eventgroups_;
};

}  // namespace vsomeip

#endif // VSOMEIP_EVENT_IMPL_HPP
