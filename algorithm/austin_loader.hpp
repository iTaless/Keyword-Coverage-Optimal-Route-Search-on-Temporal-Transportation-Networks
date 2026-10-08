// ============================================================================
// austin_loader.hpp
// ----------------------------------------------------------------------------
// Loader for the authors' Austin dataset format:
//   edges file: "u v dep arr w" per line, whitespace-separated,
//               times in seconds, w a double fare.
//   label file: "node kw1 kw2 ..." per line (keyword membership, 0/1).
//
// POIProfile construction follows the authors' choices:
//   * cov(v,k) = 1.0 if keyword k appears in node v's label list, else 0
//   * dwell time s_v = uniform random integer in [900, 1800] seconds
//     (15-30 minutes) for every node that has at least one keyword;
//     a fixed master seed makes the assignment reproducible.
// ============================================================================

#pragma once

#include <algorithm>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "transit_router.hpp"

namespace kcrtn {

struct AustinData {
    Graph g;
    POIProfile poi;
    int n_keywords = 0;
};

// edge_path: "u v dep arr w" lines
// label_path: "node kw..." lines
// s_min_s, s_max_s: dwell time range in seconds (default 900..1800)
// seed: master seed for the dwell-time randomization
inline AustinData loadAustin(const std::string& edge_path,
                             const std::string& label_path,
                             int s_min_s = 900, int s_max_s = 1800,
                             int seed = 20260928) {
    AustinData data;

    // ---- edges -----------------------------------------------------------
    {
        std::ifstream f(edge_path);
        if (!f.is_open()) return data;
        std::string line;
        int max_node = -1;
        std::vector<Edge> raw;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::istringstream is(line);
            Edge e;
            is >> e.u >> e.v >> e.t_dep >> e.t_arr >> e.w;
            max_node = std::max(max_node, std::max(e.u, e.v));
            raw.push_back(e);
        }
        data.g.init(max_node + 1);
        for (Edge& e : raw) data.g.addEdge(e);
    }

    // ---- labels ----------------------------------------------------------
    {
        std::ifstream f(label_path);
        if (!f.is_open()) return data;
        std::string line;
        std::vector<std::vector<int>> labels(data.g.n);
        int max_kw = -1;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::istringstream is(line);
            int node = -1;
            is >> node;
            if (node < 0 || node >= data.g.n) continue;
            int kw;
            while (is >> kw) {
                labels[node].push_back(kw);
                max_kw = std::max(max_kw, kw);
            }
        }
        data.n_keywords = max_kw + 1;
        data.poi.cov.assign(data.g.n, std::vector<double>(data.n_keywords, 0.0));
        data.poi.s.assign(data.g.n, 0);
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> sdur(s_min_s, s_max_s);
        for (int v = 0; v < data.g.n; ++v) {
            if (labels[v].empty()) continue;
            data.poi.s[v] = sdur(rng);
            for (int k : labels[v]) data.poi.cov[v][k] = 1.0;
        }
    }
    return data;
}

}  // namespace kcrtn
