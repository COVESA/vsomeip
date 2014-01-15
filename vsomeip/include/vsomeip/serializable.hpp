//
// serializable.hpp
//
// Date: 	Jan 9, 2014
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_SERIALIZABLE_HPP
#define VSOMEIP_SERIALIZABLE_HPP

namespace vsomeip {

class serializer;
class deserializer;

class serializable {
public:
	virtual bool serialize(serializer *_to) const = 0;
	virtual bool deserialize(deserializer *_from) = 0;

protected:
	virtual ~serializable() {};
};

} // namespace vsomeip

#endif // VSOMEIP_SERIALIZABLE_HPP
