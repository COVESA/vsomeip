// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <map>

#include "command.hpp"

namespace vsomeip_v3::protocol {

/**
 * A command for sharing configurations between peers.
 *
 * It contains a list of arbitrary key-value pairs that represent additional
 * information that is relevant to the peer, but is not covered by the other
 * commands in the protocol.
 *
 * See the vsomeip protocol documentation for more information on how this command
 * is structured.
 */
class config_command final : public command {
public:
    /** Creates a new `config_command`. */
    config_command() : command(id_e::CONFIG_ID) { }

    /**
     * Serializes the `config_command` into the given buffer.
     *
     * Serialized data will be represented in Little-Endian byte order.
     */
    void serialize(std::vector<byte_t>& _buffer) const override;

    /**
     * Deserializes the `config_command` from the given buffer.
     *
     * Serialized data is expected to be in Little-Endian byte order.
     */
    void deserialize(const std::vector<byte_t>& _buffer, error_e& _error) override;

    /** Inserts a configuration with the given value at the given key. */
    void insert(const std::string& _key, const std::string&& _value);

    /** Whether the map contains the given configuration. */
    bool contains(const std::string& _key) const;

    /**
     * Returns the value of the given configuration.
     *
     * Panics if the key does not exist.
     */
    const std::string& at(const std::string& _key) const;

    /** Returns a map of configurations and their associated values. */
    const std::map<std::string, std::string, std::less<>>& configs() const;

private:
    /** A map of configurations and their associated values. */
    std::map<std::string, std::string, std::less<>> configs_;
};

} // namespace vsomeip_v3::protocol
