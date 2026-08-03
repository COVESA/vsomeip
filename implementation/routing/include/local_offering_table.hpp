// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

#include <vsomeip/constants.hpp>
#include <vsomeip/primitive_types.hpp>

#include "internal.hpp"

namespace vsomeip_v3 {

class local_offering_table {
public:
    struct entry {
        service_t service;
        instance_t instance;
        major_version_t major;
        minor_version_t minor;
        client_t client;

        bool operator==(const entry&) const = default;

        friend std::ostream& operator<<(std::ostream& _os, const entry& _entry) {
            return _os << "[" << _entry.service << "." << _entry.instance << "." << static_cast<uint32_t>(_entry.major) << "."
                       << _entry.minor << " client=" << _entry.client << "]";
        }
    };

    explicit local_offering_table() { entries_.reserve(100); }

    bool add(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor, client_t _client) {
        auto new_entry = entry{_service, _instance, _major, _minor, _client};
        auto it = lower_bound(_service, _instance, _major);
        if (it != entries_.end() && it->service == _service && it->instance == _instance && it->major == _major) {
            // instance already known -> update entry
            *it = new_entry;
            return false;
        }
        entries_.insert(it, new_entry);
        return true;
    }

    bool remove(service_t _service, instance_t _instance, major_version_t _major) {
        if (_major != ANY_MAJOR) {
            auto it = lower_bound(_service, _instance, _major);
            if (it != entries_.end() && it->service == _service && it->instance == _instance && it->major == _major) {
                entries_.erase(it);
                return true;
            }
            return false;
        }
        // ANY_MAJOR: every entry for this (service, instance) is contiguous because the
        // table is sorted, so erase the whole run in one shot.
        auto [first, last] = instance_range(_service, _instance);
        if (first == last) {
            return false;
        }
        entries_.erase(first, last);
        return true;
    }

    client_t find_client(service_t _service, instance_t _instance, major_version_t _major) const {
        const auto* e = find_entry_ptr(_service, _instance, _major);
        return e ? e->client : static_cast<client_t>(VSOMEIP_ROUTING_CLIENT);
    }

    std::optional<entry> find_entry(service_t _service, instance_t _instance, major_version_t _major) const {
        const auto* e = find_entry_ptr(_service, _instance, _major);
        if (!e) {
            return std::nullopt;
        }
        return *e;
    }

    std::set<client_t> find_clients(service_t _service, instance_t _instance, major_version_t _major) const {
        std::set<client_t> clients;
        auto [first, last] = service_range(_service);
        if (_instance == ANY_INSTANCE) {
            for (auto it = first; it != last; ++it) {
                clients.insert(it->client);
            }
        } else {
            for (auto it = first; it != last; ++it) {
                if (it->instance == _instance && (it->major == _major || _major == ANY_MAJOR)) {
                    clients.insert(it->client);
                    break;
                }
            }
        }
        return clients;
    }

    // Visit every stored entry matching (service, instance, major, minor) using the
    // ANY_*/DEFAULT_* wildcard semantics. The callback returns true to continue the
    // iteration or false to stop early. Concrete service/instance values are resolved
    // via binary search; only ANY_* widens the traversal, so the common point query
    // stays O(log n) without copying the table.
    template<typename Callback>
    void for_each_available(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor,
                            Callback&& _callback) const {
        auto matches_version = [&](const entry& _entry) {
            const bool its_major_ok = _major == ANY_MAJOR || _major == DEFAULT_MAJOR || _major == _entry.major;
            const bool its_minor_ok = _minor == ANY_MINOR || _minor == DEFAULT_MINOR || _minor <= _entry.minor;
            return its_major_ok && its_minor_ok;
        };

        // Returns false to request stopping the outer service iteration.
        auto visit_instances = [&](const_iterator _first, const_iterator _last) {
            if (_instance != ANY_INSTANCE) {
                for (auto it = _first; it != _last; ++it) {
                    if (it->instance == _instance) {
                        return !matches_version(*it) || _callback(*it);
                    }
                }
                return true;
            }
            for (auto it = _first; it != _last; ++it) {
                if (matches_version(*it) && !_callback(*it)) {
                    return false;
                }
            }
            return true;
        };

        if (_service != ANY_SERVICE) {
            auto [first, last] = service_range(_service);
            if (first != last) {
                visit_instances(first, last);
            }
            return;
        }
        for (auto it = entries_.begin(); it != entries_.end();) {
            auto last = it;
            while (last != entries_.end() && last->service == it->service) {
                ++last;
            }
            if (!visit_instances(it, last)) {
                break;
            }
            it = last;
        }
    }

    [[nodiscard]] bool has_available(service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor) const {
        bool its_found = false;
        for_each_available(_service, _instance, _major, _minor, [&its_found](const entry&) {
            its_found = true;
            return false; // stop at the first match
        });
        return its_found;
    }

    bool is_available(service_t _service, instance_t _instance, major_version_t _major) const {
        // Single matching semantics for the whole table: the major-only availability
        // check is the minor-agnostic (ANY_MINOR) case of has_available.
        return has_available(_service, _instance, _major, ANY_MINOR);
    }

    [[nodiscard]] std::vector<entry> remove_all_for_client(client_t _client) {
        auto it =
                std::stable_partition(entries_.begin(), entries_.end(), [_client](auto const& _entry) { return _entry.client != _client; });
        std::vector<entry> removed;
        if (it == entries_.end()) {
            return removed;
        }
        std::copy(it, entries_.end(), std::back_inserter(removed));
        entries_.erase(it, entries_.end());
        return removed;
    }

    [[nodiscard]] std::vector<entry> clear() {
        std::vector<entry> removed = std::move(entries_);
        entries_.clear();
        return removed;
    }

private:
    using const_iterator = std::vector<entry>::const_iterator;

    // Only the non-const mutators (add/remove) need this; entries are ordered by (service, instance, major).
    std::vector<entry>::iterator lower_bound(service_t _service, instance_t _instance, major_version_t _major) {
        return std::lower_bound(
                entries_.begin(), entries_.end(), std::tie(_service, _instance, _major),
                [](const entry& _entry, auto const& _key) { return std::tie(_entry.service, _entry.instance, _entry.major) < _key; });
    }

    std::pair<const_iterator, const_iterator> service_range(service_t _service) const {
        auto first = std::lower_bound(entries_.begin(), entries_.end(), _service,
                                      [](const entry& _entry, service_t _search) { return _entry.service < _search; });
        auto last = std::upper_bound(first, entries_.end(), _service,
                                     [](service_t _search, const entry& _entry) { return _search < _entry.service; });
        return {first, last};
    }

    // Contiguous run of all entries (across majors) for one concrete (service, instance).
    std::pair<const_iterator, const_iterator> instance_range(service_t _service, instance_t _instance) const {
        const auto key = std::make_pair(_service, _instance);
        auto first = std::lower_bound(entries_.begin(), entries_.end(), key,
                                      [](const entry& _entry, const std::pair<service_t, instance_t>& _k) {
                                          return std::make_pair(_entry.service, _entry.instance) < _k;
                                      });
        auto last = std::upper_bound(first, entries_.end(), key, [](const std::pair<service_t, instance_t>& _k, const entry& _entry) {
            return _k < std::make_pair(_entry.service, _entry.instance);
        });
        return {first, last};
    }

    const entry* find_entry_ptr(service_t _service, instance_t _instance, major_version_t _major = ANY_MAJOR) const {
        auto [first, last] = instance_range(_service, _instance);
        if (first == last) {
            return nullptr;
        }
        if (_major == ANY_MAJOR) {
            return &(*first);
        }
        // A single instance carries only a handful of majors, so a linear scan is cheaper and clearer than a second search.
        for (auto it = first; it != last; ++it) {
            if (it->major == _major) {
                return &(*it);
            }
        }
        return nullptr;
    }

    std::vector<entry> entries_;
};
} // namespace vsomeip_v3
