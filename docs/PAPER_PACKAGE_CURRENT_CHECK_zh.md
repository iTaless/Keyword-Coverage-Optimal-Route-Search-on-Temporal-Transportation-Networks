# 本次三个附件的复核结论与提交说明

## 结论

按目前实际材料，能够核实当前BL固定查询接口和共同运行入口已实现，3,600条计时记录数量与原始工作簿一致，更新论文已明确区分旧计时和新实现验证。不能从这些文件证明旧计时确由当前统一实现产生。最终回信采用“修复接口和状态说明、明确统一协议、修正过强的旧重跑表述”的写法，不再要求您继续寻找其他文件，也不虚构补跑已完成。

回信集中回答本次STATUS矛盾，没有展开与此意见无直接关系的历史分数明细、查询时刻诊断数、上游数据来源、硬件比较和参考文献排版问题。旧正式计时来源仍无法核实是必要限定，已在回信中用一个段落交代，不能省略为“已全部证实统一正式重跑”。

## 已核实的一致内容

| 内容 | 附件复核结果 |
|---|---|
| 上传的代码包 | 与前一轮已核验完整包逐字节一致；ZIP完整性通过。 |
| 新上传的运行时间表 | 与代码包内author_runtime_results.xlsx逐字节一致；3,600个有效数据行，BL/A*/AP各1,200。 |
| 工作簿分组与超时 | 120个城市—参数—参数值—算法组，每组30条；3,016个数字耗时、584个超时文本。 |
| 工作簿可证明的范围 | 只有城市、参数、参数值、算法、耗时5列有数据；没有实际执行时的源码版本、起终点、dwell及完整查询记录，不能仅据此证明当时确实使用同一查询行。 |
| BL入口 | baseline_bl/fixed_query_runner.cpp读取--queries与--query-id；读取processed网络和查询级dwell，不自行生成查询。 |
| 公共运行脚本 | scripts/run_experiments.py对BL/A/AP传入相同查询文件与query_id，并写公共结果格式；独立BL可执行程序由公共脚本调用。 |
| 主研究与全部查询 | 固定文件有1,260行；主研究配置选1,200条敏感性查询，另外60条默认查询不计入3,600的配置数量。当前配置支持该运行数量，不代表已经产生了新的全量三方法正式计时。 |
| 更新论文 | 第17—19页§5.1.2—5.1.4及Table 3已区分当前代码/协议和历史记录；第19页起§5.2与图2—5标为历史描述性结果。 |
| 新A/AP验证 | 2,520次尝试、2,080返回路径、110个正得分精确参考质量查询，与包内实际表一致。54个A*资源受限单列。 |
| 小实例验证 | 20实例、200个排名、A*/BL对拍及AP路径检查与包内结果一致。 |
| 当前代码与各结果表 | 冻结版本的交叉检查通过；Python语法、主键、计时保留、源文件哈希、路径数、验证状态和小实例汇总一致。 |

本次代码包未出现新的算法源文件变化；已有构建、边界回归及独立验证证据适用于本次完全相同的源代码。本次不重复运行2,520次搜索，也没有新增或替换任何正式计时。

## 已在同步包直接修正的文档

1. 新增根目录STATUS.md，集中说明BL已经接入共同接口，以及当前协议、当前验证和历史计时的区别。
2. baseline_bl/STATUS.md补充权威查询文件与公共脚本路径，和根目录保持一致。
3. baseline_bl/README_BL.md的“formal BL executable”改为“production BL executable”，避免暗示当前源码已被证实产生原正式计时。
4. README链接到根目录STATUS.md；加入本次英文回复，供逐项核对。
5. 同步当前审查说明：已修正的Table 2不再写成待修正；旧版审查清单转存docs/history/，当前说明按本次更新PDF书写。
6. 重新生成文件校验表及文件变化清单。算法、查询、网络、原始计时、新验证输出均保持原值。

## 更新论文仍有的少量编辑问题

- 第23页Data Availability中的“are available in the https://...”语法不自然，宜使用“are provided in the replication repository”。最新包发布前，可先写“the accompanying replication package”，发布后再表述为公开仓库可获取。PDF中的GitHub目标链接本身与给定项目地址一致。
- 第24—25页若干参考文献仍有“Proceedings of the Proceedings of”的重复词，CCF A/SCI Q1等排名括注也仍在。按期刊书目格式删除重复词和排名括注即可，不必写入此次回信。
- 第25页参考文献29仍是“Submodular function maximization., 2014”，缺书名、出版社与页码。可采用此前完整LaTeX中已补齐的章节记录。核心数学证明不需因此重写。
- 第17页Table 2浮动到了§5正文之前，不影响数值和引用；若希望首次提及后才出现，可在原LaTeX中调整浮动位置。这是可选的排版调整。

建议的Data Availability替换代码：

```latex
\dataavailability{The accompanying replication package provides the
processed Austin and Houston temporal networks, C++17 source code,
fixed query inputs, original runtime records, and separately labelled
implementation-verification results. The project repository is
\href{https://github.com/iTaless/Keyword-Coverage-Optimal-Route-Search-on-Temporal-Transportation-Networks}{the replication repository}.
The MobilityData GTFS Validator is identified separately as validation software.}
```

该写法避免把尚未确认公开的最新附件写成已发布版本。待同步后，可将“accompanying replication package”改为实际公开仓库表述。本次没有修改您上传PDF的页面内容；上述是可以在原LaTeX中直接应用的文字和排版修正。

## 提交前的操作

请用本次GitHub_reviewer_response_aligned.zip同步GitHub完整目录，并使根目录STATUS.md、baseline_bl/STATUS.md与回信一致。该包已经准备好，但本次没有向远程仓库推送，也未假定线上同步已完成。同步时核对旧目录的文件移除项，避免仅追加新文件而留下旧STATUS。

建议将本次回复Word和同步后的代码包一并作为修订材料提供。回复没有声明“又完成了全量三方法正式重跑”，与更新论文现有证据范围一致；它解决文档与陈述矛盾，不补造旧计时的执行来源证明。
