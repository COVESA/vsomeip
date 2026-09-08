// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../protocol/include/command_types.hpp"

#include <vsomeip/constants.hpp>
#include <vsomeip/primitive_types.hpp>

#include <algorithm>
#include <vector>
#include <span>
#include <tuple>

namespace vsomeip_v3 {

/**
 * Collection of unique service + instance pairs along the first recorded major + minor version.
 * For legacy reasons no two entries differing only in the major version are allowed.
 **/
class local_service_table {
private:
    struct less {
        [[nodiscard]] bool operator()(protocol::service_data const& _lhs, protocol::service_data const& _rhs) const {
            static constexpr auto tie = [](auto const& _d) { return std::tie(_d.key_.service_, _d.key_.instance_); };
            return tie(_lhs) < tie(_rhs);
        }
    };
    static bool similar(protocol::service_data const& _lhs, protocol::service_data const& _rhs) {
        static constexpr auto tie = [](auto const& _d) { return std::tie(_d.key_.service_, _d.key_.instance_); };
        return tie(_lhs) == tie(_rhs);
    };

public:
    size_t size() const { return services_.size(); }

    protocol::service_data const* find(protocol::service_data const& _data) const {
        auto it = find_service(_data);
        if (it == services_.end()) {
            return nullptr;
        }
        auto itE = find_next_service(it, _data.key_.service_);
        it = find_instance(it, itE, _data.key_.instance_);
        // TODO this should be wrong as soon as one client can requests multiple major versions
        return it != itE ? &(*it) : nullptr;
    }

    [[nodiscard]] bool contains(protocol::service_data const& _data) const { return find(_data) != nullptr; }

    void insert(protocol::service_data const& _data) {
        auto const it = std::lower_bound(services_.begin(), services_.end(), _data, less{});
        if (it == services_.end()) {
            services_.push_back(_data);
            return;
        }
        if (similar(_data, *it)) {
            return;
        }
        services_.insert(it, _data);
    }

    void take(local_service_table& _in) {
        services_.reserve(services_.size() + _in.services_.size());
        auto it = services_.begin();
        auto const itEnd = _in.services_.end();
        for (auto itNext = _in.services_.begin(); itNext != itEnd; ++itNext) {
            it = std::lower_bound(it, services_.end(), *itNext, less{});
            if (it == services_.end()) {
                services_.insert(it, itNext, itEnd);
                _in.services_.clear();
                return;
            }
            if (similar(*itNext, *it)) {
                ++it;
            } else {
                it = services_.insert(it, *itNext);
                ++it;
            }
        }
        _in.services_.clear();
    }

    bool remove(protocol::service_data const& _data) {
        if (_data.key_.service_ == ANY_SERVICE) {
            services_.clear();
            return true;
        }
        auto it = find_service(_data);
        if (it == services_.end()) {
            return false;
        }
        auto itE = find_next_service(it, _data.key_.service_);
        if (_data.key_.instance_ == ANY_INSTANCE) {
            services_.erase(it, itE);
            return true;
        }
        it = find_instance(it, itE, _data.key_.instance_);
        if (it == itE) {
            return false;
        }
        itE = find_next_instance(it, itE, _data.key_.instance_);
        if (_data.key_.major_ == ANY_MAJOR) {
            services_.erase(it, itE);
            return true;
        }
        it = find_major(it, itE, _data.key_.major_);
        if (it != itE) {
            services_.erase(it);
            return true;
        }
        return false;
    }
    void clear() { services_.clear(); }

    std::span<protocol::service_data const> view() const { return services_; };

private:
    using iterator = std::vector<protocol::service_data>::const_iterator;
    iterator find_service(protocol::service_data const& _data) const {
        return std::lower_bound(services_.begin(), services_.end(), _data,
                                [](auto const& _lhs, auto const& _rhs) { return _lhs.key_.service_ < _rhs.key_.service_; });
    }
    iterator find_next_service(iterator _iterator, service_t _service) const {
        return std::find_if(_iterator, services_.end(), [&](auto const& _in) { return _service != _in.key_.service_; });
    };
    static iterator find_instance(iterator _itB, iterator _itE, instance_t _instance) {
        return std::find_if(_itB, _itE, [&](auto const& _in) { return _instance == _in.key_.instance_; });
    }
    static iterator find_next_instance(iterator _itB, iterator _itE, instance_t _instance) {
        return std::find_if(_itB, _itE, [&](auto const& _in) { return _instance != _in.key_.instance_; });
    }
    static iterator find_major(iterator _itB, iterator _itE, major_version_t _version) {
        return std::find_if(_itB, _itE, [&](auto const& _in) { return _version == _in.key_.major_; });
    }
    std::vector<protocol::service_data> services_;
};

} // namespace vsomeip_v3
