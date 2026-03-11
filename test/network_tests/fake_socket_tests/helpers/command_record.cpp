// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "command_record.hpp"

#include "to_string.hpp"
namespace vsomeip_v3::testing {

std::shared_ptr<command_record> command_record::create() {
    return std::make_shared<command_record>(hidden{});
}

command_record::command_record(hidden) { }

bool command_record::operator==(std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>> const& _rhs) const {
    std::scoped_lock lock{mtx_};
    if (_rhs.size() != record_.size()) {
        return false;
    }

    for (unsigned int i = 0; i < record_.size(); ++i) {
        if (record_[i].first != _rhs[i].first || record_[i].second.id_ != _rhs[i].second) {
            return false;
        }
    }
    return true;
}

vsomeip_command_handler command_record::create_collector(std::string _sender_name) {
    return [weak_self = weak_from_this(), _sender_name](auto _msg) {
        if (auto self = weak_self.lock(); self) {
            self->add(_sender_name, _msg);
        }
        // never drop any message
        return false;
    };
}

void command_record::add(std::string const& _sender_name, command_message _msg) {
    std::scoped_lock lock{mtx_};
    record_.push_back(std::make_pair(_sender_name, std::move(_msg)));
}

command_message command_record::get_last_msg() {
    std::scoped_lock lock{mtx_};
    return record_.back().second;
}

std::ostream& operator<<(std::ostream& _out, command_record const& _record) {
    std::scoped_lock lock{_record.mtx_};
    return _out << to_string(_record.record_);
}

bool operator==(std::vector<std::pair<std::string, vsomeip_v3::protocol::id_e>> const& _lhs, command_record const& _rhs) {
    return _rhs == _lhs;
}

}
