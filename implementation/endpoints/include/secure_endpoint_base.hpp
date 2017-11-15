// Copyright (c) 2017 by Vector Informatik GmbH
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef IMPLEMENTATION_ENDPOINTS_INCLUDE_SECURE_ENDPOINT_BASE_HPP_
#define IMPLEMENTATION_ENDPOINTS_INCLUDE_SECURE_ENDPOINT_BASE_HPP_

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include <string>
#include <vector>

namespace vsomeip {

class secure_endpoint_base {
public:
    secure_endpoint_base(bool _confidential, const std::vector<std::uint8_t> &_psk, const std::string &_pskid);
    virtual ~secure_endpoint_base();

protected:
    std::uint32_t get_max_dtls_payload_size();

    const std::vector<int> cipher_suite_;
    const std::vector<std::uint8_t> psk_;
    const std::string pskid_;
};

}  // namespace vsomeip

#endif  // IMPLEMENTATION_ENDPOINTS_INCLUDE_SECURE_ENDPOINT_BASE_HPP_
