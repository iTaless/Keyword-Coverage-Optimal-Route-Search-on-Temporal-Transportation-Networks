# BL：基于静态时间展开图的基线算法

本目录给出论文实验中 BL（Baseline）的 C++17 可运行实现。BL 不直接删除
公交边的时间信息，而是先把时序公交网络转换为静态有向图，再在静态图上执行
精确的 top-k 关键词覆盖路线搜索。

## 1. 转换方法

实现采用 Huang、Fu 和 Liu 在 SIGMOD 2015 论文 *Minimum Spanning Trees in
Temporal Graphs* 第 4.2 节提出的“时间副本节点 + 虚拟等待边 + 实边”思想，
并针对源点—终点路线查询进行了适配。

对于查询窗口 `[t_alpha,t_beta]`：

1. 为站点 `v` 的每个不同到达时间 `t` 建立静态节点 `v^t`；源点额外建立
   `v_s^t_alpha`。
2. 同一站点的连续时间副本之间加入费用为 0 的等待边。
3. 对时序公交边 `e=(u,v,t_dep,t_arr,w)`，从 `u` 中不晚于 `t_dep` 的最后一个
   时间副本连接到 `v^t_arr`，边费用仍为 `w`，并保存原始公交边编号。
4. 目标站点的全部时间副本连接到超级终点。

因此，转换图中的普通有向路径与原网络中的时间可行公交路线对应。代码仍会
独立检查实际到达时间、下一条公交边的出发时间和查询截止时间。

转换后的搜索遵循 Zeng 等人 ORS-KC 的 A* 基本框架，但为了与修订稿的正确性
要求一致，本实现没有把贪心 BMC 解值直接作为必然安全的上界。BL 使用的是更
宽松但可证明安全的“静态可达关键词”上界，并将原论文的单条最优路线扩展为
top-k 不同原始公交边序列。论文中应把这一适配写清楚，不能只写“调用已有
KOR 算法”而省略 top-k、停留时间和去重规则。

## 2. 停留时间和关键词计分

虚拟时间节点不被当作不同 POI。同一物理站点的全部时间副本共享站点编号和
关键词集合。路线在站点 `v` 的实际等待时间达到 `s_v` 时才确认访问；同一站点
即使有多个时间副本或被多次经过，也只计一次关键词贡献。

默认使用固定随机种子 `20260928`，为包含关键词的站点生成 `[900,1800]` 秒，
即 15–30 分钟的停留时间。可通过 `--s-min` 和 `--s-max` 修改。

## 3. 文件说明

- `bl_time_expanded.hpp`：时序图转换、BL top-k 搜索和独立路线验证。
- `transit_router.hpp`：论文 Algorithms 1–4、数据结构和小规模穷举验证器。
- `austin_loader.hpp`：读取 `u v departure arrival weight` 边文件和节点关键词文件。
- `main_with_bl.cpp`：BL、A*、AP 的统一实验程序。
- `test_bl.cpp`：确定性单元测试，以及 100 个随机小图与穷举结果的差分验证。
- `CMakeLists.txt`：构建配置。

`Edge::original_id` 用于保存输入文件中的稳定边号。过滤图可以重新编号内部边，
但所有输出路线都会投影回原始边号，因而可直接对照原始数据复核。

## 4. 编译和测试

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

也可以直接编译：

```bash
g++ -std=c++17 -O3 main_with_bl.cpp -o kcrn_bl
g++ -std=c++17 -O2 test_bl.cpp -o test_bl
./test_bl
```

## 5. 运行实验

单个参数设置的快速测试：

```bash
./kcrn_bl \
  --edges 9100edges4 \
  --labels 9100label4 \
  --queries 1 \
  --skip-default \
  --sweeps K \
  --only-param 3 \
  --kw 3 --k 1 --delta 50 --window-min 120 \
  --timeout-ms 300000 \
  --out houston_smoke
```

论文全部参数扫描：

```bash
./kcrn_bl \
  --edges DATASET_EDGES \
  --labels DATASET_LABELS \
  --queries 30 \
  --sweeps K,Delta,Window,k \
  --timeout-ms 300000 \
  --seed 20260928 \
  --s-min 900 --s-max 1800 \
  --out city_results
```

默认参数为 `|K|=5`、`Delta=70`、窗口 180 分钟、`k=10`。每个算法、每个查询的
300 秒限制包含该算法自己的全部预处理。运行时间严格超过 300 秒或内部触发
时间限制时，输出列 `timeout` 为 1。

小规模正确性验证：

```bash
./kcrn_bl \
  --edges DATASET_EDGES --labels DATASET_LABELS \
  --validate --subgraph 100 --queries 30 \
  --kw 3 --k 1 --window-min 240 --delta 90 \
  --timeout-ms 300000 --out validation
```

该模式分别运行 A*、转换后的 BL 和独立穷举器，并输出三者的最优覆盖分数是否
一致。穷举器只用于小图验证，不是论文实验中的 BL。

## 6. 输出文件

程序生成三类 CSV：

- `PREFIX.csv`：每个查询、每个算法一行，包括总运行时间、超时、可行性、实际
  返回的不同路线数、最佳覆盖分数、相对精确结果的比例、路线验证结果、BL
  展开标签数以及转换图节点/边数。
- `PREFIX.routes.csv`：每条返回路线的完整原始公交边序列、预算、覆盖分数及
  出发/到达时间。
- `PREFIX.queries.csv`：源点、终点、时间窗口、预算、k、关键词、权重、随机
  种子和停留时间范围，可用于逐查询复现。

`all_routes_valid=1` 表示该算法返回的全部路线都通过了与搜索过程独立的检查：
端点、边连接、换乘时间、时间窗口、预算、重复公交边、首次到达目标规则以及
重新计算的停留时间覆盖分数。

## 7. 在其他程序中调用 BL

```cpp
#include "bl_time_expanded.hpp"

kcrtn::BLResult result =
    kcrtn::blTimeExpandedTopK(graph, poi_profile, query, 300000);

for (const kcrtn::Route& route : result.routes) {
    kcrtn::BLRouteCheck check =
        kcrtn::validateBLRoute(graph, poi_profile, query, route);
}
```

当前实现使用 64 位掩码保存查询关键词的可达性。论文实验的 `|K|<=7` 完全在
此范围内。
