# Historical runtime and route archive

These files preserve the supplied timing records and separately sourced route evidence. They are separate from the current implementation verification in `../rechecked_aap/` and the exhaustive instances in `../../validation/seconds_instances/`.

## Main files

| File | Rows | Meaning |
| --- | ---: | --- |
| `runtime.csv` | 3,600 | Original main-study timing/status values, with separately labelled route-recovery fields |
| `routes.csv` | 28,342 | One complete route table: A* 11,687, AP 11,088, supplementary BL 5,567 |
| `runtime_summary.csv` | 120 | Group summaries of the historical timing records |
| `quality.csv` | 240 | Recorded historical quality pairs, with their score/time audit fields |
| `validation/queries.csv` | 3,600 | Query-method validation scope and outcomes |
| `validation/routes.csv` | 28,342 | Route-level clock/score checks; `node_dwell_` and `bl_` columns retain the additional diagnostics |
| `bl_recovery/runtime.csv` | 703 | All BL recovery attempts; `cohort` identifies the 653 previously completed and 50 previously timed-out groups |
| `summary.json` | — | Timing/route counts and the combined time, score, schema and BL-recovery summaries |
| `manifest.json` | — | Input/source identities, file map and consolidated source-archive mapping |
| `source/` | — | Original workbooks and tables, A/AP source archive and historical adapter |

There is one stored route table. Select `algorithm == BL` for all supplementary BL paths. Join a route's `(query_id, algorithm)` to `runtime.csv` to select the 27,181 main-study routes; the remaining 1,161 are default-setting A*/AP route records. Route ranks retain archival order and do not imply score order.

The four returned BL batches are consolidated in `source/bl_recovery.zip`. Each batch occupies its own member directory, preserving all received files. `manifest.json` maps the original batch names and hashes to those directories. The former standalone 653/50 subsets, combined JSONL copy, example files, filtered empty-route list, duplicate normalized route views and audit fragments are not separately stored.

Historical routes use raw HHMMSS integer clocks. Their audit fields do not establish elapsed-second feasibility or current exact-reference quality. Formal runtime values and timeout statuses remain unchanged; BL recovery times are separate. Current code/source hashes identify the distributed implementation and do not certify the producer of the old timings.

## Regenerate the canonical archive views

```bash
python3 scripts/integrate_bl_results.py
python3 scripts/check_release_consistency.py
```

The integration command normalizes source tables, attaches recovery evidence, runs the independent archival diagnostics and consolidates the outputs into this layout. The two diagnostics can also be rerun directly without recreating redundant tables:

```bash
python3 scripts/audit_archived_routes.py
python3 scripts/audit_score_provenance.py
```

New BL/A*/AP measurements should use a separate output directory and must not overwrite these historical timing records. Paper figure exports are omitted; `scripts/plot_results.py` can plot `runtime.csv` when needed.
