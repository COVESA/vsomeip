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
  a file (e.g. for before/after comparison with Google Benchmark's `compare.py`).
