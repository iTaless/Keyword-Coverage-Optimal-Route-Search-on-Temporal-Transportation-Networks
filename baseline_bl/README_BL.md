# BL baseline

The production BL executable is built from `fixed_query_runner.cpp`. It transforms the temporal network into a static time-expanded graph and performs exact top-k search over distinct scheduled-edge sequences.

Unlike the earlier development driver, the released entry point does not create queries. It reads the same processed station/trip tables and the same fixed query row used by A* and AP.

## Input contract

```text
--stations  processed station/keyword CSV
--trips     processed scheduled-edge CSV
--queries   fixed query CSV
--query-id  exact stable query identifier
--algorithm BL
--timeout-ms 300000
```

The selected row supplies source, target, time window, budget, keywords, normalized weights, requested `k`, seed, and the single query-level `dwell_seconds` threshold. Station-level dwell columns in processed data are ignored by the fixed-query BL runner.

## Output contract

The executable writes one JSON object. Its fields include execution status, loading time, BL transformation/search time, transformed-graph size, dwell threshold, seed, and complete returned routes. Each solution contains the stable processed edge sequence, score, budget, and arrival time.

The unified Python driver converts these records to the common runtime and route CSV schemas and independently validates every completed route.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

See `docs/BL_FIXED_QUERY_PROTOCOL.md` for the complete city-scale command.

## Seconds-only production interface

The BL executable uses --stations and --trips, matching A*/AP. All temporal numbers are elapsed seconds. HHMMSS input is converted upstream; direct raw-clock arithmetic mode is not available. The actual earlier recovery adapter remains historical source under results/final/source/bl_recovery_source/. It is not the production entry point.
