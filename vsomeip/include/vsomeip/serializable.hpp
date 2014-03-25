//
// deserializable.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERIALIZABLE_HPP
#define VSOMEIP_SERIALIZABLE_HPP

namespace vsomeip {

class serializer;

class serializable {
public:
	virtual bool serialize(serializer *_to) const = 0;

protected:
	virtual ~serializable() {};
};

} // namespace vsomeip

#endif // VSOMEIP_SERIALIZABLE_HPP
