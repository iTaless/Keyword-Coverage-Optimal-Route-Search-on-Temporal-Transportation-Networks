// ============================================================================
// transit_router.hpp
// ----------------------------------------------------------------------------
// C++17 reference implementation of the paper
//   "Keyword-Coverage Optimal Route Search on Temporal Transportation
//   Networks", Ge & Ji, Mathematics (2026).
//
// Implements: event graph; Algorithm 1 (temporal reachability EA/LD);
// Algorithm 2 (time-dependent minimum remaining budget D(v,t) via reverse
// multi-source Dijkstra with successor pointers); 4.1.4 filtering G->G_time->G_s;
// keyword-reachable masks and the coverage upper bound h(P); Algorithm 3
// (exact best-first top-k); Algorithm 4 (bounded greedy AP with certified
// feasible completion); and a small-instance exhaustive enumerator used only
// to validate the exact implementation.  The experimental BL baseline is in
// bl_time_expanded.hpp.
//
// Adapted for the Austin dataset supplied by the authors:
//   * edge weight w is a double (fare, ~0.69..7.55)
//   * times are seconds since epoch-of-feed (several service days)
//   * station labels are 0/1 keyword membership; s_v marks POI presence,
//     while the visit threshold is the query-level Query::dwell value
//   * per-query route outputs are exposed so that feasibility checks,
//     distinct-route counts and KC ratios can be reproduced
// ============================================================================

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <queue>
#include <random>
#include <utility>
#include <vector>

namespace kcrtn {

using int64 = std::int64_t;

constexpr double INF = 1e18;
constexpr double EPS = 1e-9;

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

struct Edge {
    int id = -1;
    int original_id = -1;       // stable id in the unfiltered input graph
    int u = -1, v = -1;
    int t_dep = 0, t_arr = 0;   // seconds
    double w = 0.0;             // fare, nonnegative
};

struct POIProfile {
    std::vector<std::vector<double>> cov;  // [n][n_keywords]
    std::vector<int> s;                    // [n] positive = POI present, 0 = none
    int n_keywords() const { return cov.empty() ? 0 : (int)cov[0].size(); }
};

struct Graph {
    int n = 0;
    std::vector<Edge> edges;
    std::vector<std::vector<int>> out;
    std::vector<std::vector<int>> in;
    void init(int num) { n = num; out.assign(n, {}); in.assign(n, {}); }
    int addEdge(Edge e) {
        e.id = (int)edges.size();
        if (e.original_id < 0) e.original_id = e.id;
        edges.push_back(e);
        out[e.u].push_back(e.id);
        in[e.v].push_back(e.id);
        return e.id;
    }
};

struct Query {
    int vs = -1, vt = -1;
    std::vector<int> K;          // keyword ids
    std::vector<double> lambda;  // weights, sum = 1
    double Delta = 0.0;          // budget
    int ta = 0, tb = 0;          // window in seconds
    int dwell = 0;               // query-level dwell threshold in seconds
    int k = 1;
    int n_keywords() const { return (int)K.size(); }
};

struct Route {
    std::vector<int> edges;
    double kc = 0.0;
    double bs = 0.0;
    int t_dep = 0, t_arr = 0;
    bool operator==(const Route& o) const { return edges == o.edges; }
};

// ---------------------------------------------------------------------------
// Event graph
// ---------------------------------------------------------------------------

struct EArc { int to; int edge_id; bool trip; };

struct EventGraph {
    int n = 0;
    std::vector<int> v_of, time_of;
    std::vector<std::vector<int>> times;  // times[v] sorted distinct
    std::vector<int> offset;
    std::vector<std::vector<EArc>> fwd, rev;
    int nodeId(int v, int idx) const { return offset[v] + idx; }
    int idxAt(int v, int t) const {
        auto& tv = times[v];
        auto it = std::lower_bound(tv.begin(), tv.end(), t);
        if (it == tv.end()) return (int)tv.size() - 1;
        return (int)(it - tv.begin());
    }
};

inline EventGraph buildEventGraph(const Graph& g, int ta, int tb) {
    EventGraph H;
    H.times.assign(g.n, {});
    for (int v = 0; v < g.n; ++v) { H.times[v].push_back(ta); H.times[v].push_back(tb); }
    for (const Edge& e : g.edges) {
        if (e.t_dep < ta || e.t_arr > tb) continue;
        H.times[e.u].push_back(e.t_dep);
        H.times[e.v].push_back(e.t_arr);
    }
    H.offset.assign(g.n + 1, 0);
    int tot = 0;
    for (int v = 0; v < g.n; ++v) {
        auto& tv = H.times[v];
        std::sort(tv.begin(), tv.end());
        tv.erase(std::unique(tv.begin(), tv.end()), tv.end());
        H.offset[v] = tot;
        tot += (int)tv.size();
    }
    H.offset[g.n] = tot;
    H.n = tot;
    H.v_of.assign(H.n, -1);
    H.time_of.assign(H.n, 0);
    for (int v = 0; v < g.n; ++v)
        for (int i = 0; i < (int)H.times[v].size(); ++i) {
            int nd = H.offset[v] + i;
            H.v_of[nd] = v;
            H.time_of[nd] = H.times[v][i];
        }
    H.fwd.assign(H.n, {});
    H.rev.assign(H.n, {});
    for (int v = 0; v < g.n; ++v)
        for (int i = 0; i + 1 < (int)H.times[v].size(); ++i) {
            int a = H.nodeId(v, i), b = H.nodeId(v, i + 1);
            H.fwd[a].push_back({b, -1, false});
            H.rev[b].push_back({a, -1, false});
        }
    for (const Edge& e : g.edges) {
        if (e.t_dep < ta || e.t_arr > tb) continue;
        int a = H.nodeId(e.u, H.idxAt(e.u, e.t_dep));
        int b = H.nodeId(e.v, H.idxAt(e.v, e.t_arr));
        H.fwd[a].push_back({b, e.id, true});
        H.rev[b].push_back({a, e.id, true});
    }
    return H;
}

// ---------------------------------------------------------------------------
// Algorithm 1: temporal reachability (EA / LD)
// ---------------------------------------------------------------------------

struct Reachability {
    std::vector<int> EA, LD;
};

inline Reachability algorithm1Reachability(const Graph& g, int vs, int vt,
                                           int ta, int tb) {
    EventGraph H = buildEventGraph(g, ta, tb);
    const int INFI = std::numeric_limits<int>::max();
    const int NINF = std::numeric_limits<int>::min();
    Reachability R;
    R.EA.assign(g.n, INFI);
    R.LD.assign(g.n, NINF);

    {
        int src = H.nodeId(vs, H.idxAt(vs, ta));
        std::vector<char> vis(H.n, 0);
        std::queue<int> q;
        vis[src] = 1; q.push(src);
        while (!q.empty()) {
            int x = q.front(); q.pop();
            int v = H.v_of[x];
            R.EA[v] = std::min(R.EA[v], H.time_of[x]);
            for (const EArc& a : H.fwd[x])
                if (!vis[a.to]) { vis[a.to] = 1; q.push(a.to); }
        }
    }
    {
        int src = H.nodeId(vt, H.idxAt(vt, tb));
        std::vector<char> vis(H.n, 0);
        std::queue<int> q;
        vis[src] = 1; q.push(src);
        while (!q.empty()) {
            int x = q.front(); q.pop();
            int v = H.v_of[x];
            R.LD[v] = std::max(R.LD[v], H.time_of[x]);
            for (const EArc& a : H.rev[x])
                if (!vis[a.to]) { vis[a.to] = 1; q.push(a.to); }
        }
    }
    return R;
}

// ---------------------------------------------------------------------------
// Algorithm 2: minimum remaining budget profile
// ---------------------------------------------------------------------------

struct BudgetProfile {
    int n = 0;
    std::vector<std::vector<int>> times;
    std::vector<std::vector<double>> dist;   // dist[v][idx]
    std::vector<std::vector<int>> succ;
    std::vector<std::vector<int>> succEdge;
    int ta = 0, tb = 0;
    int vt = -1;
    double query(int v, int t) const {
        auto& tv = times[v];
        auto it = std::lower_bound(tv.begin(), tv.end(), t);
        int idx = (it == tv.end()) ? (int)tv.size() - 1 : (int)(it - tv.begin());
        return dist[v][idx];
    }
    double mb(int v) const { return query(v, ta); }
};

struct Profile2 {
    BudgetProfile P;
    std::vector<int> node_v, node_idx, offset;
    double query(int v, int t) const { return P.query(v, t); }
    double mb(int v) const { return P.mb(v); }
    std::vector<int> reconstruct(int v, int t) const {
        auto& tv = P.times[v];
        auto it = std::lower_bound(tv.begin(), tv.end(), t);
        int idx = (it == tv.end()) ? (int)tv.size() - 1 : (int)(it - tv.begin());
        int cur = offset[v] + idx;
        std::vector<int> out;
        int guard = 0;
        while (cur >= 0 && guard++ <= (int)node_v.size() + 1) {
            int vv = node_v[cur], ii = node_idx[cur];
            int s = P.succ[vv][ii];
            if (s < 0) break;
            int se = P.succEdge[vv][ii];
            if (se >= 0) out.push_back(se);
            cur = s;
        }
        return out;
    }
};

inline Profile2 algorithm2BudgetProfile(const Graph& A, int vt, int ta, int tb) {
    EventGraph H = buildEventGraph(A, ta, tb);
    Profile2 prof;
    BudgetProfile& P = prof.P;
    P.n = A.n;
    P.times = H.times;
    P.ta = ta; P.tb = tb; P.vt = vt;
    P.dist.assign(A.n, {});
    P.succ.assign(A.n, {});
    P.succEdge.assign(A.n, {});
    for (int v = 0; v < A.n; ++v) {
        int m = (int)H.times[v].size();
        P.dist[v].assign(m, INF);
        P.succ[v].assign(m, -1);
        P.succEdge[v].assign(m, -1);
    }
    prof.node_v = H.v_of;
    prof.node_idx.assign(H.n, 0);
    {
        std::vector<int> ptr(A.n, 0);
        for (int nd = 0; nd < H.n; ++nd) {
            int v = H.v_of[nd];
            prof.node_idx[nd] = ptr[v]++;
        }
    }
    prof.offset.assign(A.n + 1, 0);
    for (int v = 0; v < A.n; ++v)
        prof.offset[v + 1] = prof.offset[v] + (int)H.times[v].size();

    using QItem = std::pair<double, int>;
    std::priority_queue<QItem, std::vector<QItem>, std::greater<QItem>> pq;
    std::vector<char> settled(H.n, 0);
    for (int i = 0; i < (int)H.times[vt].size(); ++i) {
        int nd = H.nodeId(vt, i);
        P.dist[vt][i] = 0.0;
        pq.push({0.0, nd});
    }
    while (!pq.empty()) {
        auto [d, x] = pq.top(); pq.pop();
        if (settled[x]) continue;
        settled[x] = 1;
        for (const EArc& a : H.rev[x]) {
            int y = a.to;
            if (settled[y]) continue;
            double w = a.trip ? A.edges[a.edge_id].w : 0.0;
            int yv = H.v_of[y], yidx = prof.node_idx[y];
            if (d + w < P.dist[yv][yidx] - EPS) {
                P.dist[yv][yidx] = d + w;
                P.succ[yv][yidx] = x;
                P.succEdge[yv][yidx] = a.trip ? a.edge_id : -1;
                pq.push({d + w, y});
            }
        }
    }
    return prof;
}

// ---------------------------------------------------------------------------
// 4.1.4 filtering
// ---------------------------------------------------------------------------

inline Graph filterToGtime(const Graph& g, int vs, int vt, int ta, int tb) {
    Reachability R = algorithm1Reachability(g, vs, vt, ta, tb);
    Graph gt;
    gt.init(g.n);
    std::vector<char> keepVertex(g.n, 1);
    for (int v = 0; v < g.n; ++v) {
        if (v == vs || v == vt) continue;
        if (R.EA[v] > R.LD[v]) keepVertex[v] = 0;
    }
    for (const Edge& e : g.edges) {
        if (e.t_dep < ta || e.t_arr > tb) continue;
        if (!keepVertex[e.u] || !keepVertex[e.v]) continue;
        if (e.t_dep < R.EA[e.u]) continue;
        if (e.t_dep > R.LD[e.u]) continue;
        gt.addEdge(e);
    }
    return gt;
}

inline Graph filterToGs(const Graph& gtime, const Profile2& prof, int vs,
                        int vt, double Delta) {
    Graph gs;
    gs.init(gtime.n);
    std::vector<char> keep(gtime.n, 1);
    for (int v = 0; v < gtime.n; ++v) {
        if (v == vs || v == vt) continue;
        if (prof.mb(v) > Delta + EPS) keep[v] = 0;
    }
    for (const Edge& e : gtime.edges)
        if (keep[e.u] && keep[e.v]) gs.addEdge(e);
    return gs;
}

// ---------------------------------------------------------------------------
// Keyword masks and upper bound
// ---------------------------------------------------------------------------

inline std::vector<std::vector<uint64_t>> reachableKeywordMasks(
        const Graph& gs, const POIProfile& poi, int n_keywords) {
    int n = gs.n;
    int words = (n_keywords + 63) / 64;
    std::vector<std::vector<uint64_t>> mask(n, std::vector<uint64_t>(words, 0));
    for (int k = 0; k < n_keywords; ++k) {
        std::vector<int> seeds;
        for (int v = 0; v < n; ++v)
            if (poi.cov[v][k] > 0) seeds.push_back(v);
        if (seeds.empty()) continue;
        std::vector<char> vis(n, 0);
        std::queue<int> q;
        for (int s : seeds) { vis[s] = 1; q.push(s); }
        while (!q.empty()) {
            int x = q.front(); q.pop();
            mask[x][k >> 6] |= (uint64_t(1) << (k & 63));
            for (int eid : gs.in[x]) {
                int p = gs.edges[eid].u;
                if (!vis[p]) { vis[p] = 1; q.push(p); }
            }
        }
    }
    return mask;
}

inline double coverageUpperBound(int v, const std::vector<double>& c,
                                 const Query& Q,
                                 const std::vector<std::vector<uint64_t>>& masks) {
    double h = 0.0;
    for (int i = 0; i < Q.n_keywords(); ++i) {
        int k = Q.K[i];
        if ((masks[v][k >> 6] >> (k & 63)) & 1)
            h += Q.lambda[i] * (1.0 - c[i]);
    }
    return h;
}

// ---------------------------------------------------------------------------
// Route evaluation (dwell-time rule + query-weighted KC)
// ---------------------------------------------------------------------------

inline double evaluateRouteKC(const Graph& g, const POIProfile& poi,
                              const Query& Q, const std::vector<int>& edges) {
    if (edges.empty()) return 0.0;
    int n_keywords = poi.n_keywords();
    std::vector<double> c(n_keywords, 0.0);
    std::vector<char> visited(g.n, 0);
    auto confirm = [&](int v, int dwell) {
        if (visited[v] || poi.s[v] <= 0 || dwell < Q.dwell) return;
        visited[v] = 1;
        for (int k = 0; k < n_keywords; ++k)
            if (poi.cov[v][k] > 0)
                c[k] = 1.0 - (1.0 - c[k]) * (1.0 - poi.cov[v][k]);
    };
    confirm(g.edges[edges[0]].u, g.edges[edges[0]].t_dep - Q.ta);
    for (int i = 0; i < (int)edges.size(); ++i) {
        const Edge& e = g.edges[edges[i]];
        int dwell = (i + 1 < (int)edges.size())
                        ? g.edges[edges[i + 1]].t_dep - e.t_arr
                        : Q.tb - e.t_arr;
        confirm(e.v, dwell);
    }
    double kc = 0.0;
    for (int i = 0; i < Q.n_keywords(); ++i) kc += Q.lambda[i] * c[Q.K[i]];
    return kc;
}

// ---------------------------------------------------------------------------
// Algorithm 3: exact best-first top-k
// ---------------------------------------------------------------------------

struct ExactResult {
    std::vector<Route> routes;
    long long expanded = 0;
    bool no_route = false;
    bool timed_out = false;
};

namespace detail {
struct Label {
    std::vector<int> edges;
    int v = -1;
    int t = 0;
    double b = 0.0;
    std::vector<char> visited;
    std::vector<double> c;
    double kc = 0.0;
    double f = 0.0;
    bool terminal = false;
};
struct LabelCmp {
    bool operator()(const std::shared_ptr<Label>& a,
                    const std::shared_ptr<Label>& b) const {
        if (std::fabs(a->f - b->f) > EPS) return a->f < b->f;
        return a->edges > b->edges;
    }
};
}  // namespace detail

inline ExactResult exactTopK(const Graph& gs, const Graph& gtime,
                             const Profile2& Dgtime, const POIProfile& poi,
                             const Query& Q,
                             const std::vector<std::vector<uint64_t>>& masks,
                             int64 time_limit_ms = -1) {
    (void)gtime;  // retained in the interface to mirror Algorithm 3 notation
    ExactResult res;
    const int n = gs.n;
    const int q = Q.n_keywords();
    if (Dgtime.query(Q.vs, Q.ta) > Q.Delta + EPS) { res.no_route = true; return res; }

    auto nowMs = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    };
    int64 t0 = nowMs();

    using detail::Label;
    using detail::LabelCmp;
    std::priority_queue<std::shared_ptr<Label>, std::vector<std::shared_ptr<Label>>,
                        LabelCmp> pq;

    auto makeLabel = [&](int vs) {
        auto L = std::make_shared<Label>();
        L->v = vs; L->t = Q.ta; L->b = 0.0;
        L->visited.assign(n, 0);
        L->c.assign(q, 0.0);
        L->kc = 0.0;
        L->f = L->kc + coverageUpperBound(vs, L->c, Q, masks);
        L->terminal = (vs == Q.vt);
        return L;
    };
    pq.push(makeLabel(Q.vs));

    std::vector<Route> topK;
    auto betterRoute = [&](const Route& x, const Route& y) {
        if (std::fabs(x.kc - y.kc) > EPS) return x.kc > y.kc;
        return x.edges < y.edges;
    };
    auto insertTopK = [&](Route r) {
        for (auto& x : topK)
            if (x.edges == r.edges) return;
        topK.push_back(r);
        std::sort(topK.begin(), topK.end(), betterRoute);
        if ((int)topK.size() > Q.k) topK.pop_back();
    };

    while (!pq.empty()) {
        ++res.expanded;
        if (time_limit_ms >= 0 && (res.expanded & 1023) == 0 &&
            nowMs() - t0 > time_limit_ms) {
            res.timed_out = true;
            break;
        }
        auto L = pq.top(); pq.pop();
        double tau = 0.0;
        if ((int)topK.size() == Q.k) tau = topK.back().kc;
        if ((int)topK.size() == Q.k && L->f < tau - EPS) continue;

        if (L->terminal) {
            Route r;
            r.edges = L->edges;
            r.kc = L->kc;
            r.bs = L->b;
            r.t_dep = L->edges.empty() ? Q.ta : gs.edges[L->edges.front()].t_dep;
            r.t_arr = L->t;
            insertTopK(r);
            continue;
        }

        for (int eid : gs.out[L->v]) {
            const Edge& e = gs.edges[eid];
            if (std::find(L->edges.begin(), L->edges.end(), eid) != L->edges.end())
                continue;
            if (e.t_dep < L->t) continue;
            if (e.t_arr > Q.tb) continue;

            auto P2 = std::make_shared<Label>(*L);
            P2->edges.push_back(eid);
            if (!L->visited[L->v] && poi.s[L->v] > 0 &&
                e.t_dep - L->t >= Q.dwell) {
                P2->visited[L->v] = 1;
                double add = 0.0;
                for (int i = 0; i < q; ++i) {
                    int k = Q.K[i];
                    if (poi.cov[L->v][k] > 0) {
                        add += Q.lambda[i] * poi.cov[L->v][k] * (1.0 - L->c[i]);
                        P2->c[i] = 1.0 - (1.0 - P2->c[i]) * (1.0 - poi.cov[L->v][k]);
                    }
                }
                P2->kc = L->kc + add;
            }
            P2->v = e.v;
            P2->t = e.t_arr;
            P2->b = L->b + e.w;
            if (e.v == Q.vt && !P2->visited[Q.vt] && poi.s[Q.vt] > 0 &&
                Q.tb - e.t_arr >= Q.dwell) {
                P2->visited[Q.vt] = 1;
                double add = 0.0;
                for (int i = 0; i < q; ++i) {
                    int k = Q.K[i];
                    if (poi.cov[Q.vt][k] > 0) {
                        add += Q.lambda[i] * poi.cov[Q.vt][k] * (1.0 - P2->c[i]);
                        P2->c[i] = 1.0 - (1.0 - P2->c[i]) * (1.0 - poi.cov[Q.vt][k]);
                    }
                }
                P2->kc += add;
            }
            if (P2->b + Dgtime.query(P2->v, P2->t) > Q.Delta + EPS) continue;
            P2->terminal = (P2->v == Q.vt);
            P2->f = P2->terminal ? P2->kc
                                 : P2->kc + coverageUpperBound(P2->v, P2->c, Q, masks);
            if ((int)topK.size() == Q.k) {
                tau = topK.back().kc;
                if (P2->f < tau - EPS) continue;
            }
            pq.push(P2);
        }
    }
    res.routes = topK;
    return res;
}

// ---------------------------------------------------------------------------
// Algorithm 4: bounded greedy search with certified feasible completion (AP)
// ---------------------------------------------------------------------------

struct APResult {
    bool found = false;
    Route route;
    int rounds_used = 0;
};

inline std::vector<int> sampleRanks(int m, int r, int seed, std::mt19937& rng) {
    std::vector<int> ranks;
    if (m <= 0 || r <= 0) return ranks;
    r = std::min(r, m);
    if (r == 1) { ranks.push_back(0); return ranks; }
    if (seed < 0) {
        for (int i = 0; i < r; ++i)
            ranks.push_back((int)((int64)i * (m - 1) / (r - 1)));
    } else {
        for (int i = 0; i < r; ++i) {
            int lo = (int)((int64)i * m / r);
            int hi = (int)((int64)(i + 1) * m / r) - 1;
            if (hi < lo) hi = lo;
            int pick = lo + (int)(rng() % (uint32_t)(hi - lo + 1));
            ranks.push_back(pick);
        }
    }
    std::sort(ranks.begin(), ranks.end());
    ranks.erase(std::unique(ranks.begin(), ranks.end()), ranks.end());
    return ranks;
}

inline APResult apSearch(const Graph& gs, const POIProfile& poi, const Query& Q,
                         const Profile2& Ds,
                         const std::vector<std::vector<int>>& dep_sorted,
                         int rho, int L, int seed, std::mt19937& rng) {
    APResult res;
    const int n = gs.n;
    const int q = Q.n_keywords();
    if (Ds.query(Q.vs, Q.ta) > Q.Delta + EPS) return res;

    std::vector<int> P_edges;
    std::vector<char> visited(n, 0);
    std::vector<double> c(q, 0.0);
    double kc = 0.0;
    int v = Q.vs, t = Q.ta;
    double b = 0.0;

    auto confirmVisit = [&](int vv, int dwell) {
        if (visited[vv] || poi.s[vv] <= 0 || dwell < Q.dwell) return;
        visited[vv] = 1;
        double add = 0.0;
        for (int i = 0; i < q; ++i) {
            int k = Q.K[i];
            if (poi.cov[vv][k] > 0) {
                add += Q.lambda[i] * poi.cov[vv][k] * (1.0 - c[i]);
                c[i] = 1.0 - (1.0 - c[i]) * (1.0 - poi.cov[vv][k]);
            }
        }
        kc += add;
    };
    auto continuationCost = [&](const std::vector<int>& ce) {
        double s = 0.0;
        for (int eid : ce) s += gs.edges[eid].w;
        return s;
    };
    auto scoreOf = [&](int eid, const std::vector<int>& Ce) {
        std::vector<int> full = P_edges;
        full.push_back(eid);
        full.insert(full.end(), Ce.begin(), Ce.end());
        double kcfull = evaluateRouteKC(gs, poi, Q, full);
        double denom = 1.0 + gs.edges[eid].w + continuationCost(Ce);
        return (kcfull - kc) / denom;
    };

    int rounds = 0;
    for (int j = 0; j < L; ++j) {
        rounds = j + 1;
        if (v == Q.vt) break;
        const auto& Av = dep_sorted[v];
        auto it = std::lower_bound(Av.begin(), Av.end(), t,
                                   [&](int eid, int tv) { return gs.edges[eid].t_dep < tv; });
        int first = (int)(it - Av.begin());
        int m = (int)Av.size() - first;
        int r = std::min(rho, m);
        std::vector<int> ranks = sampleRanks(m, r, seed, rng);
        std::vector<int> cands;
        for (int rk : ranks) cands.push_back(Av[first + rk]);
        std::vector<int> Ccur = Ds.reconstruct(v, t);
        if (!Ccur.empty()) cands.push_back(Ccur.front());
        std::sort(cands.begin(), cands.end());
        cands.erase(std::unique(cands.begin(), cands.end()), cands.end());

        int best_e = -1;
        std::vector<int> best_Ce;
        double best_score = -1e18;
        for (int eid : cands) {
            const Edge& e = gs.edges[eid];
            if (std::find(P_edges.begin(), P_edges.end(), eid) != P_edges.end()) continue;
            if (e.t_dep < t) continue;
            if (e.t_arr > Q.tb) continue;
            if (b + e.w + Ds.query(e.v, e.t_arr) > Q.Delta + EPS) continue;
            std::vector<int> Ce = Ds.reconstruct(e.v, e.t_arr);
            bool disjoint = true;
            for (int x : Ce) {
                if (x == eid ||
                    std::find(P_edges.begin(), P_edges.end(), x) != P_edges.end()) {
                    disjoint = false; break;
                }
            }
            if (!disjoint) continue;
            double sc = scoreOf(eid, Ce);
            // Recorded seed breaks score ties (Section 4.4.3): a random
            // coin among equal scores yields distinct routes across the
            // J restarts, which is what the top-k distinct-route count
            // reports. Deterministic earliest-arrival tie-breaking would
            // collapse every restart to the same route.
            bool better = (sc > best_score + EPS) ||
                          (std::fabs(sc - best_score) <= EPS && (rng() % 2 == 0));
            if (better) { best_e = eid; best_Ce = Ce; best_score = sc; }
        }
        if (best_e < 0) break;
        const Edge& e = gs.edges[best_e];
        confirmVisit(v, e.t_dep - t);
        P_edges.push_back(best_e);
        b += e.w;
        v = e.v; t = e.t_arr;
        if (v == Q.vt) confirmVisit(v, Q.tb - t);
    }

    std::vector<int> full = P_edges;
    if (v != Q.vt) {
        std::vector<int> Cfinal = Ds.reconstruct(v, t);
        full.insert(full.end(), Cfinal.begin(), Cfinal.end());
    }
    if (full.empty()) return res;
    double kcfull = evaluateRouteKC(gs, poi, Q, full);
    Route route;
    route.edges = full;
    route.kc = kcfull;
    route.bs = 0.0;
    for (int eid : full) route.bs += gs.edges[eid].w;
    route.t_dep = gs.edges[full.front()].t_dep;
    route.t_arr = gs.edges[full.back()].t_arr;
    if (route.bs > Q.Delta + EPS || route.t_dep < Q.ta || route.t_arr > Q.tb)
        return res;
    res.found = true;
    res.route = route;
    res.rounds_used = rounds;
    return res;
}

// ---------------------------------------------------------------------------
// Small-instance exhaustive enumeration used for independent validation
// ---------------------------------------------------------------------------

inline ExactResult exhaustiveEnumeratorSmall(const Graph& g,
                                             const POIProfile& poi,
                                             const Query& Q,
                                             int64 time_limit_ms) {
    EventGraph H = buildEventGraph(g, Q.ta, Q.tb);
    const int n = g.n;
    const int q = Q.n_keywords();
    std::vector<Route> topK;
    auto betterRoute = [&](const Route& x, const Route& y) {
        if (std::fabs(x.kc - y.kc) > EPS) return x.kc > y.kc;
        return x.edges < y.edges;
    };
    auto insertTopK = [&](Route r) {
        for (auto& x : topK)
            if (x.edges == r.edges) return;
        topK.push_back(r);
        std::sort(topK.begin(), topK.end(), betterRoute);
        if ((int)topK.size() > Q.k) topK.pop_back();
    };
    auto nowMs = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    };
    int64 start = nowMs();
    bool timed_out = false;

    std::function<void(int, int, int, int, double,
                       std::vector<char>, std::vector<double>, double,
                       std::vector<int>)>
        dfs = [&](int node, int v, int arr, int enter, double b,
                  std::vector<char> vis, std::vector<double> cv, double kcv,
                  std::vector<int> es) {
        if (timed_out) return;
        if (v == Q.vt) {
            if (poi.s[v] > 0 && !vis[v] && Q.tb - enter >= Q.dwell) {
                vis[v] = 1;
                double add = 0.0;
                for (int i = 0; i < q; ++i) {
                    int k = Q.K[i];
                    if (poi.cov[v][k] > 0) {
                        add += Q.lambda[i] * poi.cov[v][k] * (1.0 - cv[i]);
                        cv[i] = 1.0 - (1.0 - cv[i]) * (1.0 - poi.cov[v][k]);
                    }
                }
                kcv += add;
            }
            Route r; r.edges = es; r.kc = kcv; r.bs = b;
            r.t_dep = es.empty() ? Q.ta : g.edges[es.front()].t_dep;
            r.t_arr = arr;
            insertTopK(r);
            return;
        }
        if (nowMs() - start > time_limit_ms) { timed_out = true; return; }
        for (const EArc& a : H.fwd[node]) {
            if (!a.trip) {
                // NOTE: pass by value, do NOT move: sibling travel arcs in
                // this same loop still read vis/cv/es below.
                dfs(a.to, v, H.time_of[a.to], enter, b, vis, cv, kcv, es);
                continue;
            }
            const Edge& e = g.edges[a.edge_id];
            if (std::find(es.begin(), es.end(), a.edge_id) != es.end()) continue;
            if (b + e.w > Q.Delta + EPS) continue;
            std::vector<char> vis2 = vis;
            std::vector<double> cv2 = cv;
            double kcv2 = kcv;
            if (poi.s[v] > 0 && !vis2[v] && e.t_dep - enter >= Q.dwell) {
                vis2[v] = 1;
                double add = 0.0;
                for (int i = 0; i < q; ++i) {
                    int k = Q.K[i];
                    if (poi.cov[v][k] > 0) {
                        add += Q.lambda[i] * poi.cov[v][k] * (1.0 - cv2[i]);
                        cv2[i] = 1.0 - (1.0 - cv2[i]) * (1.0 - poi.cov[v][k]);
                    }
                }
                kcv2 += add;
            }
            std::vector<int> es2 = es; es2.push_back(a.edge_id);
            dfs(a.to, e.v, e.t_arr, e.t_arr, b + e.w, std::move(vis2),
                std::move(cv2), kcv2, std::move(es2));
        }
    };

    int src = H.nodeId(Q.vs, H.idxAt(Q.vs, Q.ta));
    dfs(src, Q.vs, Q.ta, Q.ta, 0.0,
        std::vector<char>(n, 0), std::vector<double>(q, 0.0), 0.0,
        std::vector<int>{});
    ExactResult res;
    res.routes = topK;
    res.no_route = topK.empty();
    res.timed_out = timed_out;
    return res;
}

}  // namespace kcrtn
