# New A*/AP scoring verification

This is an actual fresh run of current C++ source on all 1,260 unchanged fixed query rows (2,520 executions), with processed second-valued network times and query-level dwell. It is not a reproduction of the original author-workstation timing experiment and is not used to redraw its runtime figures.

A*: {'no_route': 1052, 'ok': 154, 'resource_limit': 54}；AP: {'no_route': 1052, 'ok': 208}；路径记录 2,080 条，独立路径/得分核查失败 0 条。

新质量子集 110 条；均值 1.00000000，中位数 1.00000000，IQR [1.00000000, 1.00000000]；110 条最佳得分相同；AP>A* 为 0 条。

Four concurrent subprocesses; 300-second external wall-clock cap including loading; 1,536 MiB virtual-address-space cap per subprocess using Linux `prlimit`. `resource_limit` is distinct from timeout and from `no_route`. An interrupted A* has no optimality claim and is excluded from quality ratios. No-route records provide no returned paths to replay; their passing output-schema audit is not an independent exhaustive city-scale proof of infeasibility. For quality, only completed positive-score A* results and completed AP results with successful replay are included; A*=0 has no ratio. Best scores use the maximum over returned routes.

Reproduce: `make` then `python3 scripts/recheck_aap.py --workers 4 --timeout 300 --memory-mib 1536`. It resumes only with the exact recorded query/data/source/binary identity. Run metadata and input hashes are included. Use a new `--out-dir` for a different binary or input. Query counts differ from the 1,200-query-per-method formal runtime study because these include 60 default-setting queries.

All supplied query values were preserved, including starts that are infeasible against the released second-valued timetable. This run establishes current code/output consistency for those literal inputs; it does not recover their historical intended time-of-day values or validate the original formal timing source.

`run_metadata.json` is the single checkpoint/input identity record. `summary.json` contains both execution totals and the `quality` section; separate duplicate identity and quality-summary files are not stored.
