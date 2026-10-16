# Fixed query records

`final_queries_1260.csv` is the authoritative realized input file. It contains 630 Austin and 630 Houston queries: 600 sensitivity-study queries plus 30 supplementary default queries per city.

All stochastic choices needed to identify the inputs are materialized in the CSV. The random seed is retained for traceability, but a reproduction run must read the stored values rather than regenerate them. BL, A*, and AP should use the identical `query_id` row.

`query_no` is a replicate number within a city/parameter/value group. It is not globally unique; always use `query_id` as the key.

The intended dwell rule is query-level: one `dwell_seconds` value is applied to every potentially visited POI for that query.

## Time-basis audit caveat

The unchanged query fields match the archived route records by ID, but their intended seconds interpretation is inconsistent with archived raw-HHMMSS integer-clock execution. See `docs/KNOWN_ISSUES.md`. A fixed query ID alone does not resolve this units issue. Do not silently regenerate query values.
