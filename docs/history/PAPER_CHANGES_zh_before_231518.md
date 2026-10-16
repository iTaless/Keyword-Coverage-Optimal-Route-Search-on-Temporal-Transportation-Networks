# 对附件论文的逐项修改清单

依据：mathematics-4569987-v1(20261015-081807).pdf，共 23 页（页脚版本 October 9, 2026）。下面页码是 PDF 印刷页码。未直接改写 PDF；没有原始 LaTeX 工程，且正式实验版本仍待追溯。条目区分可直接修改与需要正式证据才能定稿的内容。

| 位置 | 当前内容/问题 | 需要的修改 |
|---|---|---|
| 摘要，第 1 页，第 14–21 行 | 22,775 路径、874 full-k、240 个质量查询、均值 0.94、224 相同作为修订算法验证结果 | 这些是旧档案统计，不能继续写成当前秒制/查询级规则已验证的结果。待正式实验口径确定后统一替换；若明确采用本次新验证，须明确其输入含不可行查询、单独环境/资源上限，使用新统计而非与旧计时混写。 |
| §3.1–3.3，第 5–7 页 | 单查询 d_Q>0、等待不足仅影响覆盖、乘积覆盖 | 核心定义与当前生产代码一致，保留。实验和回复中的节点级 s_v 描述必须全部改为该查询阈值；历史节点表仅作为诊断来源。 |
| §3.4 与 §4，第 8–16 页 | BMC 归约、时间过滤、预算下界、精确搜索、AP证书 | 本次静态核对未发现必须改写的数学结论。不能把有限测试说成理论证明。可在实现段说明浮点预算容差 1e-9、保守上界剪枝容差 1e-12。 |
| §4.4，第 14–16 页 | AP 的采样/排序/平局规则 | 对齐实现：rho=8、L=20、J=k；多次运行分层随机采样、固定查询种子衍生各轮种子；候选平局比较补全后路径到达时间，再比较边 ID。保留无近似保证、可能不足 k 的限定。 |
| Table 2，§5.1.1，第 17 页 | Austin 2.70K / 535.50K / 198.30；Houston 9.10K / 1,796.20K / 197.40 | 若论文采用当前发布网络，改为精确值 Austin 2,683 / 535,542 / 199.61；Houston 9,094 / 1,446,762 / 159.09。若正式实验另有网络版本，先提供该输入，不能擅用当前表冒充原正式输入。 |
| §5.1.1，第 17 页 | GTFS、POI 匹配和成本来源 | 补上实际 feed/下载版本、service date、POI 来源与 500 m 匹配记录、成本单位/生成方式及许可。只能填写可追溯的真实来源。处理后数据哈希不能替代这些上游信息。 |
| §5.1.2，第 17–18 页 | BL 被描述为应用静态 KOR 过程 | 当前代码是自实现时间展开搜索，并非已核实的原论文 KOR 原版。应精确描述该实现和处理步骤，或提供并重测真正拟引用的 BL。20 个小实例可报告 BL 对拍结果，但不证明旧计时实现身份或上游算法等价。 |
| §5.1.3，第 18 页 | 固定查询、master seed=20260928、秒制时间窗和查询停留阈值 | 固定 CSV 是已实现参数记录，但种子不止一个，且原始查询文件没有 query-level dwell；不能声称它由已发布的单一生成流程完全复现。需明确实际生成/补入过程。855 个 start 在秒制输入下晚于班次，且部分字段不是合法 HHMMSS，不能整列机械转换。正式查询意图需原始生成器/输入支持。 |
| §5.1.4，第 18 页 | 正式 i7 机器、300 秒和计时范围 | 已确认的原机配置可保留，但正式记录和源码/输入哈希尚无完整对应。当前脚本外部 300 秒包括加载，报告 query time 则不含加载，二者须分别说明。本次云端验证有四并发和每进程 1536 MiB 虚拟内存限制，不能放入 i7 正式耗时图。 |
| Table 3，第 18 页 | 混合主计时、旧路径、旧质量和旧小实例统计 | 分开归档记录与新代码验证；小实例替换为本次 20 实例、200 ranks。不要把旧路径验证和新秒制验证合并成“全部通过”。 |
| §5.2 与 Figures 2–5，第 18–20 页 | 正式耗时趋势与各组两位小数统计 | 已逐格保留提供的 3,600 条正式计时，当前图也仅由该表生成；但须先完成实际正式代码/输入溯源，才可宣称是修订算法结果。如正式实现确用 HHMMSS 相减，需要重跑受影响正式实验后重绘。 |
| §5.3，第 20 页 | 22,775 路径全部可行，A*/AP full-k 等旧统计 | 原始数值时刻下结构检查通过不等于秒制可行性通过。旧记录仍可称档案，但不能当成当前规则已核验路径。新运行结果：A*: {'no_route': 1052, 'ok': 154, 'resource_limit': 54}；AP: {'no_route': 1052, 'ok': 208}；路径记录 2,080 条，独立路径/得分核查失败 0 条。 A* full-k 151；AP full-k 14（均以全部 1,260 查询为分母，资源受限单列）。必须明确实验范围后使用。 |
| §5.4，第 21 页，第 695–704 行 | 240、0.94、224，以及“精确最佳得分”资格 | 旧 1,146 条得分差异影响该依据，其中原 240 子集 81 查询有路径分数差异、56 查询的最佳记录分数变化。不能仅重打分就继承 A* 最优性。当前新实验：新质量子集 110 条；均值 1.00000000，中位数 1.00000000，IQR [1.00000000, 1.00000000]；110 条最佳得分相同；AP>A* 为 0 条。 只纳入完成且正得分、独立核查通过的 A* 对，资源受限/超时/精确零分不计质量比；不可把它与旧质量子集拼接。 |
| §5.5，第 21 页，第 705–722 行 | 20 个实例、10–83 edges、7–1,226 routes、dwell690–1772、197 ranks、七条路径实例 | 替换为 20 个秒制实例、10–96 edges、15–7,585 routes、dwell619–1718、200 ranks。删除“仅七条路径”的旧实例描述。改路径为 validation/seconds_instances/。如新增 BL 验证，报告 A*/BL 均20/20，AP仅报告返回路径可行/得分一致。明确不超过100边、10,000枚举路径/200,000展开的筛选条件。 |
| §5.6，第 21 页 | 874、224/240、0.94 和运行时间总评 | 随选定的新正式结果同步，避免继续把不同代码/时间口径的数字拼成同一实验结论。保留 AP 不保证 full-k/近似比的限定。 |
| §6，第 21–22 页 | 主要为理论/方法总结 | 理论性表述可保留；任何新增实验性优越结论必须限定到最终经过核查的数据和运行范围。 |
| Data Availability，第 22 页 | 当前 PDF 已有 GitHub 超链接，MobilityData 仅是软件 | 不需要再声称“缺少 GitHub 链接”。上传后固定到实际 commit/tag，并明确 processed 输入、实现、固定查询、原始输出、独立验证的路径。不要写尚未上传的本地 ZIP 已公开。 |
| 参考文献，第 22–23 页 | 重复“Proceedings of the Proceedings of”、CCF/SCI 排名括注、部分条目不完整 | 清理重复词和非书目信息，按期刊格式补齐卷页/DOI。未凭年份推断最新文献不存在；本次核对的近年条目有原出版方/作者来源支持。 |

## 可直接替换的 §5.5 英文段落

We constructed 20 tractable query-specific instances, ten from each city, using fixed city seeds 20261928 and 20262928. Raw HHMMSS schedule values were explicitly converted to elapsed seconds. Each instance retained all scheduled edges among the selected vertices within a 90-minute window. To make complete enumeration tractable, we rejected extractions with more than 100 edges or with more than 10,000 enumerated routes or 200,000 enumeration expansions. The accepted instances contained 3–6 vertices, 10–96 scheduled edges, and 15–7,585 feasible routes; realized query-level dwell thresholds ranged from 619 to 1,718 seconds. A separate Python depth-first enumerator recomputed feasibility and keyword coverage without using the C++ graph filters, minimum-budget profile or coverage upper bound. Across all 200 compared ranks, A* and BL matched the complete ordered top-10 edge sequences and coverage scores from enumeration (score tolerance 1e-8). Every route returned by AP passed independent feasibility and scoring checks. The exact per-instance inputs and rank-by-rank outputs are provided under validation/seconds_instances/. This validation is limited to these tractable instances and does not establish historical runtime provenance.

## 仍必须取得的最少材料

1. 正式计时实际使用的源代码/可执行文件、查询文件和网络文件，最好包含运行命令/日志；已确认过的 CPU、内存和操作系统不需要再确认。若找不到，可使用当前统一代码和明确修正的查询生成流程在正式机器重新做受影响实验。
2. 查询 start/end 的真实时间含义和对应生成依据。只需提供真实输入/生成器，不建议靠口头选择“把某列当秒还是HHMMSS”来修复证据。
3. 若正式网络确实不是本包快照，提供该版输入；否则论文 Table 2 应按当前实际文件修改。数据来源与许可记录也需要补全能取得的部分。

当前包已经把代码、验证和档案分清；上述历史事实无法由本地修改推导出来。因此现在不能以“全部已解决”的措辞提交正式回复。
