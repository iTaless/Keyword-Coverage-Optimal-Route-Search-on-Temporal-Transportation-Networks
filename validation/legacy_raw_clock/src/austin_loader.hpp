// ============================================================================
// austin_loader.hpp
// ----------------------------------------------------------------------------
// Loader for the authors' Austin dataset format:
//   edges file: "u v dep arr w" per line, whitespace-separated,
//               raw clocks in HHMMSS, converted to elapsed seconds on load.
//   label file: "node kw1 kw2 ..." per line (keyword membership, 0/1).
//
// POIProfile construction:
//   * cov(v,k) = 1.0 if keyword k appears in node v's label list, else 0.
//   * s[v] marks whether v has a POI profile.  In the small-instance
//     validation driver, the loader is called with s_min_s=s_max_s=1;
//     the actual dwell threshold is the query-level Query::dwell value.
// ============================================================================

#pragma once

#include <algorithm>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>

#include "transit_router.hpp"

namespace kcrtn {

inline int rawClockToSeconds(int value) {
    if (value < 0 || value % 100 >= 60 || value / 100 % 100 >= 60)
        throw std::runtime_error("invalid raw HHMMSS clock");
    return value / 10000 * 3600 + value / 100 % 100 * 60 + value % 100;
}

struct AustinData {
    Graph g;
    POIProfile poi;
    int n_keywords = 0;
};

// edge_path: "u v dep arr w" lines
// label_path: "node kw..." lines
// s_min_s, s_max_s: positive POI-marker range; the validation driver passes
//                    1,1 because dwell eligibility uses Query::dwell
// seed: retained for deterministic marker generation in legacy callers
inline AustinData loadAustin(const std::string& edge_path,
                             const std::string& label_path,
                             int s_min_s = 600, int s_max_s = 1800,
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
            int departure, arrival;
            if (!(is >> e.u >> e.v >> departure >> arrival >> e.w))
                throw std::runtime_error("invalid raw edge row");
            e.t_dep = rawClockToSeconds(departure);
            e.t_arr = rawClockToSeconds(arrival);
            if (e.t_arr < e.t_dep)
                throw std::runtime_error("arrival precedes departure");
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
