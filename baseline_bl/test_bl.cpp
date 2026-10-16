#include <cassert>
#include <cmath>
#include <iostream>
#include <random>

#include "bl_time_expanded.hpp"

using namespace kcrtn;

namespace {

Edge trip(int u, int v, int dep, int arr, double cost) {
    Edge e;
    e.u = u;
    e.v = v;
    e.t_dep = dep;
    e.t_arr = arr;
    e.w = cost;
    return e;
}

Query query(int source, int target, double budget, int ta, int tb, int k = 1) {
    Query q;
    q.vs = source;
    q.vt = target;
    q.K = {0};
    q.lambda = {1.0};
    q.Delta = budget;
    q.ta = ta;
    q.tb = tb;
    q.k = k;
    return q;
}

void testImpossibleTransferIsRemoved() {
    Graph g;
    g.init(3);
    g.addEdge(trip(0, 1, 0, 20, 1));
    g.addEdge(trip(1, 2, 10, 15, 1));  // departed before arrival at station 1
    POIProfile poi;
    poi.cov.assign(3, std::vector<double>(1, 0.0));
    poi.s.assign(3, 0);

    BLResult r = blTimeExpandedTopK(g, poi, query(0, 2, 10, 0, 30), 1000);
    assert(!r.timed_out);
    assert(r.no_route);
    assert(r.routes.empty());
}

void testDwellScoringAndTopK() {
    Graph g;
    g.init(3);
    const int a = g.addEdge(trip(0, 1, 0, 10, 1));
    const int b = g.addEdge(trip(1, 2, 15, 20, 1));
    const int direct = g.addEdge(trip(0, 2, 1, 19, 1));
    (void)direct;
    POIProfile poi;
    poi.cov.assign(3, std::vector<double>(1, 0.0));
    poi.s.assign(3, 0);
    poi.cov[1][0] = 1.0;
    poi.s[1] = 5;

    Query q = query(0, 2, 10, 0, 25, 2);
    q.dwell_seconds = 5;
    BLResult r = blTimeExpandedTopK(g, poi, q, 1000);
    assert(!r.timed_out);
    assert(r.routes.size() == 2);
    assert(r.routes[0].edges == std::vector<int>({a, b}));
    assert(std::fabs(r.routes[0].kc - 1.0) < 1e-9);
    assert(std::fabs(r.routes[1].kc) < 1e-9);
    for (const Route& route : r.routes) {
        BLRouteCheck check = validateBLRoute(g, poi, q, route);
        assert(check.feasible);
    }
}

void testInsufficientDwellDoesNotScore() {
    Graph g;
    g.init(3);
    g.addEdge(trip(0, 1, 0, 10, 1));
    g.addEdge(trip(1, 2, 14, 20, 1));
    POIProfile poi;
    poi.cov.assign(3, std::vector<double>(1, 0.0));
    poi.s.assign(3, 0);
    poi.cov[1][0] = 1.0;
    poi.s[1] = 5;

    Query q = query(0, 2, 10, 0, 25);
    q.dwell_seconds = 5;
    BLResult r = blTimeExpandedTopK(g, poi, q, 1000);
    assert(r.routes.size() == 1);
    assert(std::fabs(r.routes[0].kc) < 1e-9);
}

void testBudgetAndTargetDwell() {
    Graph g;
    g.init(2);
    g.addEdge(trip(0, 1, 2, 10, 4));
    POIProfile poi;
    poi.cov.assign(2, std::vector<double>(1, 0.0));
    poi.s.assign(2, 0);
    poi.cov[1][0] = 1.0;
    poi.s[1] = 5;

    Query too_expensive = query(0, 1, 3, 0, 20);
    too_expensive.dwell_seconds = 5;
    BLResult no = blTimeExpandedTopK(g, poi, too_expensive, 1000);
    assert(no.no_route && no.routes.empty());

    Query feasible = query(0, 1, 4, 0, 20);
    feasible.dwell_seconds = 5;
    BLResult yes = blTimeExpandedTopK(g, poi, feasible, 1000);
    assert(yes.routes.size() == 1);
    assert(std::fabs(yes.routes[0].kc - 1.0) < 1e-9);
    assert(validateBLRoute(g, poi, feasible, yes.routes[0]).feasible);
}

void testRandomInstancesAgainstExhaustiveEnumeration() {
    std::mt19937 rng(20261007);
    for (int trial = 0; trial < 100; ++trial) {
        Graph g;
        g.init(5);
        for (int i = 0; i < 14; ++i) {
            int u = static_cast<int>(rng() % 5);
            int v = static_cast<int>(rng() % 5);
            if (u == v) v = (v + 1) % 5;
            int dep = static_cast<int>(rng() % 16);
            int arr = dep + static_cast<int>(rng() % 5);
            double cost = 1.0 + static_cast<double>(rng() % 4);
            g.addEdge(trip(u, v, dep, arr, cost));
        }
        POIProfile poi;
        poi.cov.assign(5, std::vector<double>(2, 0.0));
        poi.s.assign(5, 2);
        for (int v = 0; v < 5; ++v)
            poi.cov[v][rng() % 2] = 1.0;

        Query q;
        q.vs = 0;
        q.vt = 4;
        q.K = {0, 1};
        q.lambda = {0.6, 0.4};
        q.Delta = 8.0;
        q.ta = 0;
        q.tb = 20;
        q.k = 3;
        q.dwell_seconds = 2;

        BLResult bl = blTimeExpandedTopK(g, poi, q, 5000);
        ExactResult exhaustive = exhaustiveEnumeratorSmall(g, poi, q, 5000);
        assert(!bl.timed_out);
        assert(!exhaustive.timed_out);
        assert(bl.routes.size() == exhaustive.routes.size());
        for (std::size_t i = 0; i < bl.routes.size(); ++i) {
            assert(bl.routes[i].edges == exhaustive.routes[i].edges);
            assert(std::fabs(bl.routes[i].kc - exhaustive.routes[i].kc) < 1e-9);
            assert(validateBLRoute(g, poi, q, bl.routes[i]).feasible);
        }
    }
}

}  // namespace

int main() {
    testImpossibleTransferIsRemoved();
    testDwellScoringAndTopK();
    testInsufficientDwellDoesNotScore();
    testBudgetAndTargetDwell();
    testRandomInstancesAgainstExhaustiveEnumeration();
    std::cout << "All BL time-expanded tests passed.\n";
    return 0;
}
