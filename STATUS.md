# Implementation and experimental evidence status

The production BL entry point is baseline_bl/fixed_query_runner.cpp. It is invoked by scripts/run_experiments.py and reads the same processed-second network and fixed query row as A* and AP through --queries and --query-id. It does not generate queries. All three methods use the query-level dwell_seconds threshold and common JSONL/runtime/route output schemas.

The authoritative file is queries/final_queries_1260.csv. The --main-study-only configuration selects 1,200 sensitivity queries and excludes 60 default-setting queries, specifying 3,600 execution attempts across BL/A/AP. This describes the current protocol, not a claim that a complete formal three-method rerun has been performed.

The author-supplied historical runtime archive contains 3,600 entries (1,200 per method), including 584 timeouts. These records are preserved under results/final/ and source/. Current source and input files do not establish the producer/input lineage of those historical measurements. They are not labelled as verified reruns of the corrected release.

Current verification is separate: results/rechecked_aap/ contains 2,520 A*/AP attempts on all 1,260 fixed rows; validation/seconds_instances/ contains 20 tractable instances and 200 ranks checked against independent enumeration for A*/BL. A*/AP returned-route verification and BL small-instance checks are not a substitute for a complete city-scale formal timing rerun.

See baseline_bl/STATUS.md, docs/EXPERIMENT_PROTOCOL.md and docs/KNOWN_ISSUES.md for the implementation and evidence scope. Historical source adapters remain explicitly labelled under results/final/source/.
