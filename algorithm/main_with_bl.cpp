// ============================================================================
// main_with_bl.cpp — reproducible A*/AP/BL experiment driver.
//
// Loads the authors' Austin dataset (edges + labels), builds the POI profile
// with cov=1 membership and dwell times s_v ~ U[900,1800] s, generates random
// queries following Section 5.1.3 (|K| in {3..7} default 5, Delta in
// {50..90} default 70, window in {120..240} min default 180, k in {1..20}
// default 10), and records, per query and algorithm:
//   * runtime, timeout, feasible, distinct-route count, KC, exact KC, ratio
//   * the FULL returned routes (edge ids, KC, budget, departure/arrival)
// into CSV files (results.csv / routes.csv), so that the reviewers' request
// for per-query outputs and route-validation logs is satisfied.
//
// BL converts the temporal graph to a static directed graph with arrival-time
// copies and then runs a static exact top-k route search.  The separate
// exhaustive enumerator is used only by --validate on small instances.
//
// Usage:
//   ./kcrn_bl --edges Austinedges --labels label --queries 30 --out results
//   ./kcrn_bl --edges ... --labels ... --validate --subgraph 100 --queries 30
// ============================================================================

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "austin_loader.hpp"
#include "bl_time_expanded.hpp"
#include "transit_router.hpp"

using namespace kcrtn;

namespace {

struct Options {
    std::string edges = "Austinedges";
    std::string labels = "label";
    int queries = 30;
    std::string sweeps = "K,Delta,Window,k";
    int rho = 3, L = 20;
    int64 timeout_ms = 300000;
    int seed = 20260928;
    int s_min = 900, s_max = 1800;
    double delta_def = 70.0;
    int win_def = 180 * 60;
    int k_def = 10;
    int kw_def = 5;
    std::string out = "results";
    bool validate = false;
    bool skip_default = false;
    std::string only_param;
    int subgraph = 100;
};

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](std::string& out) { if (i + 1 < argc) out = argv[++i]; };
        if (a == "--edges") need(o.edges);
        else if (a == "--labels") need(o.labels);
        else if (a == "--queries") { std::string v; need(v); o.queries = std::stoi(v); }
        else if (a == "--sweeps") need(o.sweeps);
        else if (a == "--rho") { std::string v; need(v); o.rho = std::stoi(v); }
        else if (a == "--L") { std::string v; need(v); o.L = std::stoi(v); }
        else if (a == "--timeout-ms") { std::string v; need(v); o.timeout_ms = std::stoll(v); }
        else if (a == "--seed") { std::string v; need(v); o.seed = std::stoi(v); }
        else if (a == "--s-min") { std::string v; need(v); o.s_min = std::stoi(v); }
        else if (a == "--s-max") { std::string v; need(v); o.s_max = std::stoi(v); }
        else if (a == "--delta") { std::string v; need(v); o.delta_def = std::stod(v); }
        else if (a == "--window-min") { std::string v; need(v); o.win_def = std::stoi(v) * 60; }
        else if (a == "--k") { std::string v; need(v); o.k_def = std::stoi(v); }
        else if (a == "--kw") { std::string v; need(v); o.kw_def = std::stoi(v); }
        else if (a == "--out") need(o.out);
        else if (a == "--validate") o.validate = true;
        else if (a == "--skip-default") o.skip_default = true;
        else if (a == "--only-param") { o.only_param = argv[++i]; }
        else if (a == "--subgraph") { std::string v; need(v); o.subgraph = std::stoi(v); }
        else if (a == "--help") {
            printf(
                "kcrn_bl --edges FILE --labels FILE [options]\n"
                "  --queries N        queries per configuration (default 30)\n"
                "  --sweeps LIST      K,Delta,Window,k (default all)\n"
                "  --rho --L          AP parameters (default rho=3 L=20)\n"
                "  --timeout-ms       per-algorithm/query limit (default 300000)\n"
                "  --s-min --s-max    dwell seconds (default 900 1800)\n"
                "  --delta --window-min --k --kw   default query values\n"
                "  --validate         run A*, transformed BL, and exhaustive\n"
                "                     enumeration on a small subgraph\n"
                "  --subgraph N       subgraph size for validation\n"
                "  --out PREFIX       results PREFIX.csv / PREFIX.routes.csv\n");
            std::exit(0);
        }
    }
    return o;
}

int64 nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct QueryGen {
    const Graph& g;
    const POIProfile& poi;
    int nkw_total;
    int t_min, t_max;  // data time range for window placement
    std::mt19937 rng;

    QueryGen(const Graph& g_, const POIProfile& poi_, int nkw_total_,
             int seed)
        : g(g_), poi(poi_), nkw_total(nkw_total_), rng(seed) {
        t_min = std::numeric_limits<int>::max();
        t_max = 0;
        for (const Edge& e : g.edges) {
            t_min = std::min(t_min, e.t_dep);
            t_max = std::max(t_max, e.t_arr);
        }
    }

    Query make(int kw_sel, double Delta, int win_s, int k) {
        Query q;
        // source: uniform among vertices that have at least one scheduled trip
        do { q.vs = (int)(rng() % (uint32_t)g.n); } while (g.out[q.vs].empty());
        // window start: aligned to a random departure time of vs so that the
        // query is meaningful (the traveler can actually board); still random
        // in that the specific trip and window are drawn at random.
        int lo_t = t_min, hi_t = std::max(lo_t, t_max - win_s);
        std::vector<int> dep_times;
        for (int eid : g.out[q.vs]) {
            int d = g.edges[eid].t_dep;
            if (d >= lo_t && d <= hi_t) dep_times.push_back(d);
        }
        if (dep_times.empty())
            dep_times.push_back(lo_t + (int)(rng() % (uint32_t)(hi_t - lo_t + 1)));
        q.ta = dep_times[(int)(rng() % (uint32_t)dep_times.size())];
        q.tb = q.ta + win_s;
        // target: uniform over all vertices
        do { q.vt = (int)(rng() % (uint32_t)g.n); } while (q.vt == q.vs);
        std::vector<int> pool(nkw_total);
        std::iota(pool.begin(), pool.end(), 0);
        std::shuffle(pool.begin(), pool.end(), rng);
        q.K.assign(pool.begin(), pool.begin() + kw_sel);
        std::vector<double> w(kw_sel);
        double sum = 0.0;
        for (auto& x : w) { x = 0.5 + (double)(rng() % 1000) / 1000.0; sum += x; }
        for (auto& x : w) x /= sum;
        q.lambda = w;
        q.Delta = Delta;
        q.k = k;
        return q;
    }
};

struct MethodOutcome {
    bool feasible = false;
    bool timed_out = false;
    bool all_routes_valid = true;
    int distinct = 0;
    double best_kc = 0.0;
    int64 runtime_ms = 0;
    long long expanded = 0;
    std::size_t static_nodes = 0;
    std::size_t static_arcs = 0;
    std::vector<Route> routes;
};

struct QueryOutcome {
    MethodOutcome bl, exact, ap;
    bool infeasible = false;
    bool exact_available = false;
    double exact_kc = 0.0;
};

// Routes produced on a filtered graph contain local edge ids.  Convert them
// back to stable ids from the unfiltered input before logging or validation.
std::vector<Route> projectRoutesToInput(const Graph& local,
                                        std::vector<Route> routes) {
    for (Route& route : routes) {
        for (int& edge_id : route.edges) {
            if (edge_id >= 0 && edge_id < static_cast<int>(local.edges.size()))
                edge_id = local.edges[edge_id].original_id;
            else
                edge_id = -1;
        }
    }
    return routes;
}

void finalizeOutcome(MethodOutcome& out, const Graph& original,
                     const POIProfile& poi, const Query& q) {
    out.distinct = static_cast<int>(out.routes.size());
    out.feasible = !out.routes.empty();
    out.best_kc = out.routes.empty() ? 0.0 : out.routes.front().kc;
    out.all_routes_valid = true;
    for (const Route& route : out.routes) {
        if (!validateBLRoute(original, poi, q, route).feasible) {
            out.all_routes_valid = false;
            break;
        }
    }
}

QueryOutcome runQueryAll(const Options& o, const Graph& g,
                         const POIProfile& poi, const Query& Q) {
    QueryOutcome oc;

    // Query-selection probe only.  Each timed method below rebuilds all of its
    // own preprocessing, so reported runtimes include the complete pipeline.
    {
        Graph probe = filterToGtime(g, Q.vs, Q.vt, Q.ta, Q.tb);
        Profile2 d = algorithm2BudgetProfile(probe, Q.vt, Q.ta, Q.tb);
        if (d.query(Q.vs, Q.ta) > Q.Delta + EPS) {
            oc.infeasible = true;
            return oc;
        }
    }

    // BL: conversion + static shortest-cost profile + static keyword masks +
    // static best-first search.  No A*/AP preprocessing is shared.
    {
        const int64 t0 = nowMs();
        BLResult bl = blTimeExpandedTopK(g, poi, Q, o.timeout_ms);
        oc.bl.runtime_ms = nowMs() - t0;
        oc.bl.timed_out = bl.timed_out || oc.bl.runtime_ms > o.timeout_ms;
        oc.bl.expanded = bl.expanded_labels;
        oc.bl.static_nodes = bl.static_nodes;
        oc.bl.static_arcs = bl.static_arcs;
        oc.bl.routes = std::move(bl.routes);  // already uses input edge ids
        finalizeOutcome(oc.bl, g, poi, Q);
    }

    // A*: Algorithms 1--3, including all per-query filtering/profile/mask
    // construction in the timed region.
    {
        const int64 t0 = nowMs();
        Graph gtime = filterToGtime(g, Q.vs, Q.vt, Q.ta, Q.tb);
        Profile2 dtime = algorithm2BudgetProfile(gtime, Q.vt, Q.ta, Q.tb);
        Graph gs = filterToGs(gtime, dtime, Q.vs, Q.vt, Q.Delta);
        auto masks = reachableKeywordMasks(gs, poi, poi.n_keywords());
        const int64 preprocessing_ms = nowMs() - t0;
        ExactResult exact_result;
        if (preprocessing_ms > o.timeout_ms) {
            exact_result.timed_out = true;
        } else {
            exact_result = exactTopK(gs, gtime, dtime, poi, Q, masks,
                                     o.timeout_ms - preprocessing_ms);
        }
        oc.exact.runtime_ms = nowMs() - t0;
        oc.exact.timed_out = exact_result.timed_out ||
                             oc.exact.runtime_ms > o.timeout_ms;
        oc.exact.expanded = exact_result.expanded;
        oc.exact.routes = projectRoutesToInput(gs, std::move(exact_result.routes));
        finalizeOutcome(oc.exact, g, poi, Q);
        oc.exact_available = !oc.exact.timed_out && oc.exact.feasible;
        oc.exact_kc = oc.exact_available ? oc.exact.best_kc : 0.0;
    }

    // AP: Algorithms 1--2, final filtering, AP's G_s profile, departure-list
    // sorting and all J runs are independently timed.
    {
        const int64 t0 = nowMs();
        Graph gtime = filterToGtime(g, Q.vs, Q.vt, Q.ta, Q.tb);
        Profile2 dtime = algorithm2BudgetProfile(gtime, Q.vt, Q.ta, Q.tb);
        Graph gs = filterToGs(gtime, dtime, Q.vs, Q.vt, Q.Delta);
        std::vector<std::vector<int>> dep_sorted(g.n);
        for (int v = 0; v < g.n; ++v) {
            dep_sorted[v] = gs.out[v];
            std::sort(dep_sorted[v].begin(), dep_sorted[v].end(),
                      [&](int a, int b) {
                          if (gs.edges[a].t_dep != gs.edges[b].t_dep)
                              return gs.edges[a].t_dep < gs.edges[b].t_dep;
                          return a < b;
                      });
        }
        Profile2 ds = algorithm2BudgetProfile(gs, Q.vt, Q.ta, Q.tb);
        std::mt19937 arng(o.seed + Q.vs * 1000003 + Q.vt * 7919);
        std::vector<Route> found_routes;
        for (int j = 0; j < std::max(1, Q.k); ++j) {
            if (nowMs() - t0 > o.timeout_ms) break;
            APResult ap = apSearch(gs, poi, Q, ds, dep_sorted, o.rho, o.L,
                                   j, arng);
            if (!ap.found) continue;
            bool duplicate = false;
            for (const Route& r : found_routes)
                if (r.edges == ap.route.edges) { duplicate = true; break; }
            if (!duplicate) found_routes.push_back(std::move(ap.route));
        }
        std::sort(found_routes.begin(), found_routes.end(),
                  [](const Route& a, const Route& b) {
                      if (std::fabs(a.kc - b.kc) > EPS) return a.kc > b.kc;
                      return a.edges < b.edges;
                  });
        if (static_cast<int>(found_routes.size()) > Q.k)
            found_routes.resize(Q.k);
        oc.ap.runtime_ms = nowMs() - t0;
        oc.ap.timed_out = oc.ap.runtime_ms > o.timeout_ms;
        oc.ap.routes = projectRoutesToInput(gs, std::move(found_routes));
        finalizeOutcome(oc.ap, g, poi, Q);
    }
    return oc;
}

void writeRoutes(FILE* f, int query_id, const std::string& sweep,
                 const std::string& pval, const char* algo,
                 const std::vector<Route>& routes) {
    for (int i = 0; i < (int)routes.size(); ++i) {
        const Route& r = routes[i];
        std::string ids;
        for (size_t j = 0; j < r.edges.size(); ++j) {
            if (j) ids += ";";
            ids += std::to_string(r.edges[j]);
        }
        fprintf(f, "%d,%s,%s,%s,%d,%.8f,%.6f,%d,%d,%d,%s\n",
                query_id, sweep.c_str(), pval.c_str(), algo, i,
                r.kc, r.bs, (int)r.edges.size(), r.t_dep, r.t_arr, ids.c_str());
    }
}

std::string joinInts(const std::vector<int>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out += ";";
        out += std::to_string(values[i]);
    }
    return out;
}

std::string joinDoubles(const std::vector<double>& values) {
    std::string out;
    char buffer[64];
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out += ";";
        std::snprintf(buffer, sizeof(buffer), "%.12f", values[i]);
        out += buffer;
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    Options o = parseArgs(argc, argv);

    AustinData data = loadAustin(o.edges, o.labels, o.s_min, o.s_max, o.seed);
    if (data.g.n == 0 || data.n_keywords == 0) {
        fprintf(stderr, "error: failed to load dataset\n");
        return 1;
    }
    fprintf(stderr, "dataset loaded: %d nodes, %d trips, %d keywords (%s)\n",
            data.g.n, (int)data.g.edges.size(), data.n_keywords, o.edges.c_str());

    // -------- validation: A*, transformed BL, exhaustive enumeration ------
    if (o.validate) {
        int size = std::min(o.subgraph, data.g.n);
        std::mt19937 rng(o.seed + 777);
        std::vector<int> nodes(data.g.n);
        std::iota(nodes.begin(), nodes.end(), 0);
        std::shuffle(nodes.begin(), nodes.end(), rng);
        std::vector<char> keep(data.g.n, 0);
        for (int i = 0; i < size; ++i) keep[nodes[i]] = 1;
        std::vector<int> remap(data.g.n, -1);
        int m = 0;
        for (int v = 0; v < data.g.n; ++v)
            if (keep[v]) remap[v] = m++;
        Graph sub;
        sub.init(m);
        for (const Edge& e : data.g.edges)
            if (keep[e.u] && keep[e.v]) {
                Edge ne = e; ne.u = remap[e.u]; ne.v = remap[e.v];
                sub.addEdge(ne);
            }
        POIProfile spoi;
        spoi.cov.assign(m, std::vector<double>(data.n_keywords, 0.0));
        spoi.s.assign(m, 0);
        for (int v = 0; v < data.g.n; ++v)
            if (keep[v]) {
                spoi.s[remap[v]] = data.poi.s[v];
                spoi.cov[remap[v]] = data.poi.cov[v];
            }
        fprintf(stderr, "validation subgraph: %d nodes, %d trips\n", m,
                (int)sub.edges.size());

        QueryGen gen(sub, spoi, data.n_keywords, o.seed + 42);
        std::string path = o.out + ".validation.csv";
        FILE* f = fopen(path.c_str(), "w");
        fprintf(f, "query_id,vs,vt,feasible,A_star_kc,BL_kc,exhaustive_kc,"
                   "A_star_ms,BL_ms,exhaustive_ms,A_matches,BL_matches\n");
        int a_matched = 0, bl_matched = 0, feasible = 0;
        int attempted = 0, accepted = 0;
        while (accepted < o.queries && attempted < 20000) {
            Query Q = gen.make(o.kw_def, o.delta_def, o.win_def, 1);
            ++attempted;
            Graph gtime = filterToGtime(sub, Q.vs, Q.vt, Q.ta, Q.tb);
            Profile2 Dgtime = algorithm2BudgetProfile(gtime, Q.vt, Q.ta, Q.tb);
            bool infeas = Dgtime.query(Q.vs, Q.ta) > Q.Delta + EPS;
            if (infeas) continue;
            ++accepted;
            Graph gs = filterToGs(gtime, Dgtime, Q.vs, Q.vt, Q.Delta);
            auto masks = reachableKeywordMasks(gs, spoi, data.n_keywords);
            int64 t0 = nowMs();
            ExactResult ex = exactTopK(gs, gtime, Dgtime, spoi, Q, masks, o.timeout_ms);
            int64 t1 = nowMs();
            int64 t2 = nowMs();
            BLResult bl = blTimeExpandedTopK(sub, spoi, Q, o.timeout_ms);
            int64 t3 = nowMs();
            int64 t4 = nowMs();
            ExactResult enumeration =
                exhaustiveEnumeratorSmall(gs, spoi, Q, o.timeout_ms);
            int64 t5 = nowMs();
            double a_kc = ex.routes.empty() ? 0.0 : ex.routes.front().kc;
            double b_kc = bl.routes.empty() ? 0.0 : bl.routes.front().kc;
            double e_kc = enumeration.routes.empty()
                              ? 0.0
                              : enumeration.routes.front().kc;
            bool has_route = !enumeration.routes.empty();
            bool a_match = ex.timed_out
                               ? false
                               : (ex.routes.empty() == enumeration.routes.empty() &&
                                  std::fabs(a_kc - e_kc) < 1e-6);
            bool bl_match = bl.timed_out
                                ? false
                                : (bl.routes.empty() == enumeration.routes.empty() &&
                                   std::fabs(b_kc - e_kc) < 1e-6);
            if (has_route) ++feasible;
            if (a_match) ++a_matched;
            if (bl_match) ++bl_matched;
            fprintf(f, "%d,%d,%d,%d,%.8f,%.8f,%.8f,%lld,%lld,%lld,%d,%d\n",
                    accepted, Q.vs, Q.vt, has_route ? 1 : 0, a_kc, b_kc,
                    e_kc, (long long)(t1 - t0), (long long)(t3 - t2),
                    (long long)(t5 - t4), a_match ? 1 : 0,
                    bl_match ? 1 : 0);
            if (accepted % 5 == 0)
                fprintf(stderr, "  validation %d/%d (attempted %d)\n",
                        accepted, o.queries, attempted);
        }
        fclose(f);
        fprintf(stderr,
                "validation done: %d/%d feasible, A* %d/%d match, "
                "BL %d/%d match -> %s\n",
                feasible, o.queries, a_matched, accepted, bl_matched,
                accepted, path.c_str());
        return 0;
    }

    // ---------------- main experiment: full graph BL + A* + AP -----------
    std::string res_path = o.out + ".csv";
    std::string rt_path = o.out + ".routes.csv";
    std::string q_path = o.out + ".queries.csv";
    FILE* fr = fopen(res_path.c_str(), "w");
    FILE* ft = fopen(rt_path.c_str(), "w");
    FILE* fq = fopen(q_path.c_str(), "w");
    if (!fr || !ft || !fq) {
        fprintf(stderr, "error: cannot create output files for prefix %s\n",
                o.out.c_str());
        if (fr) fclose(fr);
        if (ft) fclose(ft);
        if (fq) fclose(fq);
        return 2;
    }
    fprintf(fr, "sweep,param_value,algo,query_id,runtime_ms,timeout,feasible,"
                "distinct_routes,best_kc,exact_available,exact_kc,ratio,"
                "all_routes_valid,expanded_labels,static_nodes,static_arcs,"
                "vs,vt,ta,tb,Delta,k,attempted\n");
    fprintf(ft, "query_id,sweep,param_value,algo,route_idx,kc,budget,"
                "n_edges,t_dep,t_arr,edge_ids\n");
    fprintf(fq, "sweep,param_value,query_id,vs,vt,ta,tb,Delta,k,keywords,"
                "lambdas,master_seed,dwell_min_s,dwell_max_s,attempted\n");

    auto runConfig = [&](const std::string& sweep, const std::string& pval,
                         int kw_sel, double delta, int win_s, int k, int base) {
        QueryGen gen(data.g, data.poi, data.n_keywords, o.seed + base);
        int attempted = 0, accepted = 0;
        while (accepted < o.queries && attempted < 10000) {
            Query Q = gen.make(kw_sel, delta, win_s, k);
            ++attempted;
            QueryOutcome oc = runQueryAll(o, data.g, data.poi, Q);
            if (oc.infeasible) continue;   // reject, count only in attempted
            ++accepted;
            double exact_kc = oc.exact_kc;
            fprintf(fq, "%s,%s,%d,%d,%d,%d,%d,%.8f,%d,%s,%s,%d,%d,%d,%d\n",
                    sweep.c_str(), pval.c_str(), accepted, Q.vs, Q.vt,
                    Q.ta, Q.tb, Q.Delta, Q.k, joinInts(Q.K).c_str(),
                    joinDoubles(Q.lambda).c_str(), o.seed, o.s_min, o.s_max,
                    attempted);

            auto writeMethod = [&](const char* name,
                                   const MethodOutcome& method) {
                std::string ratio;
                if (oc.exact_available && exact_kc > EPS) {
                    char buffer[64];
                    std::snprintf(buffer, sizeof(buffer), "%.8f",
                                  method.best_kc / exact_kc);
                    ratio = buffer;
                }
                fprintf(fr,
                        "%s,%s,%s,%d,%lld,%d,%d,%d,%.8f,%d,%.8f,%s,%d,"
                        "%lld,%zu,%zu,%d,%d,%d,%d,%.8f,%d,%d\n",
                        sweep.c_str(), pval.c_str(), name, accepted,
                        (long long)method.runtime_ms,
                        method.timed_out ? 1 : 0,
                        method.feasible ? 1 : 0, method.distinct,
                        method.best_kc, oc.exact_available ? 1 : 0, exact_kc,
                        ratio.c_str(), method.all_routes_valid ? 1 : 0,
                        method.expanded, method.static_nodes,
                        method.static_arcs, Q.vs, Q.vt, Q.ta, Q.tb, Q.Delta,
                        Q.k, attempted);
                writeRoutes(ft, accepted, sweep, pval, name, method.routes);
            };

            // Keep the same algorithm order used in the paper figures.
            writeMethod("BL", oc.bl);
            writeMethod("A*", oc.exact);
            writeMethod("AP", oc.ap);
            // flush after every accepted query so that an interrupted run
            // (VM recycle, OOM kill) never loses completed queries.
            fflush(fr);
            fflush(ft);
            fflush(fq);
            if (accepted % 5 == 0)
                fprintf(stderr, "  [%s=%s] %d/%d accepted (attempted %d)\n",
                        sweep.c_str(), pval.c_str(), accepted, o.queries, attempted);
        }
        fprintf(stderr, "  [%s=%s] feasible rate: %d/%d (%.2f%%)\n",
                sweep.c_str(), pval.c_str(), accepted, attempted,
                100.0 * accepted / attempted);
    };

    // default configuration
    if (!o.skip_default) {
        fprintf(stderr, "== default config (|K|=%d, Delta=%.0f, W=%dmin, k=%d) ==\n",
                o.kw_def, o.delta_def, o.win_def / 60, o.k_def);
        runConfig("Default", "-", o.kw_def, o.delta_def, o.win_def, o.k_def, 1);
    }

    // sweeps
    struct SW { const char* name; std::vector<int> vals; };
    std::vector<SW> all;
    if (o.sweeps.find("K") != std::string::npos)
        all.push_back({"K", {3, 4, 5, 6, 7}});
    if (o.sweeps.find("Delta") != std::string::npos)
        all.push_back({"Delta", {50, 60, 70, 80, 90}});
    if (o.sweeps.find("Window") != std::string::npos)
        all.push_back({"Window", {120, 150, 180, 210, 240}});
    if (o.sweeps.find("k") != std::string::npos)
        all.push_back({"k", {1, 5, 10, 15, 20}});
    for (auto& sw : all) {
        for (int v : sw.vals) {
            if (!o.only_param.empty() &&
                std::to_string(v) != o.only_param) continue;
            int kw_sel = o.kw_def, k = o.k_def;
            double delta = o.delta_def;
            int win_s = o.win_def;
            if (!strcmp(sw.name, "K")) kw_sel = v;
            else if (!strcmp(sw.name, "Delta")) delta = v;
            else if (!strcmp(sw.name, "Window")) win_s = v * 60;
            else k = v;
            fprintf(stderr, "== sweep %s=%d ==\n", sw.name, v);
            runConfig(sw.name, std::to_string(v), kw_sel, delta, win_s, k,
                      100 + v);
        }
    }
    fclose(fr);
    fclose(ft);
    fclose(fq);
    fprintf(stderr, "results: %s, routes: %s, queries: %s\n",
            res_path.c_str(), rt_path.c_str(), q_path.c_str());
    return 0;
}
