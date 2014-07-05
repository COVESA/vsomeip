// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_UTILITY_HPP
#define VSOMEIP_UTILITY_HPP

#include <memory>
#include <vector>

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/message.hpp>

namespace vsomeip {

class utility {
public:
	static void * load_library(const std::string &_path, const std::string &_symbol);

	static inline bool is_request(std::shared_ptr< message > _message) {
		return (_message ? is_request(_message->get_message_type()) : false);
	}

	static inline bool is_request(byte_t _type) {
		return is_request(static_cast< message_type_e >(_type));
	}

	static inline bool is_request(message_type_e _type) {
		return ((_type < message_type_e::NOTIFICATION) ||
				(_type >= message_type_e::REQUEST_ACK &&
					_type <= message_type_e::REQUEST_NO_RETURN_ACK));
	}

	static uint32_t get_message_size(std::vector< byte_t > &_data);

	static bool exists(const std::string &_path);
};

} // namespace vsomeip

#endif // VSOMEIP_UTILITY_HPP
