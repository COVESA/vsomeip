//
// serializer.cpp
//
// Date: 	Nov 5, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//
#include <vsomeip/serializable.h>
#include <vsomeip/serializer.h>

namespace vsomeip {

Serializer::~Serializer() {
	// intentionally left empty
}

bool Serializer::serialize(Serializable *a_serializable) {
	return (a_serializable ? a_serializable->serialize(this) : false);
}

} // namespace vsomeip





