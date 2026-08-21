<!--
Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.
-->

# Benchmark tests

Micro-benchmarks built on [Google Benchmark](https://github.com/google/benchmark).
All source files matching `**/*.cpp` in this directory are compiled into a single
executable, `benchmark_tests_bin`. Registration happens via the `BENCHMARK()`
macros in each file; `main.cpp` provides `BENCHMARK_MAIN()`.

Current suites:

- `security_tests/` — policy / access-control checks.
- `tracing_tests/` — `trace::connector_impl::trace()` performance.

## Building

The benchmark target is `EXCLUDE_FROM_ALL`, so it must be requested explicitly.
`GTEST_ROOT` has to be set because the suite links gtest/gmock.

```bash
export GTEST_ROOT=/usr/src/googletest

# Default (DLT-enabled) build:
cmake -G Ninja -S . -B ../../build/vsomeip-bench \
  -DGTEST_ROOT=/usr/src/googletest \
  -DCMAKE_BUILD_TYPE=Release

cmake --build ../../build/vsomeip-bench --target benchmark_tests_bin
```

Run the two `cmake` commands from the repository root and adjust the paths, or
point `-S` at the `vsomeip` source directory. To build the **non-DLT** (console
logging) variant, add `-DDISABLE_DLT=Y` to the configure step.

The resulting binary is:

```bash
<build-dir>/test/benchmark_tests/benchmark_tests_bin
```

## Running

```bash
<build-dir>/test/benchmark_tests/benchmark_tests_bin \
  --benchmark_filter='BM_' \
  --benchmark_min_time=0.02s \
  2>/dev/null
```

- `--benchmark_filter=<regex>` selects benchmarks by name; drop it to run all.
- `--benchmark_min_time=0.02s` keeps a run short for a quick check; omit it for
  statistically stable numbers.
- `2>/dev/null` suppresses the benign DLT FIFO warnings printed in DLT builds
  when no DLT daemon is running.

Useful options:

- `--benchmark_repetitions=5 --benchmark_report_aggregates_only=true` — report
  mean / median / stddev across repetitions.
- `--benchmark_out=results.json --benchmark_out_format=json` — write results to
  a file, e.g. for comparison with `compare.py` (see below).

## Comparing before/after

Eyeballing two runs is unreliable — machine noise easily produces double-digit
percent swings between runs of identical code. For a real "did this help"
answer, use Google Benchmark's `compare.py` (available on `PATH` in this
devcontainer).

1. Build both revisions with **identical** cmake flags (e.g. into two
   `git worktree` checkouts) — comparing different build types/flags is
   meaningless.
2. Run the same filter against both binaries with enough repetitions for a
   meaningful statistical test (`compare.py`'s U test needs at least 9):

   ```bash
   <before-build>/.../benchmark_tests_bin --benchmark_filter='BM_trace' \
     --benchmark_repetitions=10 --benchmark_out=before.json --benchmark_out_format=json
   <after-build>/.../benchmark_tests_bin --benchmark_filter='BM_trace' \
     --benchmark_repetitions=10 --benchmark_out=after.json --benchmark_out_format=json
   ```

3. Compare:

   ```bash
   compare.py benchmarks before.json after.json
   ```

Reading the output: each benchmark gets one row per statistic (mean, median,
stddev, cv) plus a `_pvalue` row for the Mann-Whitney U test. Time/CPU columns
are the *fractional* change (e.g. `-0.93` ≈ 93% reduction, roughly 14x faster;
positive means slower). Treat a delta as noise, not a real effect, unless its
`_pvalue` is below ~0.05 — a large-looking delta with a high p-value (as seen
below for a single thread) is not evidence of anything.

For threaded benchmarks, "Time" (wall-clock, via `UseRealTime()`, as ours use)
is the metric lock-contention removal improves. "CPU" is *not* summed across
threads by default — Google Benchmark only measures the main thread's CPU time
unless the benchmark registers `->MeasureProcessCPUTime()` (ours don't), so for
a threaded benchmark its behavior is not a reliable per-message-cost signal.
Read "Time" for concurrency questions; ignore "CPU" here.

Also build both revisions into **separate, clean build directories** — e.g. via
`git worktree add` — rather than checking out in place and doing an incremental
rebuild. An incremental in-place rebuild risks stale objects or a missed
rebuild step silently mixing old and new code into one of the two results,
which quietly shrinks or hides the effect you're trying to measure.

Worked example (comparing the lock-removal refactor, non-DLT, 10 repetitions,
two clean build directories):

```
Benchmark                                                Time             CPU
------------------------------------------------------------------------------
BM_trace_small_concurrent/real_time/threads:1_pvalue    0.6776          0.6776  (not significant)
BM_trace_small_concurrent/real_time/threads:1_mean     -0.0052         -0.0052  (no real change)
BM_trace_small_concurrent/real_time/threads:8_pvalue    0.0002          0.0002  (highly significant)
BM_trace_small_concurrent/real_time/threads:8_mean     -0.9379         -0.6707  (~94% less wall time)
```

Single-threaded shows no significant change (expected: an uncontended mutex is
cheap), while 8 threads shows a large, statistically significant improvement —
exactly what removing lock contention should produce.
