# 审稿回复准备状态（暂不可作为“全部解决”的正式回信）

依据此前提供的同一审稿人的三项意见和 reviewer_response.docx；未虚构审稿人编号或新意见。旧回复中“we reran the evaluation after finalizing Algorithms 1–4”和“all 22,775 passed”不应直接提交：当前证据只能支持下面限定范围。

| 原意见 | 已完成证据 | 未满足的部分 |
|---|---|---|
| 1. 修订算法/停留规则与计时来源对应；不对应则重跑 | 代码映射、秒制/查询停留统一、输入/源码哈希、新A/AP2520次验证 | 3600正式记录实际源码/网络/查询版本；原查询时间含义。当前云端验证不能替代原机正式实验。 |
| 2. 独立可行性、实际路径数、相对精确结果质量、小实例穷举 | 新路径逐条核验；质量只取完成且正得分精确参考；20实例/200ranks | 旧22775路径、240/0.94/224不能沿用已验证措辞。新验证范围/资源限制须进论文，正式研究输入仍待定。 |
| 3. 完整实验规格与真正数据/结果获取方式 | 完整本地仓库、已实现查询和数据、运行命令、元数据、原始记录与验证、论文已有GitHub链接核对 | 实际公开commit/tag尚未上传；上游GTFS/POI/cost来源与许可记录不全。 |

## 可使用的英文核验稿

Dear Reviewer,

We have corrected and independently checked the current implementation of Algorithms 1–4. Production BL, A* and AP now read the same processed second-valued timetable and the same fixed query row, including its positive query-level dwell threshold. Dataset loading and query preprocessing/search times are recorded separately. The archived author-workstation runtime measurements are retained with their original provenance labels; the new verification measurements are not substituted for them.

For Comment One, the source-to-algorithm mapping and new execution metadata are available in the artifact. However, the available historical executable/input lineage and the intended clock interpretation of the archived queries have not yet been fully reconciled. We therefore do not claim that the archived 3,600 measurements have been reproduced by the corrected release. This point must be resolved by recovering the original formal source/input files or completing a documented formal rerun before resubmission.

For Comment Two, we attempted all 1,260 released fixed queries with A* and AP, giving 2,520 new executions. The status counts, memory/time limits, raw routes and independent replay logs are included under results/rechecked_aap/. The new run returned 2,080 route records; all returned paths passed independent feasibility, distinctness and coverage-score checks. Resource-limited A* attempts are separately recorded and are excluded from exact-reference quality ratios. Among the 110 eligible positive-reference queries, the mean AP/A* best-score ratio was 1.00000000, the median was 1.00000000, and 110 pairs had equal best scores. These statistics refer only to the literal released second-valued inputs and the explicitly documented verification environment. They do not retroactively validate the old quality archive or restore the historical intent of its query windows. Separately, A* and BL matched independent exhaustive enumeration on all 20 tractable second-valued city subgraphs, covering 200 complete ordered ranks; every AP route on those instances passed replay. The selection restrictions and all instance inputs are released.

For Comment Three, the artifact separates actual datasets, fixed queries, source code, original measurements, new raw outputs and validation results. The existing manuscript link to the replication repository should be pinned to the uploaded commit/tag. MobilityData GTFS Validator is identified as software. Remaining upstream feed, POI and cost provenance should be supplied from the authors' original records rather than reconstructed by assumption.

This is an internal evidence-alignment draft. The unresolved parts in Comment One and the final publication version must be completed before replacing it with a submission-ready response claiming that all concerns have been addressed.
