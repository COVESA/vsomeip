//
// utility.hpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2024 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_UTILITY_HPP
#define VSOMEIP_INTERNAL_UTILITY_HPP

#include <string>

namespace vsomeip {

class utility {
public:
	static void * load_library(const std::string &_path, const std::string &_symbol);
};

} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_UTILITY_HPP
