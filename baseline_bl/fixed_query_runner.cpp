// Fixed-query runner for the city-scale BL baseline.
//
// This executable reads the same processed network tables and the same
// query_id row as src/main.cpp.  It never generates a query or dwell value.
// BL conversion and search are both included in search_ms.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bl_time_expanded.hpp"

using namespace kcrtn;

namespace {

using Clock = std::chrono::steady_clock;

double milliseconds(Clock::time_point first, Clock::time_point second) {
    return std::chrono::duration<double, std::milli>(second - first).count();
}

std::vector<std::string> parseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (c == ',' && !quoted) {
            fields.push_back(current);
            current.clear();
        } else if (c != '\r') {
            current.push_back(c);
        }
    }
    if (quoted) throw std::runtime_error("unterminated quoted CSV field");
    fields.push_back(current);
    return fields;
}

class CsvReader {
public:
    explicit CsvReader(const std::string& path) : stream_(path) {
        if (!stream_) throw std::runtime_error("cannot open CSV: " + path);
        std::string line;
        if (!std::getline(stream_, line)) throw std::runtime_error("empty CSV: " + path);
        const auto header = parseCsvLine(line);
        for (std::size_t i = 0; i < header.size(); ++i) index_[header[i]] = i;
    }

    bool next(std::vector<std::string>& row) {
        std::string line;
        if (!std::getline(stream_, line)) return false;
        row = parseCsvLine(line);
        return true;
    }

    const std::string& get(const std::vector<std::string>& row,
                           const std::string& name) const {
        const auto found = index_.find(name);
        if (found == index_.end()) throw std::runtime_error("missing CSV column: " + name);
        if (found->second >= row.size()) throw std::runtime_error("short CSV row");
        return row[found->second];
    }

    bool has(const std::string& name) const { return index_.count(name) != 0; }

private:
    std::ifstream stream_;
    std::unordered_map<std::string, std::size_t> index_;
};

std::vector<std::string> splitList(const std::string& text) {
    std::vector<std::string> result;
    std::string current;
    for (char c : text) {
        if (c == '|' || c == ';') {
            if (!current.empty()) result.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) result.push_back(current);
    return result;
}

std::string jsonEscape(const std::string& value) {
    std::string result = "\"";
    for (char c : value) {
        switch (c) {
            case '\"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result.push_back(c); break;
        }
    }
    result.push_back('\"');
    return result;
}

std::string argument(int argc, char** argv, const std::string& name,
                     const std::string& fallback = "") {
    for (int i = 1; i + 1 < argc; i += 2) {
        if (argv[i] == name) return argv[i + 1];
    }
    if (!fallback.empty()) return fallback;
    throw std::runtime_error("required argument: " + name);
}

struct Dataset {
    Graph graph;
    POIProfile poi;
    std::vector<std::string> edge_ids;
    std::unordered_map<std::string, int> station_ids;
    std::unordered_map<std::string, int> keyword_ids;
};

Dataset loadDataset(const std::string& stations_path,
                    const std::string& trips_path) {
    Dataset data;
    struct Membership { int station; int keyword; double coverage; };
    std::vector<Membership> memberships;

    CsvReader stations(stations_path);
    std::vector<std::string> row;
    while (stations.next(row)) {
        const std::string station_name = stations.get(row, "station_id");
        auto station_inserted = data.station_ids.emplace(
            station_name, static_cast<int>(data.station_ids.size()));
        const int station = station_inserted.first->second;
        const std::string keyword_name = stations.get(row, "keyword");
        if (keyword_name.empty()) continue;
        auto keyword_inserted = data.keyword_ids.emplace(
            keyword_name, static_cast<int>(data.keyword_ids.size()));
        memberships.push_back({
            station, keyword_inserted.first->second,
            std::stod(stations.get(row, "coverage")),
        });
    }
    if (data.station_ids.empty()) throw std::runtime_error("station table has no stations");

    data.graph.init(static_cast<int>(data.station_ids.size()));
    data.poi.cov.assign(data.graph.n,
                        std::vector<double>(data.keyword_ids.size(), 0.0));
    data.poi.s.assign(data.graph.n, 0);
    for (const Membership& item : memberships) {
        data.poi.cov[item.station][item.keyword] = item.coverage;
        if (item.coverage > 0.0) data.poi.s[item.station] = 1;
    }

    CsvReader trips(trips_path);
    while (trips.next(row)) {
        const std::string from_name = trips.get(row, "from");
        const std::string to_name = trips.get(row, "to");
        const auto from = data.station_ids.find(from_name);
        const auto to = data.station_ids.find(to_name);
        if (from == data.station_ids.end() || to == data.station_ids.end()) {
            throw std::runtime_error("trip refers to an unknown station");
        }
        Edge edge;
        edge.u = from->second;
        edge.v = to->second;
        edge.t_dep = std::stoi(trips.get(row, "departure_min"));
        edge.t_arr = std::stoi(trips.get(row, "arrival_min"));
        edge.w = std::stod(trips.get(row, "cost"));
        edge.original_id = static_cast<int>(data.edge_ids.size());
        data.edge_ids.push_back(trips.get(row, "edge_id"));
        data.graph.addEdge(edge);
    }
    if (data.graph.edges.empty()) throw std::runtime_error("trip table has no edges");
    return data;
}

struct FixedQuery {
    Query query;
    std::string query_id;
    std::string city;
    std::string parameter;
    std::string value;
    std::string query_no;
    std::uint64_t seed = 0;
};

FixedQuery loadQuery(const std::string& path, const std::string& requested,
                     const Dataset& data) {
    CsvReader queries(path);
    std::vector<std::string> row;
    while (queries.next(row)) {
        if (queries.get(row, "query_id") != requested) continue;
        FixedQuery result;
        result.query_id = requested;
        result.city = queries.get(row, "city");
        result.parameter = queries.get(row, "parameter");
        result.value = queries.get(row, "value");
        result.query_no = queries.has("query_no") ? queries.get(row, "query_no") : "";
        result.seed = static_cast<std::uint64_t>(
            std::stoull(queries.get(row, "seed")));

        const auto source = data.station_ids.find(queries.get(row, "source"));
        const auto target = data.station_ids.find(queries.get(row, "target"));
        if (source == data.station_ids.end() || target == data.station_ids.end()) {
            throw std::runtime_error("query refers to an unknown station");
        }
        Query& query = result.query;
        query.vs = source->second;
        query.vt = target->second;
        query.ta = std::stoi(queries.get(row, "start_min"));
        query.tb = std::stoi(queries.get(row, "end_min"));
        query.Delta = std::stod(queries.get(row, "budget"));
        query.k = std::stoi(queries.get(row, "k"));
        query.dwell_seconds = std::stoi(queries.get(row, "dwell_seconds"));
        if (query.dwell_seconds <= 0) throw std::runtime_error("dwell_seconds must be positive");

        const auto keyword_names = splitList(queries.get(row, "keywords"));
        const auto weight_names = splitList(queries.get(row, "weights"));
        if (keyword_names.empty() || keyword_names.size() != weight_names.size()) {
            throw std::runtime_error("query keyword/weight mismatch");
        }
        double weight_sum = 0.0;
        for (std::size_t i = 0; i < keyword_names.size(); ++i) {
            const auto keyword = data.keyword_ids.find(keyword_names[i]);
            if (keyword == data.keyword_ids.end()) {
                throw std::runtime_error("query keyword is absent from station table: " +
                                         keyword_names[i]);
            }
            query.K.push_back(keyword->second);
            query.lambda.push_back(std::stod(weight_names[i]));
            weight_sum += query.lambda.back();
        }
        if (std::fabs(weight_sum - 1.0) > 5e-6) {
            throw std::runtime_error("query weights do not sum to one");
        }
        return result;
    }
    throw std::runtime_error("query_id not found: " + requested);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if ((argc - 1) % 2 != 0) {
            throw std::runtime_error("arguments must be --name value pairs");
        }
        const std::string stations = argument(argc, argv, "--stations");
        const std::string trips = argument(argc, argv, "--trips");
        const std::string queries = argument(argc, argv, "--queries");
        const std::string query_id = argument(argc, argv, "--query-id");
        const std::string algorithm = argument(argc, argv, "--algorithm", "BL");
        const int64 timeout_ms = std::stoll(
            argument(argc, argv, "--timeout-ms", "300000"));
        if (algorithm != "BL") throw std::runtime_error("algorithm must be BL");
        if (timeout_ms <= 0) throw std::runtime_error("timeout-ms must be positive");

        const auto started = Clock::now();
        Dataset data = loadDataset(stations, trips);
        FixedQuery fixed = loadQuery(queries, query_id, data);
        const auto loaded = Clock::now();
        BLResult result = blTimeExpandedTopK(
            data.graph, data.poi, fixed.query, timeout_ms);
        const auto finished = Clock::now();

        if (!result.timed_out) {
            for (const Route& route : result.routes) {
                const BLRouteCheck check = validateBLRoute(
                    data.graph, data.poi, fixed.query, route);
                if (!check.feasible) {
                    throw std::runtime_error("independent BL route check failed: " +
                                             check.reason);
                }
            }
        }

        const std::string status = result.timed_out
                                       ? "timeout"
                                       : (result.routes.empty() ? "no_route" : "ok");
        const double load_ms = milliseconds(started, loaded);
        const double search_ms = milliseconds(loaded, finished);
        std::cout << std::setprecision(17)
                  << "{\"query_id\":" << jsonEscape(fixed.query_id)
                  << ",\"algorithm\":\"BL\""
                  << ",\"status\":" << jsonEscape(status)
                  << ",\"runtime_ms\":" << (load_ms + search_ms)
                  << ",\"load_ms\":" << load_ms
                  << ",\"preprocess_ms\":0"
                  << ",\"search_ms\":" << search_ms
                  << ",\"expanded\":" << result.expanded_labels
                  << ",\"static_nodes\":" << result.static_nodes
                  << ",\"static_arcs\":" << result.static_arcs
                  << ",\"dwell_seconds\":" << fixed.query.dwell_seconds
                  << ",\"seed\":" << fixed.seed
                  << ",\"solutions\":[";
        if (!result.timed_out) {
            for (std::size_t i = 0; i < result.routes.size(); ++i) {
                if (i) std::cout << ',';
                const Route& route = result.routes[i];
                std::cout << "{\"edge_ids\":[";
                for (std::size_t j = 0; j < route.edges.size(); ++j) {
                    if (j) std::cout << ',';
                    const int edge = route.edges[j];
                    if (edge < 0 || edge >= static_cast<int>(data.edge_ids.size())) {
                        throw std::runtime_error("BL returned invalid edge id");
                    }
                    std::cout << jsonEscape(data.edge_ids[edge]);
                }
                std::cout << "],\"score\":" << route.kc
                          << ",\"budget\":" << route.bs
                          << ",\"arrival_min\":" << route.t_arr << '}';
            }
        }
        std::cout << "]}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "kcrn_bl: " << error.what() << '\n';
        return 2;
    }
}
