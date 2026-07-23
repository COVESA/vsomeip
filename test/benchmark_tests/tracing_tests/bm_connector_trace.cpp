// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Benchmarks for trace::connector_impl::trace().
//
// The benchmarks target two properties:
//   * memory allocation while building the log message - covered by the
//     per-size benchmark, which formats payloads from a few bytes up to the
//     largest size the connector actually emits.
//   * lock contention between concurrent producers - covered by the threaded
//     benchmarks, which run the same connector from an increasing number of
//     threads. If a lock serializes the producers, the aggregate items/s stops
//     scaling with the thread count.
//
// In non-DLT builds trace() writes the formatted message to std::cout via the
// vsomeip logger. To measure message building rather than terminal throughput
// (and to avoid flooding it), std::cout is detached from stdio and redirected
// into a stateless discarding buffer for the duration of a benchmark. The full
// message is still built and streamed, so the allocation cost is preserved.

#include <benchmark/benchmark.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <iostream>
#include <memory>
#include <random>
#include <streambuf>
#include <vector>

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/primitive_types.hpp>

#include "../../../implementation/tracing/include/connector_impl.hpp"
#include "../../../implementation/configuration/include/trace.hpp"
#include "../../../implementation/configuration/include/configuration_impl.hpp"
#include "../../../implementation/logger/include/logger_impl.hpp"

#ifdef USE_DLT
#include <dlt/dlt.h>
#endif

namespace {

using vsomeip_v3::byte_t;
using vsomeip_v3::instance_t;
using vsomeip_v3::method_t;
using vsomeip_v3::service_t;

constexpr std::uint16_t TRACE_HEADER_SIZE = 10; // VSOMEIP_TRACE_HEADER_SIZE

// The connector clips the traced payload to this many bytes (see trace()), so a
// ~100 KB input message is formatted up to this size only.
constexpr std::size_t MAX_TRACED_PAYLOAD = 0xffff;

// A discarding stream buffer. It is stateless, so redirecting std::cout into it
// keeps concurrent logging cheap and free of contention of its own.
class null_streambuf : public std::streambuf {
protected:
    std::streamsize xsputn(const char*, std::streamsize _n) override { return _n; }
    int overflow(int _c) override { return _c; }
};

// Shared state, set up by thread 0 before the timed region and torn down after
// it (see connector_setup / connector_teardown). Google Benchmark synchronizes
// all threads around the timed loop, so the non-thread-0 threads observe the
// fully initialized state.
std::shared_ptr<vsomeip_v3::trace::connector_impl> g_connector;
std::array<byte_t, TRACE_HEADER_SIZE> g_header;
null_streambuf g_null_sink;
std::streambuf* g_saved_cout = nullptr;

// Builds a SOME/IP message of the requested total size (at least the SOME/IP
// header). The payload beyond the header is left zero-initialized.
std::vector<byte_t> make_message(std::size_t _total_size, service_t _service, method_t _method) {
    std::vector<byte_t> data(std::max<std::size_t>(_total_size, VSOMEIP_FULL_HEADER_SIZE), 0x00);

    data[VSOMEIP_SERVICE_POS_MIN] = static_cast<byte_t>(_service >> 8);
    data[VSOMEIP_SERVICE_POS_MAX] = static_cast<byte_t>(_service & 0xff);
    data[VSOMEIP_METHOD_POS_MIN] = static_cast<byte_t>(_method >> 8);
    data[VSOMEIP_METHOD_POS_MAX] = static_cast<byte_t>(_method & 0xff);
    data[VSOMEIP_PROTOCOL_VERSION_POS] = 0x01;
    data[VSOMEIP_INTERFACE_VERSION_POS] = 0x01;

    return data;
}

std::array<byte_t, TRACE_HEADER_SIZE> make_header(instance_t _instance) {
    std::array<byte_t, TRACE_HEADER_SIZE> header{};
    header[VSOMEIP_TC_INSTANCE_POS_MIN] = static_cast<byte_t>(_instance >> 8);
    header[VSOMEIP_TC_INSTANCE_POS_MAX] = static_cast<byte_t>(_instance & 0xff);
    return header;
}

std::size_t traced_bytes(std::size_t _message_size) {
    return std::min<std::size_t>(_message_size, MAX_TRACED_PAYLOAD);
}

// Creates and enables a single shared connector with full-payload logging, so
// large messages actually exercise the message-building path instead of being
// truncated by the default size threshold.
void connector_setup() {
    // Route logging away from the terminal without skipping the work: detach
    // std::cout from stdio (removes the per-write stdio lock) and discard bytes.
    std::ios_base::sync_with_stdio(false);
    g_saved_cout = std::cout.rdbuf(&g_null_sink);

    auto configuration = std::make_shared<vsomeip_v3::cfg::configuration_impl>("");
    vsomeip_v3::logger::logger_impl::init(configuration);

#ifdef USE_DLT
    DLT_REGISTER_APP("VSIP", "vSomeIP tracing benchmark");
#endif

    g_connector = std::make_shared<vsomeip_v3::trace::connector_impl>();

    auto trace_config = std::make_shared<vsomeip_v3::cfg::trace>();
    trace_config->is_enabled_ = true;
    trace_config->is_sd_enabled_ = true;
    trace_config->full_logging_threshold_ = 0; // 0 == no threshold, always full
    g_connector->configure(trace_config);

    g_header = make_header(0x0001);
}

void connector_teardown() {
    g_connector.reset();

#ifdef USE_DLT
    DLT_UNREGISTER_APP();
#endif

    if (g_saved_cout) {
        std::cout.rdbuf(g_saved_cout);
        g_saved_cout = nullptr;
    }
}

void trace_once(const std::vector<byte_t>& _message) {
    g_connector->trace(g_header.data(), static_cast<std::uint16_t>(g_header.size()), _message.data(),
                       static_cast<std::uint32_t>(_message.size()));
}

// ---------------------------------------------------------------------------
// Single message size, single thread: isolates the per-message building and
// allocation cost across the whole payload size range.
// ---------------------------------------------------------------------------
void BM_trace_by_size(benchmark::State& state) {
    const auto message = make_message(static_cast<std::size_t>(state.range(0)), 0x1234, 0x5678);

    connector_setup();

    for (auto _ : state) {
        trace_once(message);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(traced_bytes(message.size())));

    connector_teardown();
}

BENCHMARK(BM_trace_by_size)
        ->Arg(16) // header only
        ->Arg(64) // typical small message
        ->Arg(256)
        ->Arg(1024)
        ->Arg(8192)
        ->Arg(65536) // ~64 KB, around the largest size the connector emits
        ->Arg(102400); // ~100 KB input (clipped to ~64 KB when traced)

// ---------------------------------------------------------------------------
// Small message, increasing thread count: exposes lock contention.
// ---------------------------------------------------------------------------
void BM_trace_small_concurrent(benchmark::State& state) {
    const auto message = make_message(64, 0x1234, 0x5678);

    if (state.thread_index() == 0) {
        connector_setup();
    }

    for (auto _ : state) {
        trace_once(message);
    }

    state.SetItemsProcessed(state.iterations());

    if (state.thread_index() == 0) {
        connector_teardown();
    }
}

BENCHMARK(BM_trace_small_concurrent)->ThreadRange(1, 64)->UseRealTime();

// ---------------------------------------------------------------------------
// Standard use case: Highly concurrent logging of a realistic
// size mix (mostly small, occasionally medium, rarely ~100 KB) with an
// increasing thread count. Combines the allocation and contention effects.
// ---------------------------------------------------------------------------
const std::vector<std::vector<byte_t>>& mixed_workload() {
    static const std::vector<std::vector<byte_t>> workload = [] {
        std::vector<std::vector<byte_t>> messages;
        messages.reserve(100);
        for (int i = 0; i < 100; ++i) {
            std::size_t size = 64; // 90% small
            if (i >= 98) {
                size = 102400; // 2% large (~100 KB)
            } else if (i >= 90) {
                size = 1024; // 8% medium
            }
            messages.push_back(make_message(size, static_cast<service_t>(0x1000 + i), 0x5678));
        }
        // Shuffle once, before timing, with a fixed seed. This breaks up the
        // large-message adjacency (which would otherwise let the allocator reuse
        // the just-freed buffer and flatter the large path) while keeping the
        // size distribution and the run-to-run results deterministic.
        std::mt19937 rng{0xc0ffee};
        std::shuffle(messages.begin(), messages.end(), rng);
        return messages;
    }();
    return workload;
}

void BM_trace_mixed_concurrent(benchmark::State& state) {
    const auto& workload = mixed_workload();

    if (state.thread_index() == 0) {
        connector_setup();
    }

    // Decorrelate the threads' starting points so they do not all hit the same
    // (large) message at the same time.
    std::size_t index = static_cast<std::size_t>(state.thread_index());
    std::int64_t bytes = 0;
    for (auto _ : state) {
        const auto& message = workload[index % workload.size()];
        trace_once(message);
        bytes += static_cast<std::int64_t>(traced_bytes(message.size()));
        ++index;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(bytes);

    if (state.thread_index() == 0) {
        connector_teardown();
    }
}

BENCHMARK(BM_trace_mixed_concurrent)->ThreadRange(1, 64)->UseRealTime();

} // namespace
