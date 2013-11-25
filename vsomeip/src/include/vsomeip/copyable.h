//
// copyable.h
//
// Date: 	Nov 18, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef __VSOMEIP_IMPL_SERIALIZATION_COPYABLE_H__
#define __VSOMEIP_IMPL_SERIALIZATION_COPYABLE_H__

class Copyable {
public:
	virtual Copyable *copy() const = 0;
};

#endif /* __VSOMEIP_IMPL_SERIALIZATION_COPYABLE_H__ */
