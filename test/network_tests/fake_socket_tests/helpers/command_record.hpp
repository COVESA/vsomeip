// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "command_message.hpp"
#include "vsomeip_command_handler.hpp"

#include <iostream>
#include <memory>
#include <optional>

namespace vsomeip_v3::testing {

/**
 * Helper to ease the collection of commands and the comparison with expected sequences
 **/
class command_record : public std::enable_shared_from_this<command_record> {
    struct hidden { };

public:
    static std::shared_ptr<command_record> create();
    command_record(hidden);

    [[nodiscard]] bool operator==(std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>> const& _rhs) const;

    /**
     * creates a non-message-dropping command collector for this record
     **/
    vsomeip_command_handler create_collector(std::string _sender_name);

    /**
     * add a message to the record
     **/
    void add(std::string const& _sender_name, command_message _msg);

    /**
     * get the last message received on the recorder
     **/
    command_message get_last_msg();

    /**
     * get the last recorded message with the given command id, or nullopt if none was recorded.
     * Use together with command_payload::get_if<T>() to assert on individual payload fields.
     **/
    [[nodiscard]] std::optional<command_message> find_last(vsomeip_v3::protocol::id_e _id) const;

    /**
     * number of recorded messages with the given command id
     **/
    [[nodiscard]] size_t count(vsomeip_v3::protocol::id_e _id) const;

    /**
     * the record as the (sender, command id) sequence the equality operator compares against.
     * Use it to assert on the relative order of two commands.
     **/
    [[nodiscard]] std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>> as_sequence() const;

    /**
     * user friendly output
     **/
    friend std::ostream& operator<<(std::ostream& _out, command_record const& _record);

private:
    std::mutex mutable mtx_;
    std::vector<std::pair<std::string, command_message>> record_;
};

/**
 * Ensure ordering of lhs and rhs does not matter
 **/
bool operator==(std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>> const& _lhs, command_record const& _rhs);

}
