// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __SOMEIPSDENTRY_HPP__
#define __SOMEIPSDENTRY_HPP__

#include <someip/types/primitives.hpp>

#include <serialization/podpayload.hpp>

namespace vsomeip_utilities {
namespace someip {
namespace sd {

class SomeIpSdEntryBuilder;

// Inverted field representation to align with Big-Endian byte order
struct SdEntryRaw_t {
    std::uint32_t genericAttribute = { 0 };
    // std::uint8_t ttl[3];
    // types::sdMajorVersion majorVersion = { 0 };
    std::uint32_t majorVersionAndTtl = { 0 };
    types::sdInstanceId instanceId = { 0 };
    types::sdServiceId serviceId = { 0 };
    types::sdOptsCount optsCount = { 0 };
    types::sdIndex2nd index2nd = { 0 };
    types::sdIndex1st index1st = { 0 };
    types::sdEntryType type = { 0 };
};

/**
 * @brief SomeIpSdEntry
 *
 * Covers the structure for both:
 *      - Service Entry type
 *      - EventGroup Entry type
 */
class SomeIpSdEntry
    : public serialization::PodPayload<SdEntryRaw_t> {
public:
    // CTor
    SomeIpSdEntry() = default;

    /**
     * @brief Create Builder
     *
     * @return SomeIpSdEntryBuilder
     */
    static SomeIpSdEntryBuilder create();

    // Attribute Getters
    types::sdEntryType type() const;
    types::sdIndex1st index1st() const;
    types::sdIndex2nd index2nd() const;
    types::sdOptsCount optsCount() const;
    types::sdOptsCount optsCount1st() const;
    types::sdOptsCount optsCount2nd() const;
    types::sdServiceId serviceId() const;
    types::sdInstanceId instanceId() const;
    types::sdMajorVersion majorVersion() const;
    std::uint32_t ttl() const;

    // Service Entry Type specific attribute Getters
    types::sdMinorVersion serviceMinorVersion() const;

    // Eventgroup Entry Type specific attribute Getters
    types::sdCounter eventgroupCounter() const;
    types::sdEventGroupId eventgroupId() const;

    // Message Attribute Setters
    void setType(const types::sdEntryType& _val);
    void setIndex1st(const types::sdIndex1st& _val);
    void setIndex2nd(const types::sdIndex2nd& _val);
    void setOptsCount(const types::sdOptsCount& _val);
    void setOptsCount1st(const types::sdOptsCount& _val);
    void setOptsCount2nd(const types::sdOptsCount& _val);
    void setServiceId(const types::sdServiceId& _val);
    void setInstanceId(const types::sdInstanceId& _val);
    void setMajorVersion(const types::sdMajorVersion& _val);
    void setTtl(const std::uint32_t& _val); // 24bit attribute, input argument will be truncated

    // Service Entry Type specific attribute Setters
    void setServiceMinorVersion(const types::sdMinorVersion _val);

    // Eventgroup Entry Type specific attribute Setters
    void setEventgroupCounter(const types::sdCounter _val);
    void setEventgroupId(const types::sdEventGroupId _val);

    /**
     * @brief Print to stdout utility method
     *
     */
    void print() const override;
};

/**
 * @brief Entry builder
 *
 */
class SomeIpSdEntryBuilder {
public:
    // CTor/DTor
    SomeIpSdEntryBuilder() = default;
    virtual ~SomeIpSdEntryBuilder() = default;

    operator SomeIpSdEntry() const;

    // Setter Methods
    SomeIpSdEntryBuilder& type(const types::sdEntryType& _val);
    SomeIpSdEntryBuilder& index1st(const types::sdIndex1st& _val);
    SomeIpSdEntryBuilder& index2nd(const types::sdIndex2nd& _val);
    SomeIpSdEntryBuilder& optsCount(const types::sdOptsCount& _val);
    SomeIpSdEntryBuilder& optsCount1st(const types::sdOptsCount& _val);
    SomeIpSdEntryBuilder& optsCount2nd(const types::sdOptsCount& _val);
    SomeIpSdEntryBuilder& serviceId(const types::sdServiceId& _val);
    SomeIpSdEntryBuilder& instanceId(const types::sdInstanceId& _val);
    SomeIpSdEntryBuilder& majorVersion(const types::sdMajorVersion& _val);
    SomeIpSdEntryBuilder& ttl(const std::uint32_t& _val); // 24 bit attribute, input argument will be truncated

    // Service Entry Type specific attribute Setters
    SomeIpSdEntryBuilder& serviceMinorVersion(const types::sdMinorVersion _val);

    // Eventgroup Entry Type specific attribute Setters
    SomeIpSdEntryBuilder& eventgroupCounter(const types::sdCounter _val);
    SomeIpSdEntryBuilder& eventgroupId(const types::sdEventGroupId _val);

private:
    SomeIpSdEntry m_entry;
};

} // sd
} // someip
} // vsomeip_utilities

#endif // __SOMEIPSDENTRY_HPP__
