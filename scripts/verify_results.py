#!/usr/bin/env python3
"""Independent route-level feasibility and dwell-scoring audit; optional small oracle."""
import argparse
import csv
import json
import math
from collections import defaultdict

def data(stations_file, trips_file, queries_file, needed_edges=None):
    def rows(file):
        with open(file, newline="", encoding="utf-8-sig") as f:
            return list(csv.DictReader(f))
    stations = defaultdict(lambda: {"cov": {}})
    for row in rows(stations_file):
        v = stations[row["station_id"]]
        if row["keyword"]:
            v["cov"][row["keyword"]] = float(row["coverage"])
    trips = {}
    with open(trips_file, newline="", encoding="utf-8-sig") as f:
        for r in csv.DictReader(f):
            if needed_edges is None or r["edge_id"] in needed_edges:
                trips[r["edge_id"]] = dict(r, departure_min=int(r["departure_min"]),
                    arrival_min=int(r["arrival_min"]), cost=float(r["cost"]))
    queries = {r["query_id"]: dict(r, start_min=int(r["start_min"]),
        end_min=int(r["end_min"]), budget=float(r["budget"]), k=int(r["k"]),
        dwell_seconds=float(r["dwell_seconds"]),
        keywords=r["keywords"].split("|"),
        weights=list(map(float, r["weights"].split("|")))) for r in rows(queries_file)}
    return stations, trips, queries

def evaluate(ids, q, stations, trips):
    if not ids or len(ids) != len(set(ids)):
        raise ValueError("empty or repeated edge")
    now, at = q["source"], q["start_min"]
    visited = set()
    spent = 0.0
    for i, ident in enumerate(ids):
        edge = trips[ident]
        dep, arr = edge["departure_min"], edge["arrival_min"]
        if edge["from"] != now or dep < at or dep < q["start_min"] or arr > q["end_min"]:
            raise ValueError("invalid timetable or station connection")
        if dep - at >= q["dwell_seconds"]:
            visited.add(now)
        spent += edge["cost"]
        at, now = arr, edge["to"]
        if now == q["target"] and i + 1 < len(ids):
            raise ValueError("route continues after first target arrival")
    if now != q["target"] or spent > q["budget"] + 1e-9:
        raise ValueError("wrong target or budget")
    if q["end_min"] - at >= q["dwell_seconds"]:
        visited.add(now)
    value = sum(weight * (1 - math.prod(1 - stations[v]["cov"].get(key, 0)
                                   for v in visited)) for key, weight in zip(q["keywords"], q["weights"]))
    return value, spent, at

def oracle(q, stations, trips):
    outgoing = defaultdict(list)
    for e in trips.values():
        outgoing[e["from"]].append(e)
    answer = []
    def dfs(v, at, cost, path, used):
        if v == q["target"]:
            value, budget, arrival = evaluate(path, q, stations, trips)
            answer.append((value, tuple(path), budget, arrival))
            return
        for e in outgoing[v]:
            ident = e["edge_id"]
            if (ident not in used and e["departure_min"] >= at and
                e["departure_min"] >= q["start_min"] and e["arrival_min"] <= q["end_min"] and
                cost + e["cost"] <= q["budget"] + 1e-9):
                used.add(ident)
                path.append(ident)
                dfs(e["to"], e["arrival_min"], cost + e["cost"], path, used)
                path.pop()
                used.remove(ident)
    dfs(q["source"], q["start_min"], 0, [], set())
    return sorted(answer, key=lambda x: (-x[0], x[1]))[:q["k"]]

def verify(record, stations, trips, queries, exhaustive=False):
    if record["status"] in ("timeout", "resource_limit", "error"):
        return dict(query_id=record["query_id"], algorithm=record["algorithm"],
            execution_status=record["status"], validation_status="not_checked",
            route_records_checked=0, returned_routes=0,
            requested_k=record.get("requested_k", ""),
            all_returned_routes_feasible="", all_routes_distinct="",
            scores_recomputed="", exhaustive_oracle_checked=0)
    q = queries[record["query_id"]]
    solutions = record["solutions"]
    assert len(solutions) <= q["k"]
    assert record["status"] == ("ok" if solutions else "no_route")
    sequences = [tuple(s["edge_ids"]) for s in solutions]
    assert len(sequences) == len(set(sequences)), "duplicate routes"
    for s in solutions:
        value, cost, arrival = evaluate(s["edge_ids"], q, stations, trips)
        assert math.isclose(s["score"], value, rel_tol=1e-10, abs_tol=1e-10)
        assert math.isclose(s["budget"], cost, rel_tol=1e-10, abs_tol=1e-10)
        assert s["arrival_min"] == arrival
    assert all(solutions[i]["score"] >= solutions[i+1]["score"] - 1e-12
        for i in range(len(solutions)-1)), "unsorted score"
    if exhaustive:
        truth = oracle(q, stations, trips)
        if record["algorithm"] in ("A", "BL_enum"):
            assert sequences == [r[1] for r in truth], "exact search differs from independent oracle"
        assert bool(sequences) == bool(truth), "incorrect route-existence status"
    return dict(query_id=record["query_id"], algorithm=record["algorithm"],
        execution_status=record["status"], validation_status="passed",
        route_records_checked=len(solutions), returned_routes=len(solutions),
        requested_k=q["k"], all_returned_routes_feasible=1,
        all_routes_distinct=1, scores_recomputed=1,
        exhaustive_oracle_checked=int(exhaustive))

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for field in ("stations", "trips", "queries", "results"):
        p.add_argument(f"--{field}", required=True)
    p.add_argument("--exhaustive", action="store_true", help="SMALL INSTANCES ONLY")
    p.add_argument("--city", help="only audit this city's records in a combined JSONL")
    p.add_argument("--audit-out", help="optional per-query validation CSV")
    a = p.parse_args()
    needed = None if a.exhaustive else set()
    if needed is not None:
        with open(a.results, encoding="utf-8") as f:
            for line in f:
                record = json.loads(line)
                if not a.city or record.get("city") == a.city:
                    needed.update(edge for solution in record["solutions"] for edge in solution["edge_ids"])
    s, t, q = data(a.stations, a.trips, a.queries, needed)
    count = 0
    audit_rows = []
    with open(a.results, encoding="utf-8") as f:
        for line in f:
            item = json.loads(line)
            if a.city and item.get("city") != a.city:
                continue
            audit_rows.append(verify(item, s, t, q, a.exhaustive))
            count += 1
    if a.audit_out:
        fields = ["query_id", "algorithm", "execution_status", "validation_status",
            "route_records_checked", "returned_routes", "requested_k",
            "all_returned_routes_feasible", "all_routes_distinct",
            "scores_recomputed", "exhaustive_oracle_checked"]
        with open(a.audit_out, "w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            writer.writerows(audit_rows)
    print(f"verified {count} records ({'with' if a.exhaustive else 'without'} exhaustive oracle)")

if __name__ == "__main__":
    main()
