#!/usr/bin/env python3
"""Run one fixed-query BL shard with per-query checkpoints and resume support."""

import argparse
import csv
import json
import subprocess
import sys
import time
from pathlib import Path


def read_queries(path):
    with open(path, newline="", encoding="utf-8-sig") as source:
        return list(csv.DictReader(source))


def load_completed(path):
    completed = set()
    if not path.exists():
        return completed
    with path.open(encoding="utf-8") as source:
        for number, line in enumerate(source, start=1):
            if not line.strip():
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as error:
                raise RuntimeError(f"invalid checkpoint line {number}: {error}")
            completed.add(record["query_id"])
    return completed


def dataset_args(root, city):
    if city not in {"Austin", "Houston"}:
        raise RuntimeError(f"unsupported city: {city}")
    base = root / "data" / "raw" / city
    return ["--edges", str(base / "edges.txt"),
            "--labels", str(base / "labels.txt")]


def derive_outputs(jsonl_path, queries):
    query_by_id = {row["query_id"]: row for row in queries}
    runtime_fields = [
        "query_id", "city", "parameter", "value", "query_no", "algorithm",
        "status", "recovery_runtime_sec", "search_runtime_sec",
        "timeout_threshold_sec", "dwell_seconds", "query_seed", "requested_k",
        "returned_distinct_routes", "best_kc", "route_sequences_available",
    ]
    route_fields = [
        "query_id", "city", "parameter", "value", "query_no", "algorithm",
        "route_rank", "kc", "budget", "arrival_time_sec", "edge_ids",
    ]
    runtime_path = jsonl_path.with_suffix(".runtime.csv")
    routes_path = jsonl_path.with_suffix(".routes.csv")
    with (runtime_path.open("w", newline="", encoding="utf-8") as runtime_file,
          routes_path.open("w", newline="", encoding="utf-8") as route_file):
        runtime_writer = csv.DictWriter(runtime_file, fieldnames=runtime_fields)
        route_writer = csv.DictWriter(route_file, fieldnames=route_fields)
        runtime_writer.writeheader()
        route_writer.writeheader()
        with jsonl_path.open(encoding="utf-8") as source:
            for line in source:
                if not line.strip():
                    continue
                record = json.loads(line)
                query = query_by_id[record["query_id"]]
                solutions = record.get("solutions", [])
                available = record["status"] in {"ok", "no_route"}
                runtime_writer.writerow({
                    "query_id": record["query_id"],
                    "city": query["city"],
                    "parameter": query["parameter"],
                    "value": query["value"],
                    "query_no": query.get("query_no", ""),
                    "algorithm": "BL",
                    "status": record["status"],
                    "recovery_runtime_sec": (
                        f"{record.get('wall_ms', 0.0) / 1000:.6f}"),
                    "search_runtime_sec": (
                        f"{record.get('search_ms', 0.0) / 1000:.6f}"
                        if available else ""),
                    "timeout_threshold_sec": record["timeout_threshold_sec"],
                    "dwell_seconds": query["dwell_seconds"],
                    "query_seed": query["seed"],
                    "requested_k": query["k"],
                    "returned_distinct_routes": len(solutions) if available else "",
                    "best_kc": solutions[0]["score"] if solutions else "",
                    "route_sequences_available": 1 if available else 0,
                })
                for rank, solution in enumerate(solutions):
                    route_writer.writerow({
                        "query_id": record["query_id"],
                        "city": query["city"],
                        "parameter": query["parameter"],
                        "value": query["value"],
                        "query_no": query.get("query_no", ""),
                        "algorithm": "BL",
                        "route_rank": rank,
                        "kc": solution["score"],
                        "budget": solution["budget"],
                        "arrival_time_sec": solution["arrival_min"],
                        "edge_ids": ";".join(solution["edge_ids"]),
                    })
    return runtime_path, routes_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--queries", required=True)
    parser.add_argument("--out", required=True,
                        help="checkpoint JSONL path")
    parser.add_argument("--bl-binary", required=True)
    parser.add_argument("--project-root", default=".")
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--external-grace", type=float, default=60.0)
    args = parser.parse_args()
    if args.timeout <= 0 or args.external_grace < 0:
        parser.error("timeouts must be valid")

    root = Path(args.project_root).resolve()
    queries = read_queries(args.queries)
    if not queries:
        raise RuntimeError("query shard is empty")
    if len(queries) != len({row["query_id"] for row in queries}):
        raise RuntimeError("duplicate query_id in shard")
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    completed = load_completed(out)
    mode = "a" if out.exists() else "w"
    with out.open(mode, encoding="utf-8") as target:
        for index, query in enumerate(queries, start=1):
            query_id = query["query_id"]
            if query_id in completed:
                print(f"{index}/{len(queries)} {query_id} already complete",
                      file=sys.stderr, flush=True)
                continue
            command = [str(Path(args.bl_binary).resolve())]
            command += dataset_args(root, query["city"])
            command += [
                "--queries", str(Path(args.queries).resolve()),
                "--query-id", query_id,
                "--algorithm", "BL",
                "--timeout-ms", str(int(args.timeout * 1000)),
            ]
            started = time.perf_counter()
            try:
                run = subprocess.run(
                    command, capture_output=True, text=True,
                    timeout=args.timeout + args.external_grace)
                if run.returncode:
                    record = {
                        "query_id": query_id,
                        "algorithm": "BL",
                        "status": "error",
                        "exit_code": run.returncode,
                        "error": run.stderr[-4000:],
                        "solutions": [],
                    }
                else:
                    record = json.loads(run.stdout)
            except subprocess.TimeoutExpired:
                record = {
                    "query_id": query_id,
                    "algorithm": "BL",
                    "status": "timeout",
                    "solutions": [],
                }
            record["wall_ms"] = (time.perf_counter() - started) * 1000
            record["timeout_threshold_sec"] = args.timeout
            target.write(json.dumps(record, ensure_ascii=False) + "\n")
            target.flush()
            print(f"{index}/{len(queries)} {query_id} {record['status']}",
                  file=sys.stderr, flush=True)

    runtime_path, routes_path = derive_outputs(out, queries)
    print(out)
    print(runtime_path)
    print(routes_path)


if __name__ == "__main__":
    main()
