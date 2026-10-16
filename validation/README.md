# Current exhaustive validation: elapsed seconds

`seconds_instances/` contains 20 independently checked instances, 10 from each city. `scripts/validate_seconds_instances.py` explicitly converts raw HHMMSS to seconds, constructs a seed path and keeps every trip among the retained vertices within a 90-minute window. Seeds are 20261928 (Austin) and 20262928 (Houston). The generated seed extends by 1–4 edges; vertices are 3–6, budget is 70, k=10, query dwell is sampled from integer 600–1800 seconds, and up to five keywords have equal normalized weights.

Tractability guards reject extractions with more than 100 edges or enumeration exceeding 10,000 routes or 200,000 DFS expansions. These are explicit selection restrictions, not a representative city-performance sample. Actual tractability rejections: Austin 3 edge-count and 2 enumeration-cap; Houston 1 edge-count and 1 enumeration-cap. No instance was rejected because an algorithm disagreed with the oracle.

Observed ranges: 10–96 edges, 15–7,585 feasible routes, dwell 619–1,718 seconds. A* and BL match the independently enumerated complete ordered top-10 edge sequences and scores in all 20 instances (200 ranks). Every AP route passes independent feasibility and scoring checks. Equal scores use lexicographic scheduled-edge IDs. Score tolerance is 1e-8 for oracle comparison.

The enumerator in the script calls the independent Python evaluator in `scripts/verify_results.py`; it does not use the C++ filters, budget profile, priority queue or upper bound. Per-instance CSV inputs, all three algorithm executions and rank-by-rank comparisons are included. Reproduce with `make`, then `python3 scripts/validate_seconds_instances.py` (NumPy required).

The earlier 20-instance/197-rank archive is preserved, explicitly labelled, under `legacy_raw_clock/`. Agreement on those historical numeric graphs did not establish physical time-unit correctness.
