// ============================================================================
// bl_time_expanded.hpp
// ----------------------------------------------------------------------------
// BL baseline for Keyword-Coverage Optimal Route Search on a temporal transit
// network.
//
// The temporal-to-static conversion follows the virtual-node construction in
// Huang, Fu and Liu, "Minimum Spanning Trees in Temporal Graphs", SIGMOD 2015,
// Section 4.2, adapted from a rooted temporal tree to a source-to-target route:
//
//   * v^t is created for every distinct arrival time t at physical station v;
//   * consecutive copies of v are joined by zero-cost waiting arcs;
//   * temporal trip e=(u,v,t_dep,t_arr,w) becomes a solid static arc from the
//     latest u-copy whose time is <= t_dep to v^{t_arr};
//   * all time copies of the query target connect to one super-sink.
//
// An ordinary directed path in the transformed graph therefore represents a
// time-respecting sequence of scheduled trips.  BL performs an exact static
// best-first top-k search on this graph.  It uses a conventional static
// shortest-cost-to-sink lower bound and a relaxed reachable-keyword upper
// bound.  It deliberately does not use Algorithms 1--3 or their temporal
// filtering/profile structures, keeping it an independent baseline.
//
// Dwell-time scoring is evaluated on the recovered scheduled-trip sequence:
// a physical station contributes only when the interval from its actual
// arrival (or query start at the source) to the next selected departure is at
// least s_v.  Multiple virtual copies of one station never count as multiple
// POIs.
// ============================================================================

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "transit_router.hpp"

namespace kcrtn {

enum class BLArcKind : unsigned char { Waiting, Trip, Sink };

struct BLStaticArc {
    int to = -1;
    double cost = 0.0;
    int temporal_edge_id = -1;
    BLArcKind kind = BLArcKind::Waiting;
};

struct BLReverseArc {
    int from = -1;
    double cost = 0.0;
};

struct BLStaticGraph {
    int source = -1;
    int sink = -1;
    std::vector<int> station_of;              // -1 for the super-sink
    std::vector<int> time_of;                 // undefined (0) for super-sink
    std::vector<std::vector<int>> times;      // sorted arrival copies by station
    std::vector<int> offset;                  // offsets into the static node set
    std::vector<std::vector<BLStaticArc>> out;
    std::vector<std::vector<BLReverseArc>> rev;
    std::size_t arc_count = 0;

    int nodeId(int station, int time_index) const {
        return offset[station] + time_index;
    }
};

inline void addBLStaticArc(BLStaticGraph& h, int from, int to, double cost,
                           int temporal_edge_id, BLArcKind kind) {
    h.out[from].push_back({to, cost, temporal_edge_id, kind});
    h.rev[to].push_back({from, cost});
    ++h.arc_count;
}

// Build the query-specific static graph.  Only trips fully contained in the
// query window are materialized.  The source gets an explicit t_alpha copy so
// that waiting before the first trip is represented.
inline BLStaticGraph buildBLStaticGraph(const Graph& g, const Query& q) {
    BLStaticGraph h;
    h.times.assign(g.n, {});
    if (q.vs < 0 || q.vs >= g.n || q.vt < 0 || q.vt >= g.n ||
        q.vs == q.vt || q.ta > q.tb) {
        return h;
    }

    h.times[q.vs].push_back(q.ta);
    for (const Edge& e : g.edges) {
        if (e.t_dep < q.ta || e.t_arr > q.tb || e.t_dep > e.t_arr) continue;
        h.times[e.v].push_back(e.t_arr);
    }

    h.offset.assign(g.n + 1, 0);
    int temporal_nodes = 0;
    for (int v = 0; v < g.n; ++v) {
        auto& tv = h.times[v];
        std::sort(tv.begin(), tv.end());
        tv.erase(std::unique(tv.begin(), tv.end()), tv.end());
        h.offset[v] = temporal_nodes;
        temporal_nodes += static_cast<int>(tv.size());
    }
    h.offset[g.n] = temporal_nodes;
    h.sink = temporal_nodes;

    const int total_nodes = temporal_nodes + 1;
    h.station_of.assign(total_nodes, -1);
    h.time_of.assign(total_nodes, 0);
    h.out.assign(total_nodes, {});
    h.rev.assign(total_nodes, {});

    for (int v = 0; v < g.n; ++v) {
        for (int i = 0; i < static_cast<int>(h.times[v].size()); ++i) {
            const int node = h.nodeId(v, i);
            h.station_of[node] = v;
            h.time_of[node] = h.times[v][i];
        }
    }

    // Virtual zero-cost arcs allow a traveler who arrived earlier to wait.
    for (int v = 0; v < g.n; ++v) {
        for (int i = 0; i + 1 < static_cast<int>(h.times[v].size()); ++i) {
            addBLStaticArc(h, h.nodeId(v, i), h.nodeId(v, i + 1), 0.0,
                           -1, BLArcKind::Waiting);
        }
    }

    // A trip starts from the latest arrival-copy not later than its departure.
    for (const Edge& e : g.edges) {
        if (e.t_dep < q.ta || e.t_arr > q.tb || e.t_dep > e.t_arr) continue;
        const auto& src_times = h.times[e.u];
        const auto& dst_times = h.times[e.v];
        auto src_it = std::upper_bound(src_times.begin(), src_times.end(), e.t_dep);
        if (src_it == src_times.begin()) continue;
        --src_it;
        auto dst_it = std::lower_bound(dst_times.begin(), dst_times.end(), e.t_arr);
        if (dst_it == dst_times.end() || *dst_it != e.t_arr) continue;
        const int src_idx = static_cast<int>(src_it - src_times.begin());
        const int dst_idx = static_cast<int>(dst_it - dst_times.begin());
        addBLStaticArc(h, h.nodeId(e.u, src_idx), h.nodeId(e.v, dst_idx),
                       e.w, e.id, BLArcKind::Trip);
    }

    // Any first arrival at the target is a complete route.
    for (int i = 0; i < static_cast<int>(h.times[q.vt].size()); ++i) {
        addBLStaticArc(h, h.nodeId(q.vt, i), h.sink, 0.0, -1,
                       BLArcKind::Sink);
    }

    auto src_it = std::lower_bound(h.times[q.vs].begin(), h.times[q.vs].end(),
                                   q.ta);
    if (src_it != h.times[q.vs].end() && *src_it == q.ta) {
        h.source = h.nodeId(q.vs,
                            static_cast<int>(src_it - h.times[q.vs].begin()));
    }
    return h;
}

inline std::vector<double> blStaticMinCostToSink(const BLStaticGraph& h) {
    std::vector<double> dist(h.out.size(), INF);
    if (h.sink < 0 || h.sink >= static_cast<int>(h.out.size())) return dist;

    using Item = std::pair<double, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    dist[h.sink] = 0.0;
    pq.push({0.0, h.sink});
    while (!pq.empty()) {
        auto [d, x] = pq.top();
        pq.pop();
        if (d > dist[x] + EPS) continue;
        for (const BLReverseArc& a : h.rev[x]) {
            const double nd = d + a.cost;
            if (nd + EPS < dist[a.from]) {
                dist[a.from] = nd;
                pq.push({nd, a.from});
            }
        }
    }
    return dist;
}

// Bit i means that some virtual copy of a station carrying query keyword i is
// statically reachable.  Ignoring dwell, budget and used-edge restrictions is
// a relaxation, so the resulting coverage bound is admissible.
inline std::vector<std::uint64_t> blReachableQueryKeywordMask(
        const BLStaticGraph& h, const POIProfile& poi, const Query& q) {
    std::vector<std::uint64_t> mask(h.out.size(), 0);
    const int qn = q.n_keywords();
    if (qn > 64) return mask;  // current experiments use at most seven

    for (int i = 0; i < qn; ++i) {
        const int kw = q.K[i];
        std::vector<unsigned char> seen(h.out.size(), 0);
        std::queue<int> bfs;
        for (int node = 0; node < static_cast<int>(h.out.size()); ++node) {
            const int v = h.station_of[node];
            if (v >= 0 && v < static_cast<int>(poi.cov.size()) &&
                kw >= 0 && kw < static_cast<int>(poi.cov[v].size()) &&
                poi.cov[v][kw] > 0.0) {
                seen[node] = 1;
                bfs.push(node);
            }
        }
        while (!bfs.empty()) {
            const int x = bfs.front();
            bfs.pop();
            mask[x] |= (std::uint64_t{1} << i);
            for (const BLReverseArc& a : h.rev[x]) {
                if (!seen[a.from]) {
                    seen[a.from] = 1;
                    bfs.push(a.from);
                }
            }
        }
    }
    return mask;
}

struct BLResult {
    std::vector<Route> routes;
    long long expanded_labels = 0;
    bool timed_out = false;
    bool no_route = false;       // asserted only after a complete non-timeout run
    std::size_t static_nodes = 0;
    std::size_t static_arcs = 0;
};

namespace bl_detail {

struct PathLink {
    int edge_id = -1;
    int length = 0;
    std::shared_ptr<const PathLink> previous;
};

struct Label {
    int node = -1;
    int station_entry_time = 0;  // actual arrival, unchanged by virtual waits
    double budget = 0.0;
    std::vector<int> visited_pois;
    std::vector<double> coverage;  // aligned with q.K
    double kc = 0.0;
    double upper = 0.0;
    std::shared_ptr<const PathLink> path;
    std::uint64_t serial = 0;
};

struct LabelCmp {
    bool operator()(const std::shared_ptr<Label>& a,
                    const std::shared_ptr<Label>& b) const {
        if (std::fabs(a->upper - b->upper) > EPS)
            return a->upper < b->upper;
        return a->serial > b->serial;
    }
};

inline bool pathContains(const std::shared_ptr<const PathLink>& path,
                         int edge_id) {
    for (auto p = path; p; p = p->previous)
        if (p->edge_id == edge_id) return true;
    return false;
}

inline std::vector<int> recoverPath(std::shared_ptr<const PathLink> path) {
    std::vector<int> edges;
    if (path) edges.reserve(path->length);
    for (auto p = path; p; p = p->previous) edges.push_back(p->edge_id);
    std::reverse(edges.begin(), edges.end());
    return edges;
}

inline bool alreadyVisited(const std::vector<int>& visited, int v) {
    return std::find(visited.begin(), visited.end(), v) != visited.end();
}

inline void confirmVisit(int v, int dwell, const POIProfile& poi,
                         const Query& q, std::vector<int>& visited,
                         std::vector<double>& coverage, double& kc) {
    if (v < 0 || v >= static_cast<int>(poi.s.size()) || poi.s[v] <= 0 ||
        dwell < poi.s[v] || alreadyVisited(visited, v)) {
        return;
    }
    visited.push_back(v);
    for (int i = 0; i < q.n_keywords(); ++i) {
        const int kw = q.K[i];
        if (kw < 0 || kw >= static_cast<int>(poi.cov[v].size())) continue;
        const double p = poi.cov[v][kw];
        if (p <= 0.0) continue;
        const double next = 1.0 - (1.0 - coverage[i]) * (1.0 - p);
        kc += q.lambda[i] * (next - coverage[i]);
        coverage[i] = next;
    }
}

inline double upperBound(int node, double kc,
                         const std::vector<double>& coverage,
                         const std::vector<std::uint64_t>& masks,
                         const Query& q) {
    double ub = kc;
    const std::uint64_t m = masks[node];
    for (int i = 0; i < q.n_keywords(); ++i) {
        if ((m >> i) & std::uint64_t{1})
            ub += q.lambda[i] * (1.0 - coverage[i]);
    }
    return ub;
}

}  // namespace bl_detail

// Exact BL top-k search on the transformed static graph.  Conversion,
// shortest-path preprocessing and keyword-mask preprocessing all occur inside
// this call and must therefore be included in the externally measured time.
inline BLResult blTimeExpandedTopK(const Graph& g, const POIProfile& poi,
                                   const Query& q,
                                   int64 time_limit_ms = 300000) {
    BLResult result;
    const auto started = std::chrono::steady_clock::now();
    auto elapsedMs = [&]() -> int64 {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - started)
            .count();
    };

    BLStaticGraph h = buildBLStaticGraph(g, q);
    result.static_nodes = h.out.size();
    result.static_arcs = h.arc_count;
    if (h.source < 0 || h.sink < 0 || h.out.empty() || q.n_keywords() > 64) {
        result.no_route = true;
        return result;
    }

    std::vector<double> min_cost = blStaticMinCostToSink(h);
    if (min_cost[h.source] >= INF / 2 ||
        min_cost[h.source] > q.Delta + EPS) {
        result.no_route = true;
        return result;
    }
    if (time_limit_ms >= 0 && elapsedMs() > time_limit_ms) {
        result.timed_out = true;
        return result;
    }

    std::vector<std::uint64_t> keyword_mask =
        blReachableQueryKeywordMask(h, poi, q);
    if (time_limit_ms >= 0 && elapsedMs() > time_limit_ms) {
        result.timed_out = true;
        return result;
    }

    using bl_detail::Label;
    using bl_detail::LabelCmp;
    using bl_detail::PathLink;
    std::priority_queue<std::shared_ptr<Label>,
                        std::vector<std::shared_ptr<Label>>, LabelCmp>
        queue;
    std::uint64_t next_serial = 0;

    auto initial = std::make_shared<Label>();
    initial->node = h.source;
    initial->station_entry_time = q.ta;
    initial->coverage.assign(q.n_keywords(), 0.0);
    initial->upper = bl_detail::upperBound(
        initial->node, 0.0, initial->coverage, keyword_mask, q);
    initial->serial = next_serial++;
    queue.push(initial);

    std::vector<Route> top_k;
    auto betterRoute = [](const Route& a, const Route& b) {
        if (std::fabs(a.kc - b.kc) > EPS) return a.kc > b.kc;
        return a.edges < b.edges;
    };
    auto insertRoute = [&](Route route) {
        for (const Route& existing : top_k)
            if (existing.edges == route.edges) return;
        top_k.push_back(std::move(route));
        std::sort(top_k.begin(), top_k.end(), betterRoute);
        if (static_cast<int>(top_k.size()) > q.k) top_k.pop_back();
    };

    while (!queue.empty()) {
        if (time_limit_ms >= 0 && elapsedMs() > time_limit_ms) {
            result.timed_out = true;
            break;
        }
        std::shared_ptr<Label> current = queue.top();
        queue.pop();
        ++result.expanded_labels;

        const double threshold =
            static_cast<int>(top_k.size()) == q.k ? top_k.back().kc : 0.0;
        if (static_cast<int>(top_k.size()) == q.k &&
            current->upper < threshold - EPS) {
            // queue is ordered by upper bound, so all remaining labels are no
            // better; equality is retained for lexicographic tie handling.
            break;
        }

        const int current_station = h.station_of[current->node];
        for (const BLStaticArc& arc : h.out[current->node]) {
            if (arc.kind == BLArcKind::Sink) continue;

            if (arc.kind == BLArcKind::Waiting) {
                if (current->budget + min_cost[arc.to] > q.Delta + EPS) continue;
                auto next = std::make_shared<Label>(*current);
                next->node = arc.to;
                next->upper = bl_detail::upperBound(
                    next->node, next->kc, next->coverage, keyword_mask, q);
                next->serial = next_serial++;
                if (static_cast<int>(top_k.size()) == q.k &&
                    next->upper < top_k.back().kc - EPS)
                    continue;
                queue.push(std::move(next));
                continue;
            }

            const int edge_id = arc.temporal_edge_id;
            if (edge_id < 0 || edge_id >= static_cast<int>(g.edges.size())) continue;
            const Edge& e = g.edges[edge_id];
            if (e.u != current_station || e.t_dep < current->station_entry_time ||
                e.t_dep < q.ta || e.t_arr > q.tb ||
                bl_detail::pathContains(current->path, edge_id)) {
                continue;
            }

            const double next_budget = current->budget + e.w;
            if (next_budget > q.Delta + EPS) continue;

            auto next = std::make_shared<Label>(*current);
            bl_detail::confirmVisit(
                current_station, e.t_dep - current->station_entry_time, poi, q,
                next->visited_pois, next->coverage, next->kc);
            next->path = std::make_shared<PathLink>(PathLink{
                edge_id, current->path ? current->path->length + 1 : 1,
                current->path});
            next->node = arc.to;
            next->station_entry_time = e.t_arr;
            next->budget = next_budget;

            if (e.v == q.vt) {
                bl_detail::confirmVisit(q.vt, q.tb - e.t_arr, poi, q,
                                        next->visited_pois, next->coverage,
                                        next->kc);
                Route route;
                route.edges = bl_detail::recoverPath(next->path);
                route.kc = next->kc;
                route.bs = next->budget;
                route.t_dep = route.edges.empty()
                                  ? q.ta
                                  : g.edges[route.edges.front()].t_dep;
                route.t_arr = e.t_arr;
                insertRoute(std::move(route));
                continue;
            }

            if (min_cost[next->node] >= INF / 2 ||
                next->budget + min_cost[next->node] > q.Delta + EPS) {
                continue;
            }
            next->upper = bl_detail::upperBound(
                next->node, next->kc, next->coverage, keyword_mask, q);
            next->serial = next_serial++;
            if (static_cast<int>(top_k.size()) == q.k &&
                next->upper < top_k.back().kc - EPS)
                continue;
            queue.push(std::move(next));
        }
    }

    result.routes = std::move(top_k);
    result.no_route = !result.timed_out && result.routes.empty();
    return result;
}

struct BLRouteCheck {
    bool feasible = false;
    std::string reason;
    double recomputed_budget = 0.0;
    double recomputed_kc = 0.0;
};

// Independent post-search validation used by the experiment driver.  It does
// not trust the transformed path or the search label's cached score.
inline BLRouteCheck validateBLRoute(const Graph& g, const POIProfile& poi,
                                    const Query& q, const Route& route) {
    BLRouteCheck check;
    if (route.edges.empty()) {
        check.reason = "empty route";
        return check;
    }
    int station = q.vs;
    int time = q.ta;
    std::vector<int> used;
    used.reserve(route.edges.size());
    for (std::size_t i = 0; i < route.edges.size(); ++i) {
        const int edge_id = route.edges[i];
        if (edge_id < 0 || edge_id >= static_cast<int>(g.edges.size())) {
            check.reason = "invalid edge id";
            return check;
        }
        if (std::find(used.begin(), used.end(), edge_id) != used.end()) {
            check.reason = "repeated scheduled edge";
            return check;
        }
        used.push_back(edge_id);
        const Edge& e = g.edges[edge_id];
        if (e.u != station) {
            check.reason = "disconnected edge sequence";
            return check;
        }
        if (e.t_dep < time) {
            check.reason = "missed departure";
            return check;
        }
        if (e.t_dep < q.ta || e.t_arr > q.tb) {
            check.reason = "outside query window";
            return check;
        }
        if (station == q.vt) {
            check.reason = "route continues after first target arrival";
            return check;
        }
        check.recomputed_budget += e.w;
        station = e.v;
        time = e.t_arr;
    }
    if (station != q.vt) {
        check.reason = "route does not end at target";
        return check;
    }
    if (check.recomputed_budget > q.Delta + EPS) {
        check.reason = "budget exceeded";
        return check;
    }
    check.recomputed_kc = evaluateRouteKC(g, poi, q, route.edges);
    if (std::fabs(check.recomputed_budget - route.bs) > 1e-6) {
        check.reason = "cached budget mismatch";
        return check;
    }
    if (std::fabs(check.recomputed_kc - route.kc) > 1e-6) {
        check.reason = "cached coverage mismatch";
        return check;
    }
    check.feasible = true;
    check.reason = "OK";
    return check;
}

}  // namespace kcrtn

