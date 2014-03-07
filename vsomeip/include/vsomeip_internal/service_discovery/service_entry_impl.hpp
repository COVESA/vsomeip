//
// service_entry_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SERVICE_DISCOVERY_SERVICE_ENTRY_IMPL_HPP
#define VSOMEIP_INTERNAL_SERVICE_DISCOVERY_SERVICE_ENTRY_IMPL_HPP

#include <vsomeip/service_discovery/service_entry.hpp>
#include <vsomeip_internal/service_discovery/entry_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class service_entry_impl
		: virtual public service_entry,
		  virtual public entry_impl {
public:
	service_entry_impl();
	virtual ~service_entry_impl();

	minor_version get_minor_version() const;
	void set_minor_version(minor_version _version);

	bool serialize(vsomeip::serializer *_to) const;
	bool deserialize(vsomeip::deserializer *_from);

private:
	minor_version minor_version_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SERVICE_DISCOVERY_SERVICE_ENTRY_IMPL_HPP
