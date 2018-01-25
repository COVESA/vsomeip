// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <ostream>
#include "../../../e2e_protection/include/e2exf/config.hpp"

namespace vsomeip {

std::ostream &operator<<(std::ostream &_os, const e2exf::data_identifier &_data_identifier) {
    _os << _data_identifier.first << _data_identifier.second;
    return _os;
}

} // namespace vsomeip
