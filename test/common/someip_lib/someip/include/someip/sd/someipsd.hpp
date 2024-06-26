// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __SOMEIPSD_HPP__
#define __SOMEIPSD_HPP__

#include <someip/types/primitives.hpp>
#include <someip/sd/someipsdentries.hpp>
#include <someip/sd/someipsdoptions.hpp>

namespace vsomeip_utilities {
namespace someip {
namespace sd {

class SomeIpSdBuilder;

// Inverted field representation to align with Big-Endian byte order
struct SomeIpSdHeaderRaw_t {
    std::uint8_t reserved[3] { 0, 0, 0 };
    types::sdFlags_t flags { 0 };
};

/**
 * @brief Some/IP SD
 *
 */
class SomeIpSd
    : public serialization::PodPayload<SomeIpSdHeaderRaw_t> {
friend class SomeIpSdBuilder;

public:
    // Copy Ctors
    SomeIpSd(const SomeIpSd&);
    SomeIpSd(const SomeIpSd&&);

    // Assignment operators
    const SomeIpSd& operator=(const SomeIpSd&);
    const SomeIpSd& operator=(const SomeIpSd&&);

    /**
     * @brief Create Fragment Builder
     *
     * @return SomeIpSdBuilder
     */
    static SomeIpSdBuilder create();

    /**
     * @brief Deserialize data from stream
     *
     * @param _stream
     */
    virtual std::size_t deserialize(std::istream& _stream) override;

    /**
     * @brief Serialize data to stream
     *
     * @param _stream
     */
    virtual std::size_t serialize(std::ostream& _stream) const override;

    // Message Attribute Getters
    types::sdFlags_t flags() const;
    bool reboot() const;
    bool unicast() const;
    bool controlFlag() const;
    std::uint32_t reserved() const;

    SomeIpSdEntries& entries();
    SomeIpSdOptions& options();

    // Message Attribute Setters
    void setFlags(const types::sdFlags_t& _flags);
    void setReboot(const bool _val);
    void setUnicast(const bool _val);
    void setControlFlag(const bool _val);
    void setReserved(const std::array<std::uint8_t, 3>);

    /**
     * @brief Print to stdout utility method
     *
     */
    void print() const override;

protected:
    // CTor
    SomeIpSd() = default;

private:
    SomeIpSdEntries m_entries;
    SomeIpSdOptions m_options;
};

/**
 * @brief Builder
 *
 */
class SomeIpSdBuilder {
public:
    // CTor/DTor
    SomeIpSdBuilder() = default;
    virtual ~SomeIpSdBuilder() = default;

    operator SomeIpSd() const;

    // Setter Methods
    SomeIpSdBuilder& flags(const types::sdFlags_t _flags);
    SomeIpSdBuilder& reboot(const bool _val);
    SomeIpSdBuilder& unicast(const bool _val);
    SomeIpSdBuilder& controlFlag(const bool _val);
    SomeIpSdBuilder& pushEntry(SomeIpSdEntry& _entry);
    SomeIpSdBuilder& pushOptions(SomeIpSdOption& _entry);
    SomeIpSdBuilder& pushEntry(SomeIpSdEntry&& _entry);
    SomeIpSdBuilder& pushOption(SomeIpSdOption&& _entry);

private:
    SomeIpSd m_entry;
};

} // sd
} // someip
} // vsomeip_utilities

#endif // __SOMEIPSD_HPP__
