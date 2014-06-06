//
// deserializable.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_DESERIALIZABLE_HPP
#define VSOMEIP_DESERIALIZABLE_HPP

namespace vsomeip {

class deserializer;

class deserializable {
public:
	virtual bool deserialize(deserializer *_from) = 0;

protected:
	virtual ~deserializable() {};
};

} // namespace vsomeip

#endif // VSOMEIP_SERIALIZABLE_HPP
