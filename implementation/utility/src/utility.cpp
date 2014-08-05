// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <dlfcn.h>
#include <sys/stat.h>

#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>

#include "../include/byteorder.hpp"
#include "../include/utility.hpp"

namespace vsomeip {

bool utility::is_notification(const byte_t *_data) {
	return (0 == _data[VSOMEIP_CLIENT_POS_MIN]
			&& 0 == _data[VSOMEIP_CLIENT_POS_MAX]
			&& 0 == _data[VSOMEIP_SESSION_POS_MIN]
			&& 0 == _data[VSOMEIP_SESSION_POS_MAX]);
}

uint32_t utility::get_message_size(const byte_t *_data, uint32_t _size) {
	uint32_t its_size(0);
	if (VSOMEIP_SOMEIP_HEADER_SIZE <= _size) {
		its_size = VSOMEIP_SOMEIP_HEADER_SIZE
				+ VSOMEIP_BYTES_TO_LONG(_data[4], _data[5], _data[6], _data[7]);
	}
	return (its_size);
}

uint32_t utility::get_payload_size(const byte_t *_data, uint32_t _size) {
	uint32_t its_size(0);
	if (VSOMEIP_SOMEIP_HEADER_SIZE <= _size) {
		its_size = VSOMEIP_BYTES_TO_LONG(_data[4], _data[5], _data[6], _data[7])
				- VSOMEIP_SOMEIP_HEADER_SIZE;
	}
	return (its_size);
}

void * utility::load_library(const std::string &_path,
		const std::string &_symbol) {
	void * its_symbol = 0;

	void *handle = dlopen(_path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
	if (0 != handle) {
		its_symbol = dlsym(handle, _symbol.c_str());
	} else {
		VSOMEIP_ERROR<< "Loading failed: (" << dlerror() << ")";
	}

	return (its_symbol);
}

bool utility::exists(const std::string &_path) {
	struct stat its_stat;
	return (stat(_path.c_str(), &its_stat) == 0);
}

}  // namespace vsomeip
