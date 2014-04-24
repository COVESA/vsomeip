//
// option.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SD_OPTION_HPP
#define VSOMEIP_SD_OPTION_HPP

#include <vsomeip/deserializable.hpp>
#include <vsomeip/serializable.hpp>
#include <vsomeip/sd/enumeration_types.hpp>

namespace vsomeip {
namespace sd {

class option
	: public serializable,
	  public deserializable {
public:
	virtual ~option() {};
	virtual bool operator==(const option &_option) const = 0;

	virtual uint16_t get_length() const = 0;
	virtual option_type get_type() const = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_OPTION_HPP
