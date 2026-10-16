# BL status

The production `fixed_query_runner.cpp` is a custom time-expanded exact route-search implementation using processed seconds and query-level dwell. It reads identical fixed inputs to A*/AP. Unit tests, numerical/dwell boundary tests, and all 20 current small-instance oracle comparisons pass.

It is not established to be an unmodified implementation of a cited static KOR algorithm. The revised manuscript describes this custom implementation. Its current source hash does not prove that it produced the archived workstation timings. Supplemental old-clock BL recovery is historical evidence only.

The authoritative input is queries/final_queries_1260.csv. The common driver is scripts/run_experiments.py; production BL does not generate queries. See the root STATUS.md for the distinction between current protocol, current verification and historical runtime records.
