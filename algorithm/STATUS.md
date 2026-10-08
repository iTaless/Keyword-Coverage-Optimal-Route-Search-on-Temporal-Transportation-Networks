# BL integration status

The files in this directory implement a time-expanded static-graph BL search, preserve original temporal edge identifiers, support top-k distinct routes, and include deterministic and randomized differential tests.

The implementation currently has its own raw-data driver and query generator. It is not yet connected to `queries/reviewer_queries.csv` or `scripts/run_experiments.py`. Therefore its city-scale output must not be combined with A/AP results until the fixed-query interface and common JSONL schema are added.

`BL_enum` in `src/main.cpp` is a different component: it is an exhaustive small-instance oracle and must never be labelled as the city-scale BL baseline.

