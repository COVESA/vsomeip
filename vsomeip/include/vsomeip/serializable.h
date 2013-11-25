//
// serializable.h
//
// Date: 	Oct 31, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_SERIALIZABLE_H__
#define __VSOMEIP_SERIALIZABLE_H__

namespace vsomeip {

class Serializer;
class Deserializer;

class Serializable {
public:
	virtual ~Serializable() {};

	virtual bool serialize(Serializer *a_serializer) const = 0;
	virtual bool deserialize(Deserializer *a_deserializer) = 0;
};

} // namespace vsomeip

#endif // __VSOMEIP_SERIALIZABLE_H__
