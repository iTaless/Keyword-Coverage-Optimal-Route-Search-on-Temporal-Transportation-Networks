# BL seconds-based fixed-query protocol

BL reads data/processed/<city>/stations.csv and trips.csv and the same fixed query row as A*/AP. Time and query dwell arithmetic is exclusively in seconds. Use scripts/run_seconds_study.py for all three methods, or scripts/run_bl_shard.py with --queries, --out, --bl-binary and --project-root for BL only. The latter also uses processed tables; verify its paths with scripts/verify_bl_shard.py.

BL production emits processed scheduled-edge IDs. Raw zero-based IDs appear only in archived recovery evidence. Never mix those namespaces. Checkpoints from old raw-clock runs are rejected by the current BL-only runner; use a new output path. Default external timeout scope matches the unified runner and includes input loading.

Historical recovered paths and their original adapter are preserved as source evidence, not production answers. Their original timing/status data is never silently replaced. See docs/KNOWN_ISSUES.md and docs/EXPERIMENT_PROTOCOL.md.
