# Data dictionary

## Fixed queries

`query_id` is the stable global key; `query_no` is a replicate number within a city/parameter/value group. Query columns retain source/target, budget, keywords/weights, k, seed, start_min/end_min and dwell_seconds. The legacy names describe intended second-valued query windows/dwell, but archive replay has exposed a mismatch with HHMMSS clock integers. Do not silently convert or regenerate query parameters.

## Composite final results

`main_study.runtime.csv` has 3,600 formal rows. `status` is formal completed/timeout; `runtime_sec` is blank for formal timeouts. Those columns are unchanged from the supplied workbook. `supplementary_status`, `supplementary_timeout_threshold_sec` and `recovery_runtime_sec` refer only to later BL recovery. Route counts/best scores are available evidence, with origin named by `route_record_origin`; they need not describe the formal timed execution.

`main_study.routes.csv` has 27,181 rows (21,614 A/AP, 5,567 supplementary BL). `all_methods.routes.csv` has 28,342 rows, including supplementary default A/AP queries. `route_level.routes.csv` remains the original A/AP-only 22,775 rows.

Every integrated route identifies its `record_origin`, `edge_id_namespace`, `time_basis` and `validation_scope`. Route rank preserves archival ordering; it is not assumed to equal score order. A derived best_kc always uses the maximum recorded route score.

`main_study.jsonl` is normalized composite data, not raw executable stdout. Formal time/status fields are separate from `supplementary_execution`. Its BL solutions may come from a recovery run even when formal status is timeout. Fresh executions of run_experiments.py write raw ok/no_route/timeout/resource_limit/error records with a different manifest schema.

`main_study.validation.csv` keeps formal `execution_status` separately from recovered `route_execution_status`. `validation_time_basis=raw_integer_clock` narrows any pass to archive replay; `processed_seconds_validation_status=not_passed` reports the unresolved second-valued input mismatch. No route record is not proof of infeasibility. No-route outcomes and timeouts remain distinct.

`coverage_quality.csv` preserves 240 recorded ratios; it is not a table of newly validated optima. See audit/coverage_quality.recorded_score_audit.csv before reuse.

## Edge IDs and time encoding

Raw rows contain source, target, departure HHMMSS, arrival HHMMSS and cost. Their stable IDs are zero-based record indices, encoded as decimal strings in archived paths. Raw ID 0 corresponds to processed ID austin_e000001 or houston_e000001. Processed CSV tables convert HHMMSS values to elapsed seconds. These are different namespaces and different time bases.

The archive's arrival_time_sec/arrival_min fields contain unconverted raw-clock integers despite their names. Thus subtraction of archive clocks is not valid elapsed-second arithmetic. The independent audit retains both raw-clock outcomes and replay against converted seconds without pretending they are equivalent.

Station-level dwell fields are legacy outputs; fixed-query implementations use the query-level dwell threshold. Source files and originally supplied records are preserved unchanged.


## Fresh A*/AP verification

`results/rechecked_aap/aap.jsonl` is actual current executable output plus execution metadata. `aap.runtime.csv` separates query time from outer wall time and leaves failed/unavailable route counts blank. `aap.routes.csv` contains processed IDs and elapsed-second arrivals. `aap.validation.csv` distinguishes passed route replay from not_checked resource-limited attempts. A no-route schema pass checks no returned routes and is not a city-scale independent infeasibility oracle.

`coverage_quality.all_queries.csv` has one row per fixed query, explicit eligibility and exclusion reasons. A route-free or incomplete output has no reported best-route score. AP with no route contributes zero coverage in an eligible positive-A* pair only, as the explicit empirical convention. All current quality aggregates are in quality_summary.json. 新质量子集 110 条；均值 1.00000000，中位数 1.00000000，IQR [1.00000000, 1.00000000]；110 条最佳得分相同；AP>A* 为 0 条。
