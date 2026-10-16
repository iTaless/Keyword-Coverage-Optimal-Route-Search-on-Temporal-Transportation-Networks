# 上传和复核

这是完整目录包，包含当前代码、固定输入、新 A/AP 原始结果和核验、20 个秒制小实例、原正式计时及单独标记的历史档案。未向远程仓库推送。当前源码通过检查不等于旧论文全部实验结论已经复现；正式回复前仍需处理 KNOWN_ISSUES.md 和 PAPER_CHANGES_zh.md 中的未决项。

1. 解压到一个新目录，先阅读 README.md、docs/FINAL_AUDIT_zh.md 和 docs/PAPER_CHANGES_zh.md。
2. 执行 `sha256sum -c MANIFEST.sha256` 检查文件完整性。构建和算法测试为 `make`、`make test`；已归档表的交叉检查为 `python3 scripts/check_release_consistency.py`。这个脚本校验本次冻结的结果数量，不是对任意重新生成实验都适用的固定断言模板。
3. 本地复跑 A/AP：`python3 scripts/recheck_aap.py --workers 1 --timeout 300 --memory-mib 0 --out-dir results/local_aap_rerun`。这个命令禁用本次验证环境的内存限制，不覆盖已归档结果。正式比较应串行执行并记录实际机器。若有不同查询或源码，必须使用新输出目录；不要把新耗时填回历史计时表。
4. 三方法统一主实验：`python3 scripts/run_seconds_study.py --out results/local_main_study/main_study.jsonl`。完整主实验应在真实正式机器、明确查询含义后执行。当前脚本外部限时包含加载，报告的查询耗时不含加载；请不要据此宣称已复现原论文的计时边界。
5. 上传已有仓库时应同步完整树并检查 `docs/UPLOAD_FILES_CHANGED.csv`，包含标记为 removed 的旧路径。旧 validation/small_instances 等已移动到 validation/legacy_raw_clock/；只在网页里追加新文件会留下旧内容。请不要把 build/、临时输出或另一层同名仓库目录当成源码上传。
6. 发布后以实际公开 commit/tag 更新论文 Data Availability；本包没有假定尚未完成的远程发布已成功。

不需要重新确认已经提供的 i7-9700K、32GB、Ubuntu18.04 配置或 BL 补跑数量。需要的是实际正式实验原始代码/输入与查询时刻生成依据；如果这些文件无法找回，则应以明确记录的新正式实验代替相关旧实验主张。
