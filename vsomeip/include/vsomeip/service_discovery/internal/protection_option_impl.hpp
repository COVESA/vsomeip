//
// protection_option_impl.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_PROTECTION_OPTION_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_PROTECTION_OPTION_IMPL_HPP

#include <vsomeip/service_discovery/protection_option.hpp>
#include <vsomeip/service_discovery/internal/option_impl.hpp>

namespace vsomeip {
namespace service_discovery {

class protection_option_impl
		: virtual public protection_option,
		  virtual public option_impl {
public:
	protection_option_impl();
	virtual ~protection_option_impl();
	bool operator ==(const option &_other) const;

	alive_counter get_alive_counter() const;
	void set_alive_counter(alive_counter _counter);

	crc get_crc() const;
	void set_crc(crc _crc);

	bool serialize(vsomeip::serializer *_to) const;
	bool deserialize(vsomeip::deserializer *_from);

private:
	alive_counter counter_;
	crc crc_;
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_PROTECTION_OPTION_IMPL_HPP
