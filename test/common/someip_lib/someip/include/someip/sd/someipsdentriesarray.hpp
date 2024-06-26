// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef __SOMEIPSDENTRIESBASE_HPP__
#define __SOMEIPSDENTRIESBASE_HPP__

#include <vector>

#include <someip/types/primitives.hpp>

#include <serialization/podpayload.hpp>

namespace vsomeip_utilities {
namespace someip {
namespace sd {

// Inverted field representation to align with Big-Endian byte order
struct EntriesLengthRaw_t {
    types::length_t length { 0 };
};

template<typename TEntry>
class SomeIpSdEntriesArray
    : public serialization::PodPayload<EntriesLengthRaw_t> {
public:
    // CTor
    explicit SomeIpSdEntriesArray() {};

    // Copy/Move CTors
    SomeIpSdEntriesArray(const SomeIpSdEntriesArray& _other)
        : serialization::PodPayload<EntriesLengthRaw_t>{ _other }
        , m_entries { _other.m_entries } {}
    SomeIpSdEntriesArray(const SomeIpSdEntriesArray&& _other)
        : serialization::PodPayload<EntriesLengthRaw_t> { _other }
        , m_entries { std::move(_other.m_entries) } {}

    // Copy/Move assignment operators
    const SomeIpSdEntriesArray& operator=(const SomeIpSdEntriesArray& _other) {
        serialization::PodPayload<EntriesLengthRaw_t>::operator=(_other);
        m_entries = _other.m_entries;
        return *this;
    }

    const SomeIpSdEntriesArray& operator=(const SomeIpSdEntriesArray&& _other) {
        serialization::PodPayload<EntriesLengthRaw_t>::operator=(_other);
        m_entries = std::move(_other.m_entries);
        return *this;
    }

    /**
     * @brief The Serializable object instance actual size
     *
     * @return std::size_t
     */
    virtual std::size_t actualSize() const override {
        return serialization::PodPayload<EntriesLengthRaw_t>::actualSize() + m_podPayload.length;
    }

    /**
     * @brief Deserialize data from stream
     *
     * @param _stream
     */
    virtual std::size_t deserialize(std::istream& _stream) override {
        std::size_t headerLen = serialization::PodPayload<EntriesLengthRaw_t>::deserialize(_stream);
        std::size_t entriesLen = 0;

        while (entriesLen < length()) {
            TEntry entry = TEntry::create();
            std::size_t len = entry.deserialize(_stream);

            // Stop parsing if unnable to deserialize an entry
            if (len == 0)
                break;

            m_entries.push_back(std::move(entry));

            entriesLen += len;
        }

        std::size_t totalLen = headerLen + entriesLen;

        if (totalLen != actualSize()) {
            std::cerr << __func__ << ": Failed to deserialize!\n";
            totalLen = 0;
        }

        return totalLen;
    }

    /**
     * @brief Serialize data to stream
     *
     * @param _stream
     */
    virtual std::size_t serialize(std::ostream& _stream) const override {
        std::size_t ret = 0;

        ret = serialization::PodPayload<EntriesLengthRaw_t>::serialize(_stream);

        for (std::uint32_t i = 0; i < m_entries.size(); ++i) {
            ret += m_entries.at(i).serialize(_stream);
        }

        if (ret != actualSize())
            std::cerr << __func__ << ": Failed to serialize!\n";

        return ret;
    }

    // Getters
    types::length_t length() const { return m_podPayload.length; }

    // Entries management

    /**
     * @brief Access entries
     *
     * @return std::vector<TEntry>& Referent to all entries
     */
    std::vector<TEntry>& get() {
        return m_entries;
    }

    /**
     * @brief Access entries by index
     *
     * @param _index
     * @return TEntry& Referent to the entry at the provided index
     */
    TEntry& operator[](const std::uint32_t _index) {
        return m_entries[_index];
    }

    /**
     * @brief Returns the number of stored entries
     *
     * @return uint32_t
     */
    std::uint32_t entriesCount() const {
        return static_cast<std::uint32_t>(m_entries.size());
    }

    /**
     * @brief Pushes a copy of the provided entry into the entry array
     * NOTE: To keep the payload's length consistency, once an entry is pushed into the array it cannot be modified
     *
     * @param _entry
     */
    void pushEntry(TEntry& _entry) {
        m_podPayload.length += static_cast<std::uint32_t>(_entry.actualSize());
        m_entries.push_back(_entry);
    }

    /**
     * @brief Moves an entry into the entry array
     * NOTE: To keep the payload's length consistency, once an entry is pushed into the array it cannot be modified
     *
     * @param _entry
     */
    void pushEntry(TEntry&& _entry) {
        m_podPayload.length += static_cast<std::uint32_t>(_entry.actualSize());
        m_entries.push_back(_entry);
    }

    /**
     * @brief Forces the length of the entry array to a given value.
     * NOTE: This should only be used to test MALFORMED messages!
     *
     * @param _length
     */
    void forceEntryArraySize(types::length_t _length) {
        m_podPayload.length = _length;
    }

    /**
     * @brief Print to stdout utility method
     *
     */
    void print() const override {
        // Store original format
        std::ios_base::fmtflags prevFmt(std::cout.flags());

        // Print payload
        std::cout << "[" __FILE__ "]"
            << "\n\tPOD size: " << serialization::PodPayload<EntriesLengthRaw_t>::size()
            << "\n\t--\n\tlength: " << length()
            << "\n\tEntries count: " << entriesCount() << '\n';

        // Restore original format
        std::cout.flags(prevFmt);

        for (std::uint32_t i = 0; i < m_entries.size(); ++i) {
            std::cout << "[Entry " << i << "]\n";
            m_entries[i].print();
        }

    }

private:
    std::vector<TEntry> m_entries;
};

} // sd
} // someip
} // vsomeip_utilities

#endif // __SOMEIPSDENTRIESBASE_HPP__
