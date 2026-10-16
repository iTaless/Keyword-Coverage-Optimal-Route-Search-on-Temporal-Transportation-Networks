# Keyword-Coverage Optimal Route Search on Temporal Transportation Networks

C++17 implementations of exact A*, the AP heuristic and a custom time-expanded BL baseline. All three methods read the same processed-second network and fixed query row, including its query-level dwell threshold.

## Build and test

```bash
make
make test
python3 scripts/check_release_consistency.py
```

Python 3.9+ and a C++17 compiler are required. Install `requirements.txt` for NumPy validation and optional plotting. CMake configuration is also included.

## Reproduce

```bash
python3 scripts/run_seconds_study.py --limit 2 --out results/local_pilot/pilot.jsonl
python3 scripts/run_seconds_study.py --out results/local_main_study/main_study.jsonl
python3 scripts/recheck_aap.py --workers 4 --timeout 300 --memory-mib 1536 --out-dir results/local_aap
python3 scripts/validate_seconds_instances.py
```

The canonical input is `queries/final_queries_1260.csv`: 1,200 sensitivity rows plus 60 default-setting rows. The main-study runner selects the 1,200 sensitivity rows, specifying 3,600 attempts across BL/A*/AP. This protocol is separate from the preserved historical timing archive; a complete new formal three-method timing run is not claimed.

All current search time arithmetic uses elapsed seconds despite legacy `*_min` column names. AP uses rho=8, L=20 and J=k. The external 300-second process timeout includes input loading; reported query time is preprocessing plus search, excluding loading. The separate A*/AP verification used four workers and a 1,536-MiB per-process virtual-address-space limit. Resource-limited exact runs are excluded from positive-reference quality comparisons.

## Contents

| Directory | Contents |
| --- | --- |
| `src/`, `baseline_bl/` | Current algorithms and BL fixed-query entry point |
| `data/raw/`, `data/processed/` | Released network snapshots and deterministic conversion inputs |
| `queries/` | Fixed query rows and time-unit information |
| `results/rechecked_aap/` | 2,520 attempts, 2,080 returned routes, independent replay and 110 quality pairs |
| `validation/seconds_instances/` | 20 tractable instances, all three methods and 200 exhaustive rank comparisons |
| `results/final/` | 3,600 historical timing records, original workbooks, archived routes and supplementary BL evidence |
| `validation/legacy_raw_clock/` | Separately labelled earlier numeric-clock validation |
| `scripts/`, `tests/`, `examples/` | Runners, conversion, validation, regression tests and toy inputs |
| `docs/` | Experimental protocol, data definitions, provenance and evidence limitations |

Paper figure exports and internal manuscript/review working notes are not included. Runtime plots can be regenerated from the retained measurements:

```bash
python3 scripts/plot_results.py --runtime results/final/main_study.runtime.csv --out-dir results/local_figures
```

See [STATUS.md](STATUS.md), [the experimental protocol](docs/EXPERIMENT_PROTOCOL.md), [data dictionary](docs/DATA_DICTIONARY.md), [data provenance](docs/DATA_PROVENANCE.md), and [evidence limitations](docs/KNOWN_ISSUES.md).

Historical times and current verification are separate evidence sources. The historical executable/input lineage and intended query clocks remain unresolved; those measurements do not establish a current-release speedup. Public archival JSON records omit machine-specific platform strings; execution limits, times, statuses, routes and scores are retained.

Repository: https://github.com/iTaless/Keyword-Coverage-Optimal-Route-Search-on-Temporal-Transportation-Networks

GTFS validation software: https://github.com/MobilityData/gtfs-validator
