# 最终代码和结果口径

当前 BL、A*、AP 均读取 processed 秒制网络与同一查询的 dwell_seconds，不再直接减 HHMMSS。固定查询没有被私自改值。

运行：`make`、`make test`；三方法主实验命令 `python3 scripts/run_seconds_study.py`。A/AP 全查询验证命令 `python3 scripts/recheck_aap.py --workers 4 --timeout 300 --memory-mib 1536`。已有实际结果在 results/rechecked_aap/，A*: {'no_route': 1052, 'ok': 154, 'resource_limit': 54}；AP: {'no_route': 1052, 'ok': 208}；路径记录 2,080 条，独立路径/得分核查失败 0 条。

新质量子集 110 条；均值 1.00000000，中位数 1.00000000，IQR [1.00000000, 1.00000000]；110 条最佳得分相同；AP>A* 为 0 条。

这里解决的是当前输入、当前代码、新输出之间的得分一致性；不表示旧的 1,146 条得分已被证明正确。旧节点停留规则解释其中 325 条，另外 821 条仍不能复现。也不能把重新给旧路径打分当作重新求出精确最优值。

正式 3,600 条计时保留原电脑记录；BL 补跑路径保留各自来源。原论文还需要核实正式实现/输入版本和查询时刻口径。论文修改与审稿回复准备见 docs/PAPER_CHANGES_zh.md、docs/REVIEWER_RESPONSE_STATUS_zh.md。
