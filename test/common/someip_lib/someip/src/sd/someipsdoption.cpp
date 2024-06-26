// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <someip/sd/someipsdoption.hpp>

namespace vsomeip_utilities {
namespace someip {
namespace sd {

// CTor
SomeIpSdOption::SomeIpSdOption() {
    m_podPayload.length = calcInitialSdOptionHeaderLengthAttr();
}

std::size_t SomeIpSdOption::payloadSize() const {
    // SD Options length include the reserved attribute size
    return static_cast<std::size_t>(length()) - sizeof(m_podPayload.reserved);
}

SomeIpSdOptionBuilder SomeIpSdOption::create() {
    return SomeIpSdOptionBuilder();
}

types::sdOptionType SomeIpSdOption::type() const {
    return m_podPayload.type;
}

types::sdOptionLength SomeIpSdOption::length() const {
    return m_podPayload.length;
}

std::uint8_t SomeIpSdOption::reserved() const {
    return m_podPayload.reserved;
}

void SomeIpSdOption::setType(const types::sdOptionType& _val) {
    m_podPayload.type = _val;
}

void SomeIpSdOption::print() const {
    // Store original format
    std::ios_base::fmtflags prevFmt(std::cout.flags());

    // Print payload
    std::cout << "[" __FILE__ "]"
        << "\n\tPOD size: " << serialization::PodPayload<SdOptionHeaderRaw_t>::size()
        << "\n\t--"
        << "\n\tlength: " << length() << std::hex << std::showbase
        << "\n\ttype: " << unsigned(type())
        << "\n\treserved: " << unsigned(m_podPayload.reserved)
        << "\n\t--" << std::dec
        << "\n\tPayload Size: " << unsigned(payloadSize()) << '\n';

    // Restore original format
    std::cout.flags(prevFmt);
}

SomeIpSdOptionBuilder::operator SomeIpSdOption() const {
    return std::move(m_entry);
}

SomeIpSdOptionBuilder& SomeIpSdOptionBuilder::type(const types::sdOptionType& _val) {
    m_entry.setType(_val);
    return *this;
}

} // namespace sd
} // namespace someip
} // namespace vsomeip_utilities
