//
// copyable.hpp
//
// Date: 	Nov 18, 2013
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013 Bayerische Motoren Werke AG (BMW). 
// All rights reserved.
//

#ifndef VSOMEIP_IMPL_SERIALIZATION_COPYABLE_HPP
#define VSOMEIP_IMPL_SERIALIZATION_COPYABLE_HPP

namespace vsomeip {

class copyable {
public:
	virtual copyable *copy(bool _is_deep_copy_request = true) const = 0;

protected:
	virtual ~copyable() {};
};

} // namespace vsomeip

#endif // VSOMEIP_IMPL_SERIALIZATION_COPYABLE_HPP
