//
// utility.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright 2013, 2024 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//
#include <dlfcn.h>

#include <vsomeip_internal/utility.hpp>

namespace vsomeip {

void * utility::load_library(const std::string &_path, const std::string &_symbol) {
	void * its_symbol = 0;

	void *handle = dlopen(_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
	if (0 != handle) {
		its_symbol = dlsym(handle, _symbol.c_str());
	} else {
	}

	return its_symbol;
}

} // namespace vsomeip




