// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/channel_impl.hpp"

#include <vsomeip/internal/logger.hpp>

namespace vsomeip_v3::trace {

channel_impl::channel_impl(const std::string& _id, DltContext* _log_context) : id_(_id), log_context_(_log_context) { }

std::string channel_impl::get_id() const {
    return id_;
}

DltContext* channel_impl::get_log_context() const {
    return log_context_;
}

void channel_impl::add_filter(const match_t& _match, filter_type_e _type) {
    // Create a filter function
    std::function<bool(service_t, instance_t, method_t)> filter_func;
    if (std::get<0>(_match) != ANY_SERVICE) {
        if (std::get<1>(_match) != ANY_INSTANCE) {
            if (std::get<2>(_match) != ANY_METHOD) {
                filter_func = [_match](service_t _s, instance_t _i, method_t _m) {
                    return (std::get<0>(_match) == _s && std::get<1>(_match) == _i && std::get<2>(_match) == _m);
                };
            } else {
                filter_func = [_match](service_t _s, instance_t _i, method_t) {
                    return (std::get<0>(_match) == _s && std::get<1>(_match) == _i);
                };
            }
        } else {
            if (std::get<2>(_match) != ANY_METHOD) {
                filter_func = [_match](service_t _s, instance_t, method_t _m) {
                    return (std::get<0>(_match) == _s && std::get<1>(_match) == _m);
                };
            } else {
                filter_func = [_match](service_t _s, instance_t, method_t) { return (std::get<0>(_match) == _s); };
            }
        }
    } else {
        if (std::get<1>(_match) != ANY_INSTANCE) {
            if (std::get<2>(_match) != ANY_METHOD) {
                filter_func = [_match](service_t, instance_t _i, method_t _m) {
                    return (std::get<1>(_match) == _i && std::get<2>(_match) == _m);
                };
            } else {
                filter_func = [_match](service_t, instance_t _i, method_t) { return (std::get<1>(_match) == _i); };
            }
        } else {
            if (std::get<2>(_match) != ANY_METHOD) {
                filter_func = [_match](service_t, instance_t, method_t _m) { return (std::get<2>(_match) == _m); };
            } else {
                filter_func = [](service_t, instance_t, method_t) { return true; };
            }
        }
    }

    add_filter_intern(std::move(filter_func), _type);
}

void channel_impl::add_filter(const std::vector<match_t>& _matches, filter_type_e _type) {
    bool has_service(false);
    bool has_instance(false);
    bool has_method(false);

    for (auto m : _matches) {
        if (std::get<0>(m) != ANY_SERVICE) {
            has_service = true;
        }
        if (std::get<1>(m) != ANY_INSTANCE) {
            has_instance = true;
        }
        if (std::get<2>(m) != ANY_METHOD) {
            has_method = true;
        }
    }

    // Create a filter function
    std::function<bool(service_t, instance_t, method_t)> filter_func;
    if (has_service) {
        if (has_instance) {
            if (has_method) {
                filter_func = [_matches](service_t _s, instance_t _i, method_t _m) {
                    for (const auto& m : _matches) {
                        if ((std::get<0>(m) == _s || std::get<0>(m) == ANY_SERVICE)
                            && (std::get<1>(m) == _i || std::get<1>(m) == ANY_INSTANCE)
                            && (std::get<2>(m) == _m || std::get<2>(m) == ANY_METHOD)) {
                            return true;
                        }
                    }
                    return false;
                };
            } else {
                filter_func = [_matches](service_t _s, instance_t _i, method_t) {
                    for (const auto& m : _matches) {
                        if ((std::get<0>(m) == _s || std::get<0>(m) == ANY_SERVICE)
                            && (std::get<1>(m) == _i || std::get<1>(m) == ANY_INSTANCE)) {
                            return true;
                        }
                    }
                    return false;
                };
            }
        } else {
            if (has_method) {
                filter_func = [_matches](service_t _s, instance_t, method_t _m) {
                    for (const auto& m : _matches) {
                        if ((std::get<0>(m) == _s || std::get<0>(m) == ANY_SERVICE)
                            && (std::get<2>(m) == _m || std::get<2>(m) == ANY_METHOD)) {
                            return true;
                        }
                    }
                    return false;
                };
            } else {
                filter_func = [_matches](service_t _s, instance_t, method_t) {
                    for (auto& m : _matches) {
                        if (std::get<0>(m) == _s || std::get<0>(m) == ANY_SERVICE) {
                            return true;
                        }
                    }
                    return false;
                };
            }
        }
    } else {
        if (has_instance) {
            if (has_method) {
                filter_func = [_matches](service_t, instance_t _i, method_t _m) {
                    for (auto& m : _matches) {
                        if ((std::get<1>(m) == _i || std::get<1>(m) == ANY_INSTANCE)
                            && (std::get<2>(m) == _m || std::get<2>(m) == ANY_METHOD)) {
                            return true;
                        }
                    }
                    return false;
                };
            } else {
                filter_func = [_matches](service_t, instance_t _i, method_t) {
                    for (auto& m : _matches) {
                        if (std::get<1>(m) == _i || std::get<1>(m) == ANY_INSTANCE) {
                            return true;
                        }
                    }
                    return false;
                };
            }
        } else {
            if (has_method) {
                filter_func = [_matches](service_t, instance_t, method_t _m) {
                    for (auto& m : _matches) {
                        if (std::get<2>(m) == _m || std::get<2>(m) == ANY_METHOD) {
                            return true;
                        }
                    }
                    return false;
                };
            } else {
                filter_func = [](service_t, instance_t, method_t) { return true; };
            }
        }
    }

    add_filter_intern(std::move(filter_func), _type);
}

void channel_impl::add_filter(const match_t& _from, const match_t& _to, filter_type_e _type) {

    // Check usage of ANY_* which is forbidden here
    if (std::get<0>(_from) == ANY_SERVICE || std::get<1>(_from) == ANY_INSTANCE || std::get<2>(_from) == ANY_METHOD
        || std::get<0>(_to) == ANY_SERVICE || std::get<1>(_to) == ANY_INSTANCE || std::get<2>(_to) == ANY_METHOD) {
        VSOMEIP_ERROR << "Trace filter configuration error: You must not use wildcards in range filters.";
        return;
    }

    std::function<bool(service_t, instance_t, method_t)> its_filter_func = [_from, _to](service_t _s, instance_t _i, method_t _m) {
        return (std::get<0>(_from) <= _s && _s <= std::get<0>(_to) && std::get<1>(_from) <= _i && _i <= std::get<1>(_to)
                && std::get<2>(_from) <= _m && _m <= std::get<2>(_to));
    };

    add_filter_intern(its_filter_func, _type);
}

void channel_impl::add_filter_intern(filter_func_t _func, filter_type_e _type) {
    switch (_type) {
    case filter_type_e::NEGATIVE:
        negative_.emplace_back(std::move(_func));
        break;
    default: // POSITIVE, HEADER_ONLY, FULL_PAYLOAD
        positive_.emplace_back(trace_filter_entry{std::move(_func), _type});
        break;
    }
}

trace_result_e channel_impl::matches(service_t _service, instance_t _instance, method_t _method) {
    // If a negative filter matches --> drop!
    for (const auto& filter : negative_) {
        if (filter(_service, _instance, _method)) {
            return trace_result_e::DROP;
        }
    }

    // If a positive/header-only/full-payload filter matches --> forward!
    // POSITIVE and FULL_PAYLOAD log the full payload (POSITIVE_FILTER),
    // HEADER_ONLY logs the header only.
    bool has_positive(false);
    for (const auto& filter : positive_) {
        if (filter.func(_service, _instance, _method)) {
            return filter.type == filter_type_e::HEADER_ONLY ? trace_result_e::HEADER_ONLY_FILTER : trace_result_e::POSITIVE_FILTER;
        }

        // Only a POSITIVE filter restricts the channel to an allow-list;
        // HEADER_ONLY and FULL_PAYLOAD leave other messages on the default path.
        if (filter.type == filter_type_e::POSITIVE) {
            has_positive = true;
        }
    }

    // If no positive filter is defined --> forward everything (subject to the
    // full-logging threshold).
    if (!has_positive) {
        return trace_result_e::DEFAULT;
    }

    // Default --> Drop!
    return trace_result_e::DROP;
}

} // namespace vsomeip_v3::trace
