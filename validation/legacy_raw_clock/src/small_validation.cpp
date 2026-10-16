#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "austin_loader.hpp"
#include "transit_router.hpp"

using namespace kcrtn;
namespace fs = std::filesystem;

namespace {

constexpr int MASTER_SEED = 20260928;
constexpr int INSTANCES_PER_CITY = 10;
constexpr int WINDOW_SECONDS = 90 * 60;
constexpr double BUDGET = 70.0;
constexpr int REQUESTED_K = 10;
constexpr int MAX_INSTANCE_EDGES = 600;
constexpr std::uint64_t MAX_ENUM_ROUTES = 500000;
constexpr long long ENUM_LIMIT_MS = 30000;
constexpr long long ASTAR_LIMIT_MS = 30000;

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct ScoredRoute {
    std::vector<int> original_edge_ids;
    double score = 0.0;
};

bool betterScoredRoute(const ScoredRoute& x, const ScoredRoute& y) {
    if (std::fabs(x.score - y.score) > EPS) return x.score > y.score;
    return x.original_edge_ids < y.original_edge_ids;
}

struct IndependentResult {
    std::uint64_t feasible_routes = 0;
    double best_score = -1.0;
    std::vector<ScoredRoute> top_routes;
    bool timed_out = false;
    long long runtime_ms = 0;
};

double independentScore(const Graph& g, const POIProfile& poi,
                        const Query& q, const std::vector<int>& route) {
    if (route.empty()) return 0.0;
    std::vector<double> coverage(q.K.size(), 0.0);
    std::vector<char> credited(g.n, 0);

    auto credit = [&](int v, int dwell) {
        if (v < 0 || v >= g.n || credited[v] || poi.s[v] <= 0 ||
            dwell < q.dwell) return;
        credited[v] = 1;
        for (std::size_t i = 0; i < q.K.size(); ++i) {
            const double cv = poi.cov[v][q.K[i]];
            coverage[i] = 1.0 - (1.0 - coverage[i]) * (1.0 - cv);
        }
    };

    credit(g.edges[route.front()].u,
           g.edges[route.front()].t_dep - q.ta);
    for (std::size_t i = 0; i < route.size(); ++i) {
        const Edge& e = g.edges[route[i]];
        const int dwell = (i + 1 < route.size())
                              ? g.edges[route[i + 1]].t_dep - e.t_arr
                              : q.tb - e.t_arr;
        credit(e.v, dwell);
    }

    double score = 0.0;
    for (std::size_t i = 0; i < q.K.size(); ++i)
        score += q.lambda[i] * coverage[i];
    return score;
}

IndependentResult enumerateIndependently(const Graph& g,
                                         const POIProfile& poi,
                                         const Query& q) {
    IndependentResult result;
    const long long started = nowMs();
    std::vector<char> used(g.edges.size(), 0);
    std::vector<int> route;

    std::function<void(int, int, double)> dfs =
        [&](int v, int current_time, double cost) {
            if (result.timed_out) return;
            if (nowMs() - started > ENUM_LIMIT_MS ||
                result.feasible_routes >= MAX_ENUM_ROUTES) {
                result.timed_out = true;
                return;
            }
            if (v == q.vt) {
                ++result.feasible_routes;
                const double score = independentScore(g, poi, q, route);
                result.best_score = std::max(result.best_score, score);
                ScoredRoute ranked;
                ranked.score = score;
                for (int eid : route)
                    ranked.original_edge_ids.push_back(
                        g.edges[eid].original_id);
                result.top_routes.push_back(std::move(ranked));
                std::sort(result.top_routes.begin(), result.top_routes.end(),
                          betterScoredRoute);
                if (result.top_routes.size() > static_cast<std::size_t>(q.k))
                    result.top_routes.pop_back();
                return;
            }
            for (int eid : g.out[v]) {
                if (used[eid]) continue;
                const Edge& e = g.edges[eid];
                if (e.t_dep < current_time || e.t_dep < q.ta ||
                    e.t_arr > q.tb || cost + e.w > q.Delta + EPS)
                    continue;
                used[eid] = 1;
                route.push_back(eid);
                dfs(e.v, e.t_arr, cost + e.w);
                route.pop_back();
                used[eid] = 0;
                if (result.timed_out) return;
            }
        };

    dfs(q.vs, q.ta, 0.0);
    result.runtime_ms = nowMs() - started;
    if (result.best_score < 0.0) result.best_score = 0.0;
    return result;
}

struct CandidateInstance {
    Graph graph;
    POIProfile poi;
    Query query;
    std::vector<int> original_nodes;
    int extraction_attempt = 0;
};

std::vector<int> keywordPool(const POIProfile& poi) {
    std::set<int> values;
    for (const auto& row : poi.cov)
        for (int k = 0; k < static_cast<int>(row.size()); ++k)
            if (row[k] > 0.0) values.insert(k);
    return std::vector<int>(values.begin(), values.end());
}

bool buildCandidate(const AustinData& data, std::mt19937& rng,
                    int attempt, CandidateInstance& out) {
    if (data.g.edges.empty()) return false;
    const int first_id = static_cast<int>(rng() % data.g.edges.size());
    const Edge* first = &data.g.edges[first_id];
    std::vector<int> path{first_id};
    std::vector<int> vertices{first->u, first->v};
    std::unordered_set<int> seen{first->u, first->v};
    int current = first->v;
    int current_time = first->t_arr;
    const int deadline = first->t_dep + WINDOW_SECONDS;
    const int target_hops = 2 + static_cast<int>(rng() % 3);

    while (static_cast<int>(path.size()) < target_hops) {
        std::vector<int> choices;
        for (int eid : data.g.out[current]) {
            const Edge& e = data.g.edges[eid];
            if (e.t_dep >= current_time && e.t_arr <= deadline &&
                !seen.count(e.v))
                choices.push_back(eid);
        }
        if (choices.empty()) break;
        int eid = choices[static_cast<int>(rng() % choices.size())];
        path.push_back(eid);
        current = data.g.edges[eid].v;
        current_time = data.g.edges[eid].t_arr;
        vertices.push_back(current);
        seen.insert(current);
    }
    if (path.size() < 2) return false;

    const int ta = std::max(0, data.g.edges[path.front()].t_dep - 300);
    const int tb = ta + WINDOW_SECONDS;
    if (data.g.edges[path.back()].t_arr > tb) return false;
    double path_cost = 0.0;
    for (int eid : path) path_cost += data.g.edges[eid].w;
    if (path_cost > BUDGET + EPS) return false;

    std::vector<int> extras;
    for (int v : vertices) {
        for (int eid : data.g.out[v]) {
            const Edge& e = data.g.edges[eid];
            if (e.t_dep >= ta && e.t_arr <= tb && !seen.count(e.v))
                extras.push_back(e.v);
        }
    }
    std::shuffle(extras.begin(), extras.end(), rng);
    for (int v : extras) {
        if (vertices.size() >= 6) break;
        if (seen.insert(v).second) vertices.push_back(v);
    }

    std::sort(vertices.begin(), vertices.end());
    std::vector<int> remap(data.g.n, -1);
    for (int i = 0; i < static_cast<int>(vertices.size()); ++i)
        remap[vertices[i]] = i;

    Graph sub;
    sub.init(static_cast<int>(vertices.size()));
    for (const Edge& e : data.g.edges) {
        if (e.t_dep < ta || e.t_arr > tb) continue;
        if (e.u < 0 || e.v < 0 || e.u >= data.g.n || e.v >= data.g.n)
            continue;
        if (remap[e.u] < 0 || remap[e.v] < 0) continue;
        Edge ne = e;
        ne.u = remap[e.u];
        ne.v = remap[e.v];
        sub.addEdge(ne);
    }
    if (sub.edges.empty() ||
        static_cast<int>(sub.edges.size()) > MAX_INSTANCE_EDGES)
        return false;
    for (auto& adjacency : sub.out) {
        std::sort(adjacency.begin(), adjacency.end(), [&](int a, int b) {
            if (sub.edges[a].t_dep != sub.edges[b].t_dep)
                return sub.edges[a].t_dep < sub.edges[b].t_dep;
            return a < b;
        });
    }

    POIProfile spoi;
    spoi.cov.assign(sub.n,
                    std::vector<double>(data.n_keywords, 0.0));
    spoi.s.assign(sub.n, 0);
    for (int i = 0; i < sub.n; ++i) {
        spoi.cov[i] = data.poi.cov[vertices[i]];
        bool has_keyword = false;
        for (double x : spoi.cov[i])
            if (x > 0.0) { has_keyword = true; break; }
        spoi.s[i] = has_keyword ? 1 : 0;
    }

    std::vector<int> pool = keywordPool(spoi);
    if (pool.size() < 3) return false;
    std::shuffle(pool.begin(), pool.end(), rng);
    const int keyword_count = std::min<int>(5, pool.size());

    Query q;
    q.vs = remap[data.g.edges[path.front()].u];
    q.vt = remap[data.g.edges[path.back()].v];
    q.ta = ta;
    q.tb = tb;
    q.Delta = BUDGET;
    q.k = REQUESTED_K;
    q.dwell = 600 + static_cast<int>(rng() % 1201);
    q.K.assign(pool.begin(), pool.begin() + keyword_count);
    q.lambda.resize(keyword_count);
    double weight_sum = 0.0;
    for (double& w : q.lambda) {
        w = 0.5 + static_cast<double>(rng() % 1001) / 1000.0;
        weight_sum += w;
    }
    for (double& w : q.lambda) w /= weight_sum;

    if (q.vs < 0 || q.vt < 0 || q.vs == q.vt) return false;
    out.graph = std::move(sub);
    out.poi = std::move(spoi);
    out.query = std::move(q);
    out.original_nodes = std::move(vertices);
    out.extraction_attempt = attempt;
    return true;
}

void writeInstance(const fs::path& dir, const CandidateInstance& inst) {
    fs::create_directories(dir);
    {
        std::ofstream f(dir / "edges.csv");
        f << "edge_id,original_edge_id,u,v,t_dep,t_arr,cost\n";
        f << std::fixed << std::setprecision(6);
        for (const Edge& e : inst.graph.edges)
            f << e.id << ',' << e.original_id << ',' << e.u << ',' << e.v
              << ',' << e.t_dep << ',' << e.t_arr << ',' << e.w << '\n';
    }
    {
        std::ofstream f(dir / "node_keywords.csv");
        f << "node_id,original_node_id,keywords\n";
        for (int v = 0; v < inst.graph.n; ++v) {
            f << v << ',' << inst.original_nodes[v] << ",\"";
            bool first = true;
            for (int k = 0; k < inst.poi.n_keywords(); ++k) {
                if (inst.poi.cov[v][k] <= 0.0) continue;
                if (!first) f << ';';
                f << k;
                first = false;
            }
            f << "\"\n";
        }
    }
    {
        std::ofstream f(dir / "query.csv");
        f << "source,target,t_alpha,t_beta,budget,k,dwell_seconds,keywords,weights\n";
        f << inst.query.vs << ',' << inst.query.vt << ',' << inst.query.ta
          << ',' << inst.query.tb << ',' << inst.query.Delta << ','
          << inst.query.k << ',' << inst.query.dwell << ",\"";
        for (std::size_t i = 0; i < inst.query.K.size(); ++i) {
            if (i) f << ';';
            f << inst.query.K[i];
        }
        f << "\",\"";
        f << std::fixed << std::setprecision(10);
        for (std::size_t i = 0; i < inst.query.lambda.size(); ++i) {
            if (i) f << ';';
            f << inst.query.lambda[i];
        }
        f << "\"\n";
    }
}

bool runAStar(const CandidateInstance& inst, double& best_score,
              std::vector<ScoredRoute>& top_routes, bool& timed_out,
              long long& runtime_ms) {
    const long long started = nowMs();
    Graph gtime = filterToGtime(inst.graph, inst.query.vs, inst.query.vt,
                                inst.query.ta, inst.query.tb);
    Profile2 profile = algorithm2BudgetProfile(
        gtime, inst.query.vt, inst.query.ta, inst.query.tb);
    if (profile.query(inst.query.vs, inst.query.ta) >
        inst.query.Delta + EPS) {
        runtime_ms = nowMs() - started;
        best_score = 0.0;
        top_routes.clear();
        timed_out = false;
        return true;
    }
    Graph gs = filterToGs(gtime, profile, inst.query.vs, inst.query.vt,
                          inst.query.Delta);
    auto masks = reachableKeywordMasks(gs, inst.poi,
                                       inst.poi.n_keywords());
    ExactResult result = exactTopK(gs, gtime, profile, inst.poi,
                                   inst.query, masks, ASTAR_LIMIT_MS);
    runtime_ms = nowMs() - started;
    timed_out = result.timed_out;
    best_score = result.routes.empty() ? 0.0 : result.routes.front().kc;
    top_routes.clear();
    for (const Route& route : result.routes) {
        ScoredRoute ranked;
        ranked.score = route.kc;
        for (int eid : route.edges)
            ranked.original_edge_ids.push_back(gs.edges[eid].original_id);
        top_routes.push_back(std::move(ranked));
    }
    std::sort(top_routes.begin(), top_routes.end(), betterScoredRoute);
    return !timed_out;
}

void writeTopKComparison(const fs::path& instance_dir,
                         const std::vector<ScoredRoute>& astar,
                         const std::vector<ScoredRoute>& exhaustive) {
    std::ofstream f(instance_dir / "topk_comparison.csv");
    f << "rank,A_star_score,exhaustive_score,score_match,"
         "A_star_original_edge_ids,exhaustive_original_edge_ids,route_match\n";
    const std::size_t n = std::max(astar.size(), exhaustive.size());
    for (std::size_t i = 0; i < n; ++i) {
        const bool have_a = i < astar.size();
        const bool have_e = i < exhaustive.size();
        const bool score_match = have_a && have_e &&
            std::fabs(astar[i].score - exhaustive[i].score) <= 1e-8;
        const bool route_match = have_a && have_e &&
            astar[i].original_edge_ids == exhaustive[i].original_edge_ids;
        f << (i + 1) << ',' << std::fixed << std::setprecision(10);
        if (have_a) f << astar[i].score;
        f << ',';
        if (have_e) f << exhaustive[i].score;
        f << ',' << (score_match ? 1 : 0) << ",\"";
        if (have_a) {
            for (std::size_t j = 0; j < astar[i].original_edge_ids.size(); ++j) {
                if (j) f << ';';
                f << astar[i].original_edge_ids[j];
            }
        }
        f << "\",\"";
        if (have_e) {
            for (std::size_t j = 0;
                 j < exhaustive[i].original_edge_ids.size(); ++j) {
                if (j) f << ';';
                f << exhaustive[i].original_edge_ids[j];
            }
        }
        f << "\"," << (route_match ? 1 : 0) << '\n';
    }
}

bool runCity(const std::string& city, const std::string& edge_path,
             const std::string& label_path, int city_offset,
             const fs::path& output_root, std::ofstream& metadata,
             std::ofstream& results) {
    AustinData data = loadAustin(edge_path, label_path, 1, 1,
                                 MASTER_SEED + city_offset);
    if (data.g.n == 0 || data.g.edges.empty() || data.n_keywords == 0) {
        std::cerr << "Failed to load " << city << "\n";
        return false;
    }
    std::cerr << city << ": loaded " << data.g.n << " vertices and "
              << data.g.edges.size() << " scheduled edges\n";

    std::mt19937 rng(MASTER_SEED + city_offset);
    int accepted = 0;
    for (int attempt = 1; attempt <= 200000 && accepted < INSTANCES_PER_CITY;
         ++attempt) {
        CandidateInstance inst;
        if (!buildCandidate(data, rng, attempt, inst)) continue;

        IndependentResult exhaustive =
            enumerateIndependently(inst.graph, inst.poi, inst.query);
        if (exhaustive.timed_out || exhaustive.feasible_routes == 0) continue;

        double astar_score = 0.0;
        std::vector<ScoredRoute> astar_top_routes;
        bool astar_timeout = false;
        long long astar_ms = 0;
        if (!runAStar(inst, astar_score, astar_top_routes, astar_timeout,
                      astar_ms))
            continue;

        ++accepted;
        const std::string instance_id =
            city + "_" + (accepted < 10 ? "0" : "") +
            std::to_string(accepted);
        const fs::path instance_dir = output_root / "small_instances" /
                                      city / instance_id;
        writeInstance(instance_dir, inst);
        writeTopKComparison(instance_dir, astar_top_routes,
                            exhaustive.top_routes);

        const bool score_match =
            std::fabs(astar_score - exhaustive.best_score) <= 1e-8;
        bool topk_score_match =
            astar_top_routes.size() == exhaustive.top_routes.size();
        bool topk_route_match = topk_score_match;
        if (topk_score_match) {
            for (std::size_t i = 0; i < astar_top_routes.size(); ++i) {
                if (std::fabs(astar_top_routes[i].score -
                              exhaustive.top_routes[i].score) > 1e-8)
                    topk_score_match = false;
                if (astar_top_routes[i].original_edge_ids !=
                    exhaustive.top_routes[i].original_edge_ids)
                    topk_route_match = false;
            }
        }
        metadata << instance_id << ',' << city << ','
                 << (MASTER_SEED + city_offset) << ','
                 << inst.extraction_attempt << ',' << inst.graph.n << ','
                 << inst.graph.edges.size() << ',' << inst.query.vs << ','
                 << inst.query.vt << ',' << inst.query.ta << ','
                 << inst.query.tb << ',' << inst.query.Delta << ','
                 << inst.query.k << ',' << inst.query.dwell << ','
                 << exhaustive.feasible_routes << '\n';

        results << instance_id << ',' << city << ',' << std::fixed
                << std::setprecision(10) << astar_score << ','
                << exhaustive.best_score << ',' << (score_match ? 1 : 0)
                << ',' << (topk_score_match ? 1 : 0) << ','
                << (topk_route_match ? 1 : 0) << ','
                << astar_top_routes.size() << ','
                << exhaustive.top_routes.size() << ','
                << exhaustive.feasible_routes << ',' << astar_ms << ','
                << exhaustive.runtime_ms << ',' << (astar_timeout ? 1 : 0)
                << ',' << (exhaustive.timed_out ? 1 : 0) << '\n';

        std::cerr << city << " " << accepted << "/"
                  << INSTANCES_PER_CITY << ": V=" << inst.graph.n
                  << " E=" << inst.graph.edges.size()
                  << " feasible=" << exhaustive.feasible_routes
                  << " A*=" << astar_score
                  << " ENUM=" << exhaustive.best_score
                  << " best_match=" << score_match
                  << " topk_scores=" << topk_score_match
                  << " topk_routes=" << topk_route_match << "\n";
    }
    return accepted == INSTANCES_PER_CITY;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "Usage: small_validation AUSTIN_EDGES AUSTIN_LABELS "
                     "HOUSTON_EDGES HOUSTON_LABELS OUTPUT_DIR\n";
        return 2;
    }
    fs::path output_root = argv[5];
    fs::create_directories(output_root);

    std::ofstream metadata(output_root / "small_instance_metadata.csv");
    metadata << "instance_id,city,random_seed,extraction_attempt,n_vertices,"
                "n_scheduled_edges,source,target,t_alpha,t_beta,budget,k,"
                "dwell_seconds,feasible_route_count\n";
    std::ofstream results(output_root / "exhaustive_validation.csv");
    results << "instance_id,city,A_star_best_score,exhaustive_best_score,"
               "best_score_match,full_top_k_score_match,"
               "full_top_k_route_match,A_star_returned_routes,"
               "exhaustive_expected_top_k_size,"
               "exhaustive_feasible_route_count,A_star_runtime_ms,"
               "enumeration_runtime_ms,A_star_timeout,enumeration_timeout\n";

    const bool austin_ok = runCity("austin", argv[1], argv[2], 1000,
                                   output_root, metadata, results);
    const bool houston_ok = runCity("houston", argv[3], argv[4], 2000,
                                    output_root, metadata, results);
    return (austin_ok && houston_ok) ? 0 : 1;
}
