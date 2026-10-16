# Result and input definitions

## Fixed inputs

`queries/final_queries_1260.csv` contains 1,200 sensitivity rows plus 60 default-setting rows. `query_id` is globally unique; `query_no` is a replicate number within a city/parameter/value group. Query values, seeds, keyword weights and query-level dwell thresholds are preserved. Production network arithmetic uses elapsed seconds despite legacy `*_min` column names. See KNOWN_ISSUES.md for the historical query-clock limitation.

## Historical archive: results/final

`runtime.csv` contains the 3,600 supplied formal timing/status records. `status` and `runtime_sec` describe the original timed execution. `supplementary_status`, `supplementary_timeout_threshold_sec` and `recovery_runtime_sec` describe separate BL recovery attempts. A returned route count or best score may refer to those later paths, as identified by `route_record_origin`.

`routes.csv` contains one canonical 28,342-row archive table. Its key is `(query_id, algorithm, route_rank)`; its edge lists use raw zero-based record IDs. It includes 22,775 A*/AP paths and 5,567 BL recovery paths. Joining `(query_id, algorithm)` against `runtime.csv` selects the 27,181 main-study paths. Default-setting paths are retained in the same table. A rank is an archived sequence position, not a certified score rank.

`validation/queries.csv` records the query-method validation scope, keeping formal execution status separate from route-recovery status. `validation/routes.csv` joins the formerly separate time/score diagnostics by the route key. Unprefixed columns compare raw-integer and processed-second replay; `node_dwell_` columns retain the earlier node-versus-query dwell comparison on A*/AP routes; `bl_` columns retain the returned BL validation fields. Empty columns mean that diagnostic does not apply to the row.

`quality.csv` preserves the 240 historical recorded ratios. `audit_` columns contain the associated independent diagnostic and eligibility information. These are not the current 110 completed positive-reference comparisons and are not new certified optima. `runtime_summary.csv` contains 120 historical groups. `summary.json` combines the archive counts and audit summaries under explicit section names.

`bl_recovery/runtime.csv` contains all 703 attempts. The `cohort` and original formal status distinguish the 653 previously completed queries from the 50 previously timed-out queries. Recovery outcomes remain ok/no_route/timeout; zero returned routes are not automatically a timeout. The extra interpretation and original A*/AP count columns apply to the 69 attempts without paths. BL paths are selected from `routes.csv` by algorithm; no duplicate 653/50 route files are stored.

`source/bl_recovery.zip` consolidates the four received batch archives into separate member directories. Its input mapping is in `manifest.json`. All batch members remain available, including raw executable JSONL and validation files. Public copies omit machine-specific platform strings, while execution settings, timing/status fields, routes and scores remain unchanged. The A/AP mixed-schema source archive and original workbooks/tables are retained separately.

## Current A*/AP verification: results/rechecked_aap

`aap.jsonl` is current executable output plus execution metadata. `aap.runtime.csv` separates query time from outer wall time. `aap.routes.csv` records processed edge IDs and elapsed-second arrivals. `aap.validation.csv` distinguishes passed replay from resource-limited attempts that could not be checked. A no-route output-schema pass is not an exhaustive city-scale infeasibility proof.

`coverage_quality.all_queries.csv` records each query's eligibility and exclusion reason. Only normally completed positive-score A* references with completed, validated AP output contribute a ratio. The best score is the maximum over returned paths. Zero-score feasible outputs, no-route outcomes and interrupted exact runs remain distinct.

`summary.json` holds all execution and quality summaries; its `quality` section reports eligible pairs, mean/median/IQR, exact-score matches and per-method route-producing/positive-score/zero-score counts. `run_metadata.json` is the single input/execution identity record used for checkpoint validation and resume. It includes hashes, execution limits, concurrency, Python version and the recorded command.
