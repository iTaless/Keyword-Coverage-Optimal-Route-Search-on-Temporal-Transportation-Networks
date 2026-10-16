# Release status

Production BL/A*/AP and fresh independent validators consistently use elapsed seconds and query-level positive dwell thresholds. Builds and the five Make test stages pass. Twenty physical-second city subgraphs pass independent exhaustive comparison (200 ranks, A*/BL) and AP route checks.

All 1,260 fixed query rows have now been attempted for A*/AP (2,520 executions) under an explicitly recorded verification environment: A*: {'no_route': 1052, 'ok': 154, 'resource_limit': 54}；AP: {'no_route': 1052, 'ok': 208}；路径记录 2,080 条，独立路径/得分核查失败 0 条。

The supplied 3,600 author-workstation runtime cells are preserved. They have NOT been replaced with these new measurements. Legacy route scores still have 1,146 mismatches under the paper's query-dwell rule; 325 are explained by old node-dwell inputs, and 821 are not reproduced even under that rule. None of the 28,342 archived routes passes replay against the current processed-second tables and unchanged query windows.

The latest revised manuscript explicitly separates current verification from the historical runtime archive, and its Table 2 matches the released snapshot. Historical source/input lineage and intended query-time interpretation remain unresolved. No complete verified formal three-method rerun is claimed. See KNOWN_ISSUES.md and PAPER_PACKAGE_CURRENT_CHECK_zh.md for the current scope.
