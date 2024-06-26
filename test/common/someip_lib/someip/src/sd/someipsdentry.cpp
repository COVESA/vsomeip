// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <someip/sd/someipsdentry.hpp>

namespace vsomeip_utilities {
namespace someip {
namespace sd {

SomeIpSdEntryBuilder SomeIpSdEntry::create() {
    return SomeIpSdEntryBuilder();
}

SomeIpSdEntryBuilder::operator SomeIpSdEntry() const {
    return std::move(m_entry);
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::type(const types::sdEntryType& _val) {
    m_entry.setType(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::index1st(const types::sdIndex1st& _val) {
    m_entry.setIndex1st(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::index2nd(const types::sdIndex2nd& _val) {
    m_entry.setIndex2nd(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::optsCount(const types::sdOptsCount& _val) {
    m_entry.setOptsCount(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::optsCount1st(const types::sdOptsCount& _val) {
    m_entry.setOptsCount1st(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::optsCount2nd(const types::sdOptsCount& _val) {
    m_entry.setOptsCount2nd(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::serviceId(const types::sdServiceId& _val) {
    m_entry.setServiceId(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::instanceId(const types::sdInstanceId& _val) {
    m_entry.setInstanceId(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::majorVersion(const types::sdMajorVersion& _val) {
    m_entry.setMajorVersion(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::ttl(const std::uint32_t& _val) {
    m_entry.setTtl(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::serviceMinorVersion(const types::sdMinorVersion _val) {
    m_entry.setServiceMinorVersion(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::eventgroupCounter(const types::sdCounter _val) {
    m_entry.setEventgroupCounter(_val);
    return *this;
}

SomeIpSdEntryBuilder& SomeIpSdEntryBuilder::eventgroupId(const types::sdEventGroupId _val) {
    m_entry.setEventgroupId(_val);
    return *this;
}

types::sdEntryType SomeIpSdEntry::type() const {
    return m_podPayload.type;
}

types::sdIndex1st SomeIpSdEntry::index1st() const {
    return m_podPayload.index1st;
}

types::sdIndex2nd SomeIpSdEntry::index2nd() const {
    return m_podPayload.index2nd;
}

types::sdOptsCount SomeIpSdEntry::optsCount() const {
    return m_podPayload.optsCount;
}

types::sdOptsCount SomeIpSdEntry::optsCount1st() const {
    return m_podPayload.optsCount >> 4;
}

types::sdOptsCount SomeIpSdEntry::optsCount2nd() const {
    return m_podPayload.optsCount & ~(0xF << 4);
}

types::sdServiceId SomeIpSdEntry::serviceId() const {
    return m_podPayload.serviceId;
}

types::sdInstanceId SomeIpSdEntry::instanceId() const {
    return m_podPayload.instanceId;
}

types::sdMajorVersion SomeIpSdEntry::majorVersion() const {
    return static_cast<std::uint8_t>(m_podPayload.majorVersionAndTtl >> 24);
}

std::uint32_t SomeIpSdEntry::ttl() const {
    return m_podPayload.majorVersionAndTtl & ~(0xFF << 24);
}

types::sdMinorVersion SomeIpSdEntry::serviceMinorVersion() const {
    return static_cast<types::sdMinorVersion >(m_podPayload.genericAttribute);
}

types::sdCounter SomeIpSdEntry::eventgroupCounter() const {
    return (m_podPayload.genericAttribute >> 16) & 0xF;
}

types::sdEventGroupId SomeIpSdEntry::eventgroupId() const {
    return static_cast<std::uint16_t>(m_podPayload.genericAttribute);
}

void SomeIpSdEntry::setType(const types::sdEntryType& _val) {
    m_podPayload.type = _val;
}

void SomeIpSdEntry::setIndex1st(const types::sdIndex1st& _val) {
    m_podPayload.index1st = _val;
}

void SomeIpSdEntry::setIndex2nd(const types::sdIndex2nd& _val) {
    m_podPayload.index2nd = _val;
}

void SomeIpSdEntry::setOptsCount(const types::sdOptsCount& _val) {
    m_podPayload.optsCount = _val;
}

void SomeIpSdEntry::setOptsCount1st(const types::sdOptsCount& _val) {
    m_podPayload.optsCount =
        (m_podPayload.optsCount & ~(0xF << 4)) | ((_val & ~(0xF << 4)) << 4);
}

void SomeIpSdEntry::setOptsCount2nd(const types::sdOptsCount& _val) {
    m_podPayload.optsCount =
        (m_podPayload.optsCount & ~(0xF)) | (_val & ~(0xF << 4));
}

void SomeIpSdEntry::setServiceId(const types::sdServiceId& _val) {
    m_podPayload.serviceId = _val;
}

void SomeIpSdEntry::setInstanceId(const types::sdInstanceId& _val) {
    m_podPayload.instanceId = _val;
}

void SomeIpSdEntry::setMajorVersion(const types::sdMajorVersion& _val) {
    m_podPayload.majorVersionAndTtl =
        (m_podPayload.majorVersionAndTtl & ~(0xFF << 24)) | (_val << 24);
}

void SomeIpSdEntry::setTtl(const std::uint32_t& _val) {
    m_podPayload.majorVersionAndTtl =
        (m_podPayload.majorVersionAndTtl & ~(0xFFFFFF)) | (_val & (0xFFFFFF));
}

void SomeIpSdEntry::setServiceMinorVersion(const types::sdMinorVersion _val) {
    m_podPayload.genericAttribute = static_cast<std::uint32_t>(_val);
}

void SomeIpSdEntry::setEventgroupCounter(const types::sdCounter _val) {
    m_podPayload.genericAttribute =
        (m_podPayload.genericAttribute& ~(0xF << 16)) | ((_val & 0xF) << 16);
}

void SomeIpSdEntry::setEventgroupId(const types::sdEventGroupId _val) {
    m_podPayload.genericAttribute =
        (m_podPayload.genericAttribute & ~(0xFFFF)) | _val;
}

void SomeIpSdEntry::print() const {
    // Store original format
    std::ios_base::fmtflags prevFmt(std::cout.flags());

    // Print payload
    std::cout << "[" __FILE__ "]"
        << "\n\tPOD size: " << serialization::PodPayload<SdEntryRaw_t>::size()
        << "\n\t--" << std::hex << std::showbase
        << "\n\tTTL: " << unsigned(ttl())
        << "\n\tMajor Version: " << unsigned(majorVersion())
        << "\n\tInstance Id: " << unsigned(instanceId())
        << "\n\tService Id: " << unsigned(serviceId())
        << "\n\tOptions Count: " << unsigned(optsCount())
        << "\n\tOptions 1st Count: " << unsigned(optsCount1st())
        << "\n\tOptions 2nd Count: " << unsigned(optsCount2nd())
        << "\n\t1st Index: " << unsigned(index1st())
        << "\n\t2nd Index: " << unsigned(index2nd())
        << "\n\tType: " << unsigned(type())
        << "\n\t-- Service Entry Attributes"
        << "\n\tMinor Version: " << unsigned(serviceMinorVersion())
        << "\n\t-- Eventgroup Entry Attributes"
        << "\n\tEventgroup ID: " << unsigned(eventgroupId())
        << "\n\tCounter: " << unsigned(eventgroupCounter()) << '\n';

    // Restore original format
    std::cout.flags(prevFmt);
}

} // namespace sd
} // namespace someip
} // namespace vsomeip_utilities
