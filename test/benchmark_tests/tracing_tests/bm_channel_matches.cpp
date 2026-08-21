// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Benchmarks for channel_impl::matches() - measures how the per-channel filter
// list scan scales with the number of configured filters, in isolation from
// connector_impl/DLT/message-formatting overhead (see bm_connector_trace.cpp
// for the full trace() path, including a filter-count scenario in context).
//
// Filters are added through the same public API production code uses
// (channel_impl::add_filter()); the log context is nullptr since matches()
// never touches it.

#include <benchmark/benchmark.h>

#include <cstdint>
#include <memory>

#include <vsomeip/constants.hpp>
#include <vsomeip/primitive_types.hpp>

#include "../../../implementation/tracing/include/channel_impl.hpp"
#include "../../../implementation/tracing/include/enumeration_types.hpp"

namespace {

using vsomeip_v3::instance_t;
using vsomeip_v3::method_t;
using vsomeip_v3::service_t;
using vsomeip_v3::trace::channel_impl;
using vsomeip_v3::trace::filter_type_e;
using vsomeip_v3::trace::match_t;

constexpr service_t QUERY_SERVICE = 0x1234;
constexpr instance_t QUERY_INSTANCE = 0x5678;
constexpr method_t QUERY_METHOD = 0x0abc;

enum class match_position_e { none, first, last };

// Builds a channel with `_filter_count` distinct POSITIVE filters. Depending on
// `_position`, either none of them match (QUERY_SERVICE, QUERY_INSTANCE,
// QUERY_METHOD), the first one does, or the last one does - the middle two
// bracket the cost of a full list scan; `first` is the O(1) baseline.
std::shared_ptr<channel_impl> make_channel(int _filter_count, match_position_e _position) {
    auto channel = std::make_shared<channel_impl>("TC", nullptr);
    for (int i = 0; i < _filter_count; ++i) {
        const bool use_matching =
                (_position == match_position_e::first && i == 0) || (_position == match_position_e::last && i == _filter_count - 1);
        const match_t match = use_matching ? match_t{QUERY_SERVICE, QUERY_INSTANCE, QUERY_METHOD}
                                           : match_t{static_cast<service_t>(0x2000 + i), QUERY_INSTANCE, QUERY_METHOD};
        channel->add_filter(match, filter_type_e::POSITIVE);
    }
    return channel;
}

void run(benchmark::State& state, match_position_e _position) {
    auto channel = make_channel(static_cast<int>(state.range(0)), _position);

    for (auto _ : state) {
        benchmark::DoNotOptimize(channel->matches(QUERY_SERVICE, QUERY_INSTANCE, QUERY_METHOD));
    }
    state.SetItemsProcessed(state.iterations());
}

constexpr int MAX_FILTERS = 256;

// Best case, included as a baseline: the first filter matches, so cost is O(1)
// regardless of how many more filters follow it.
void BM_trace_matches_matched_first(benchmark::State& state) {
    run(state, match_position_e::first);
}
BENCHMARK(BM_trace_matches_matched_first)->Arg(1)->Arg(4)->Arg(16)->Arg(64)->Arg(MAX_FILTERS);

// Worst case for a dropped message: no filter matches, so matches() scans the
// whole list before falling through to DROP (a positive filter exists, so "no
// match" means dropped, not the forward-everything default).
void BM_trace_matches_no_match(benchmark::State& state) {
    run(state, match_position_e::none);
}
BENCHMARK(BM_trace_matches_no_match)->Arg(1)->Arg(4)->Arg(16)->Arg(64)->Arg(MAX_FILTERS);

// Worst case for a message that is still traced: the matching filter is last,
// so matches() scans the whole list before finding it.
void BM_trace_matches_matched_last(benchmark::State& state) {
    run(state, match_position_e::last);
}
BENCHMARK(BM_trace_matches_matched_last)->Arg(1)->Arg(4)->Arg(16)->Arg(64)->Arg(MAX_FILTERS);

} // namespace
