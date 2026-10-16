#!/usr/bin/env python3
"""Run BL/A*/AP on identical fixed query_id rows and record provenance."""
import argparse
import csv
import hashlib
import json
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path

from verify_results import data, verify

def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("binary", "queries", "out"):
        p.add_argument(f"--{name}", required=True)
    p.add_argument("--bl-binary",
        help="fixed-query BL executable; required when --algorithms contains BL")
    p.add_argument("--dataset", action="append", required=True, metavar="CITY:STATIONS:TRIPS")
    p.add_argument("--timeout", type=float, default=300.0)
    p.add_argument("--rho", type=int, default=8)
    p.add_argument("--rounds", type=int, default=20)
    p.add_argument("--runs", type=int, default=20)
    p.add_argument("--runs-policy", choices=("k", "fixed"), default="k",
        help="AP restart count J: use the query's k (default) or the fixed --runs value")
    p.add_argument("--algorithms", default="BL,A,AP",
        help="comma-separated BL,A,AP; BL_enum is a small-instance oracle only")
    p.add_argument("--query-witnesses", help="optional feasibility-witness CSV to hash in the manifest")
    p.add_argument("--time-unit", choices=("seconds",), default="seconds")
    p.add_argument("--dwell-description", default="unspecified")
    p.add_argument("--city", help="run only this city from a combined query file")
    p.add_argument("--main-study-only", action="store_true",
        help="exclude supplementary rows whose parameter is default")
    p.add_argument("--limit", type=int, default=0, help="development smoke test")
    a = p.parse_args()
    feeds = {}
    for item in a.dataset:
        city, st, tr = item.split(":", 2)
        feeds[city] = (st, tr)
    algorithms = a.algorithms.split(",")
    if not set(algorithms) <= {"BL", "A", "BL_enum", "AP"} or a.timeout <= 0:
        p.error("invalid algorithms or timeout")
    if "BL" in algorithms and not a.bl_binary:
        p.error("--bl-binary is required when --algorithms contains BL")
    with open(a.queries, newline="", encoding="utf-8-sig") as f:
        queries = list(csv.DictReader(f))
    if a.city:
        queries = [row for row in queries if row["city"] == a.city]
    if a.main_study_only:
        queries = [row for row in queries if row["parameter"] != "default"]
    if a.limit:
        queries = queries[:a.limit]
    out = Path(a.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    project_root = Path(__file__).resolve().parents[1]
    compiler_report = "not detected"
    if shutil.which("g++"):
        compiler = subprocess.run(["g++", "--version"], capture_output=True, text=True)
        compiler_report = compiler.stdout.splitlines()[0] if compiler.stdout else "unavailable"
    meta = dict(binary=str(Path(a.binary).resolve()), binary_sha256=sha(a.binary),
        bl_binary=(str(Path(a.bl_binary).resolve()) if a.bl_binary else None),
        bl_binary_sha256=(sha(a.bl_binary) if a.bl_binary else None),
        cpp_source_sha256=sha(project_root / "src/main.cpp"),
        bl_source_sha256=sha(project_root / "baseline_bl/fixed_query_runner.cpp"),
        runner_source_sha256=sha(Path(__file__)),
        compiler_reported=compiler_report,
        query_sha256=sha(a.queries), datasets={city: dict(stations=str(Path(st).resolve()),
        stations_sha256=sha(st), trips=str(Path(tr).resolve()), trips_sha256=sha(tr))
        for city, (st, tr) in feeds.items()}, timeout_sec=a.timeout,
        algorithms=algorithms, rho=a.rho, rounds=a.rounds, runs_policy=a.runs_policy,
        fixed_runs=a.runs,
        reported_runtime="query_runtime_ms = preprocess_ms + search_ms",
        timeout_scope="external subprocess wall clock, including input loading",
        time_unit=a.time_unit, dwell_description=a.dwell_description,
        implementation_time_basis="processed_seconds",
        query_witnesses=(dict(path=str(Path(a.query_witnesses).resolve()),
            sha256=sha(a.query_witnesses)) if a.query_witnesses else None),
        python=platform.python_version(), platform=platform.platform(),
        generated_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        main_study_only=a.main_study_only,
        source_revision=(subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True,
            text=True).stdout.strip() if shutil.which("git") else "") or "unversioned")
    out.with_suffix(".manifest.json").write_text(json.dumps(meta, indent=2), encoding="utf-8")
    total = len(queries) * len(algorithms)
    with out.open("w", encoding="utf-8") as f:
        for ix, row in enumerate(queries):
            city = row["city"]
            st, tr = feeds[city]
            for alg in algorithms:
                query_runs = int(row["k"]) if a.runs_policy == "k" else a.runs
                if alg == "BL":
                    args = [a.bl_binary, "--stations", st, "--trips", tr,
                        "--queries", a.queries, "--query-id", row["query_id"],
                        "--algorithm", "BL", "--timeout-ms", str(int(a.timeout * 1000))]
                else:
                    args = [a.binary, "--stations", st, "--trips", tr,
                        "--queries", a.queries, "--query-id", row["query_id"],
                        "--algorithm", alg, "--rho", str(a.rho),
                        "--rounds", str(a.rounds), "--runs", str(query_runs)]
                start = time.perf_counter()
                try:
                    run = subprocess.run(args, capture_output=True, text=True, timeout=a.timeout)
                    if run.returncode:
                        status = "resource_limit" if run.returncode in (-9, 137) or "bad_alloc" in run.stderr else "error"
                        result = dict(query_id=row["query_id"], algorithm=alg, status=status,
                            exit_code=run.returncode, error=run.stderr[:2000], solutions=[])
                    else:
                        result = json.loads(run.stdout)
                except subprocess.TimeoutExpired:
                    result = dict(query_id=row["query_id"], algorithm=alg, status="timeout", solutions=[])
                result["wall_ms"] = (time.perf_counter() - start) * 1000
                result["time_basis"] = "processed_seconds"
                if result["status"] in ("ok", "no_route"):
                    result["query_runtime_ms"] = result["preprocess_ms"] + result["search_ms"]
                result["city"] = city
                result["parameter"] = row["parameter"]
                result["value"] = row["value"]
                result["query_no"] = int(row.get("query_no") or 0)
                result["dwell_seconds"] = int(row["dwell_seconds"])
                result["query_seed"] = int(row["seed"])
                result["requested_k"] = int(row["k"])
                f.write(json.dumps(result, ensure_ascii=False) + "\n")
                f.flush()
                print(f"{ix*len(algorithms)+algorithms.index(alg)+1}/{total} {row['query_id']} {alg} {result['status']}", file=sys.stderr)
    # Audit after all C++ processes exit to avoid holding two city-sized
    # Python dictionaries alongside an event graph in the subprocess.
    audit_rows = []
    for city, (st, tr) in feeds.items():
        needed = set()
        with out.open(encoding="utf-8") as f:
            for line in f:
                record = json.loads(line)
                if record["city"] == city:
                    needed.update(edge for solution in record["solutions"] for edge in solution["edge_ids"])
        cached = data(st, tr, a.queries, needed)
        with out.open(encoding="utf-8") as f:
            for line in f:
                record = json.loads(line)
                if record["city"] == city:
                    audit = verify(record, *cached)
                    audit.update(city=city, parameter=record["parameter"], value=record["value"])
                    audit_rows.append(audit)
        del cached
    audit_path = out.with_suffix(".validation.csv")
    audit_fields = ["city", "parameter", "value", "query_id", "algorithm",
        "execution_status", "validation_status", "route_records_checked",
        "returned_routes", "requested_k", "all_returned_routes_feasible",
        "all_routes_distinct", "scores_recomputed", "exhaustive_oracle_checked"]
    with audit_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=audit_fields)
        writer.writeheader()
        writer.writerows(audit_rows)

    runtime_path = out.with_suffix(".runtime.csv")
    route_path = out.with_suffix(".routes.csv")
    runtime_fields = ["query_id", "city", "parameter", "value", "query_no",
        "algorithm", "status", "runtime_sec", "timeout_threshold_sec",
        "dwell_seconds", "query_seed", "requested_k", "returned_distinct_routes",
        "best_kc", "time_basis"]
    route_fields = ["query_id", "city", "parameter", "value", "query_no",
        "algorithm", "route_rank", "kc", "budget", "arrival_time_sec", "edge_ids", "time_basis"]
    with (runtime_path.open("w", newline="", encoding="utf-8") as runtime_file,
          route_path.open("w", newline="", encoding="utf-8") as route_file):
        runtime_writer = csv.DictWriter(runtime_file, fieldnames=runtime_fields)
        route_writer = csv.DictWriter(route_file, fieldnames=route_fields)
        runtime_writer.writeheader()
        route_writer.writeheader()
        with out.open(encoding="utf-8") as source:
            for line in source:
                record = json.loads(line)
                solutions = record.get("solutions", [])
                completed = record["status"] in ("ok", "no_route")
                runtime_writer.writerow(dict(
                    query_id=record["query_id"], city=record["city"],
                    parameter=record["parameter"], value=record["value"],
                    query_no=record["query_no"], algorithm=record["algorithm"],
                    status=record["status"],
                    runtime_sec=(f"{record['query_runtime_ms'] / 1000:.6f}" if completed else ""),
                    timeout_threshold_sec=a.timeout,
                    dwell_seconds=record["dwell_seconds"],
                    query_seed=record["query_seed"], requested_k=record["requested_k"],
                    returned_distinct_routes=len(solutions) if completed else "",
                    best_kc=(max(s["score"] for s in solutions) if solutions else ""), time_basis="processed_seconds"))
                for rank, solution in enumerate(solutions):
                    route_writer.writerow(dict(
                        query_id=record["query_id"], city=record["city"],
                        parameter=record["parameter"], value=record["value"],
                        query_no=record["query_no"], algorithm=record["algorithm"],
                        route_rank=rank, kc=solution["score"], budget=solution["budget"],
                        arrival_time_sec=solution["arrival_min"],
                        edge_ids=";".join(solution["edge_ids"]), time_basis="processed_seconds"))
    print(f"{out}\n{runtime_path}\n{route_path}\n{audit_path}")

if __name__ == "__main__":
    main()
