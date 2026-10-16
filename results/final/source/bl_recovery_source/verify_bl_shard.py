#!/usr/bin/env python3
"""Independently validate raw-edge BL route output for one query shard."""

import argparse
import csv
from collections import defaultdict
from pathlib import Path


def rows(path):
    with open(path, newline="", encoding="utf-8-sig") as source:
        return list(csv.DictReader(source))


def load_edges(path):
    result = []
    with open(path, encoding="utf-8") as source:
        for line_number, line in enumerate(source, start=1):
            if not line.strip():
                continue
            values = line.split()
            if len(values) != 5:
                raise RuntimeError(f"invalid edge row {line_number} in {path}")
            result.append((int(values[0]), int(values[1]), int(values[2]),
                           int(values[3]), float(values[4])))
    return result


def load_labels(path):
    result = defaultdict(set)
    with open(path, encoding="utf-8") as source:
        for line in source:
            values = [int(value) for value in line.split()]
            if values:
                result[values[0]].update(values[1:])
    return result


def validate(query, route, edges, labels):
    edge_ids = [int(value) for value in route["edge_ids"].split(";") if value]
    if not edge_ids:
        return False, "empty route", 0.0, 0.0
    if len(edge_ids) != len(set(edge_ids)):
        return False, "repeated scheduled edge", 0.0, 0.0
    source = int(query["source"])
    target = int(query["target"])
    start = int(query["start_min"])
    end = int(query["end_min"])
    budget_limit = float(query["budget"])
    dwell = int(query["dwell_seconds"])
    station = source
    time = start
    budget = 0.0
    path_edges = []
    for edge_id in edge_ids:
        if edge_id < 0 or edge_id >= len(edges):
            return False, "invalid edge id", budget, 0.0
        edge = edges[edge_id]
        u, v, departure, arrival, cost = edge
        if u != station:
            return False, "disconnected edge sequence", budget, 0.0
        if departure < time:
            return False, "missed departure", budget, 0.0
        if departure < start or arrival > end:
            return False, "outside query window", budget, 0.0
        path_edges.append(edge)
        budget += cost
        station = v
        time = arrival
    if station != target:
        return False, "route does not end at target", budget, 0.0
    if budget > budget_limit + 1e-9:
        return False, "budget exceeded", budget, 0.0

    covered = set()
    first = path_edges[0]
    if first[2] - start >= dwell:
        covered.update(labels[first[0]])
    for index, edge in enumerate(path_edges):
        next_departure = path_edges[index + 1][2] if index + 1 < len(path_edges) else end
        if next_departure - edge[3] >= dwell:
            covered.update(labels[edge[1]])
    keywords = [int(value) for value in query["keywords"].split("|")]
    weights = [float(value) for value in query["weights"].split("|")]
    score = sum(weight for keyword, weight in zip(keywords, weights)
                if keyword in covered)
    if abs(budget - float(route["budget"])) > 1e-6:
        return False, "recorded budget mismatch", budget, score
    if abs(score - float(route["kc"])) > 1e-6:
        return False, "recorded score mismatch", budget, score
    return True, "OK", budget, score


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--queries", required=True)
    parser.add_argument("--routes", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--project-root", default=".")
    args = parser.parse_args()

    query_rows = rows(args.queries)
    queries = {row["query_id"]: row for row in query_rows}
    route_rows = rows(args.routes)
    root = Path(args.project_root).resolve()
    cities = sorted({queries[row["query_id"]]["city"] for row in route_rows})
    datasets = {}
    for city in cities:
        base = root / "data" / "raw" / city
        datasets[city] = (load_edges(base / "edges.txt"),
                          load_labels(base / "labels.txt"))

    fields = [
        "query_id", "algorithm", "route_rank", "feasible", "reason",
        "recomputed_budget", "recomputed_kc", "duplicate_route",
    ]
    seen = defaultdict(set)
    failures = 0
    with open(args.out, "w", newline="", encoding="utf-8") as target:
        writer = csv.DictWriter(target, fieldnames=fields)
        writer.writeheader()
        for route in route_rows:
            query = queries[route["query_id"]]
            edges, labels = datasets[query["city"]]
            feasible, reason, budget, score = validate(
                query, route, edges, labels)
            route_key = route["edge_ids"]
            duplicate = route_key in seen[route["query_id"]]
            seen[route["query_id"]].add(route_key)
            if duplicate:
                feasible = False
                reason = "duplicate route"
            if not feasible:
                failures += 1
            writer.writerow({
                "query_id": route["query_id"],
                "algorithm": "BL",
                "route_rank": route["route_rank"],
                "feasible": int(feasible),
                "reason": reason,
                "recomputed_budget": f"{budget:.10f}",
                "recomputed_kc": f"{score:.10f}",
                "duplicate_route": int(duplicate),
            })
    print(f"validated {len(route_rows)} routes; failures={failures}")
    if failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
