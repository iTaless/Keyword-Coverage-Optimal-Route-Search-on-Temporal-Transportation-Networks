# Keyword-Coverage Optimal Route Search on Temporal Transportation Networks

The production BL, A* and AP executables use **elapsed seconds**. They read the same processed network and fixed query row. Raw HHMMSS clock values are converted upstream by scripts/convert_austin_raw.py; GTFS HH:MM:SS input is converted by scripts/gtfs_to_trips.py. Search and fresh-run validation perform elapsed-second arithmetic.

## Build and verify

```bash
make
make test
```

CMake is also supported. Tests include the BL unit suite, 45 randomized small-graph comparisons, the three-method fixed-query interface, elapsed-dwell boundary and fractional-budget regressions. Python 3.9+ and a C++17 compiler are required; install `requirements.txt` for plotting and NumPy validation.

## Run common inputs

```bash
python3 scripts/run_seconds_study.py --limit 2 --out results/seconds_unified/pilot.jsonl
python3 scripts/run_seconds_study.py
```

The second command runs the 1,200 main query IDs for BL/A/AP, with separate fresh output under results/seconds_unified/. Each method receives the same query ID, source/target, time window, budget, keywords/weights, k, dwell_seconds and seed. AP uses rho=8, rounds=20, J=k. All temporal numbers read by production code are seconds despite legacy *_min field names. The external 300-second subprocess limit includes loading; reported query time includes per-query preprocessing/search and excludes loading.

## Important distinction between code and historical results

Seconds-based code is unified. The fixed query CSV has NOT been silently modified. Its original values are read as seconds. Input diagnostics in queries/TIME_UNIT.json find 855 of 1,260 query starts after the final released scheduled arrival; such queries are infeasible for those fixed inputs. The intended earlier time-of-day values cannot be recovered automatically: 80 starts and 278 ends are not valid HHMMSS encodings.

The supplied original workstation runtime records and recovered paths remain under results/final/ as labelled historical evidence. They do not become validated output from the unified seconds code. Archived paths use raw numeric HHMMSS clocks, and the original A/AP archive has 1,146 recorded score differences. See docs/KNOWN_ISSUES.md. No historical time, route or quality ratio has been relabelled to conceal these differences.

## Outputs

- results/rechecked_aap/: 2,520 actual new A*/AP attempts on all 1,260 unchanged query rows, independently replayed for scoring. Resource limits and input interpretation are documented separately.
- validation/seconds_instances/: 20 current city subgraphs; 200 A*/BL ranks match independent exhaustive enumeration; AP returned routes pass validation.
- results/seconds_unified/: six-run current pipeline pilot; the full three-method formal runtime study has not been repeated on the author workstation.
- results/final/: supplied formal runtime archive and separately sourced BL recovery evidence, with clock/score audit.
- docs/UPLOAD_REVIEW_zh.md: current review and next-input requirements.
- SECONDS_UNIFIED_zh.md: Chinese explanation and running instructions.

## Implementation mapping

| Component | Implementation |
|---|---|
| Algorithms 1--4 / A* and AP | src/main.cpp |
| City-scale BL | baseline_bl/fixed_query_runner.cpp; bl_time_expanded.hpp |
| Common experiment runner | scripts/run_experiments.py |
| Seconds-only convenience command | scripts/run_seconds_study.py |
| Independent fresh-run validation | scripts/verify_results.py |
| BL-only seconds recovery and validation | scripts/run_bl_shard.py; verify_bl_shard.py |
| Historical archive audit | scripts/audit_archived_routes.py |

BL production input is --stations/--trips, the same as A*/AP. It no longer offers a raw-clock arithmetic mode. The old recovery adapter is retained only as historical source under results/final/source/bl_recovery_source/. Current exhaustive validation is under validation/seconds_instances/. Earlier numeric-clock small-instance evidence is explicitly archived under validation/legacy_raw_clock/.

Repository: https://github.com/iTaless/Keyword-Coverage-Optimal-Route-Search-on-Temporal-Transportation-Networks

GTFS validation software (not a dataset feed): https://github.com/MobilityData/gtfs-validator

## Submission status

The authoritative implementation and experimental-evidence status is summarized in [STATUS.md](STATUS.md).

See docs/PAPER_CHANGES_zh.md and docs/FINAL_AUDIT_zh.md. Current output scoring has been checked; the original formal source/input lineage remains unresolved. The package is ready to inspect and upload as a transparently labelled artifact, but does not certify all empirical claims in the attached manuscript.

The manuscript reviewed for this release is mathematics-4569987-v1(20261015-231518).pdf. See docs/PAPER_PACKAGE_CURRENT_CHECK_zh.md for the current paper/package comparison; earlier review notes are explicitly stored under docs/history/.
