#!/usr/bin/env python3
"""Normalize the author-workstation result archive into results/final.

This is a format conversion only.  It never executes BL, A*, or AP and never
fills unavailable BL route sequences with invented empty answers.
"""

import argparse
import csv
import hashlib
import json
from collections import Counter, defaultdict
from datetime import datetime, timezone
from pathlib import Path


def rows(path):
    with Path(path).open(newline="", encoding="utf-8-sig") as stream:
        return list(csv.DictReader(stream))


def write_csv(path, fieldnames, records):
    with Path(path).open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(records)


def digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def normalized_sweep(value):
    return {
        "Default": "default",
        "K": "keywords",
        "Delta": "budget",
        "Window": "window",
        "k": "k",
    }[value]


def normalized_algorithm(value):
    return "A" if value == "A*" else value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--queries", required=True)
    parser.add_argument("--runtime", required=True)
    parser.add_argument("--routes", required=True)
    parser.add_argument("--route-validation", required=True)
    parser.add_argument("--query-summary", required=True)
    parser.add_argument("--group-summary", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--runtime-workbook")
    parser.add_argument("--runtime-summary-workbook")
    args = parser.parse_args()

    output = Path(args.out_dir)
    output.mkdir(parents=True, exist_ok=True)

    queries = rows(args.queries)
    query_by_id = {record["query_id"]: record for record in queries}
    query_id_by_key = {
        (record["city"], record["parameter"], record["value"],
         str(int(float(record["query_no"])))): record["query_id"]
        for record in queries
    }

    runtime = rows(args.runtime)
    route_source = rows(args.routes)
    validation_source = rows(args.route_validation)
    query_summary_source = rows(args.query_summary)
    group_summary_source = rows(args.group_summary)

    routes_by_execution = defaultdict(list)
    normalized_routes = []
    for record in route_source:
        algorithm = normalized_algorithm(record["algorithm"])
        normalized = {
            "query_id": record["query_id"],
            "city": record["city"],
            "parameter": record["parameter"],
            "value": record["value"],
            "query_no": record["query_no"],
            "algorithm": algorithm,
            "route_rank": record["route_rank"],
            "kc": record["recorded_kc"],
            "budget": record["budget"],
            "arrival_time_sec": record["arrival_time_sec"],
            "edge_ids": record["edge_ids"],
            "record_origin": "author_workstation_route_archive",
        }
        normalized_routes.append(normalized)
        routes_by_execution[(record["query_id"], algorithm)].append(normalized)

    route_fields = [
        "query_id", "city", "parameter", "value", "query_no", "algorithm",
        "route_rank", "kc", "budget", "arrival_time_sec", "edge_ids",
        "record_origin",
    ]
    write_csv(output / "route_level.routes.csv", route_fields, normalized_routes)

    main_query_ids = {record["query_id"] for record in runtime}
    main_routes = [record for record in normalized_routes
                   if record["query_id"] in main_query_ids]
    write_csv(output / "main_study.routes.csv", route_fields, main_routes)

    validation_by_execution = defaultdict(list)
    detail_validation = []
    for record in validation_source:
        copied = dict(record)
        copied["algorithm"] = normalized_algorithm(record["algorithm"])
        detail_validation.append(copied)
        validation_by_execution[(record["query_id"], copied["algorithm"])].append(copied)
    write_csv(
        output / "route_level.validation_details.csv",
        list(detail_validation[0]) if detail_validation else [],
        detail_validation,
    )

    runtime_records = []
    json_records = []
    validation_records = []
    for record in runtime:
        query = query_by_id[record["query_id"]]
        algorithm = normalized_algorithm(record["algorithm"])
        solutions = sorted(
            routes_by_execution.get((record["query_id"], algorithm), []),
            key=lambda item: int(float(item["route_rank"])),
        )
        route_sequences_available = algorithm != "BL"
        normalized_status = "timeout" if record["status"] == "timeout" else "completed"
        runtime_records.append({
            "query_id": record["query_id"],
            "city": record["city"],
            "parameter": record["parameter"],
            "value": record["value"],
            "query_no": record["query_no"],
            "algorithm": algorithm,
            "status": normalized_status,
            "runtime_sec": record["runtime_sec"],
            "timeout_threshold_sec": record["timeout_threshold_sec"],
            "dwell_seconds": query["dwell_seconds"],
            "query_seed": query["seed"],
            "requested_k": query["k"],
            "returned_distinct_routes": len(solutions) if route_sequences_available else "",
            "best_kc": max(float(item["kc"]) for item in solutions) if solutions else "",
            "route_sequences_available": int(route_sequences_available),
            "record_origin": "author_workstation_formal_rerun",
        })
        json_records.append({
            "query_id": record["query_id"],
            "algorithm": algorithm,
            "status": normalized_status,
            "query_runtime_ms": (float(record["runtime_sec"]) * 1000
                                 if record["runtime_sec"] else None),
            "timeout_threshold_sec": float(record["timeout_threshold_sec"]),
            "city": record["city"],
            "parameter": record["parameter"],
            "value": record["value"],
            "query_no": int(float(record["query_no"])),
            "dwell_seconds": int(float(query["dwell_seconds"])),
            "query_seed": int(float(query["seed"])),
            "requested_k": int(float(query["k"])),
            "route_sequences_available": route_sequences_available,
            "record_origin": "author_workstation_formal_rerun",
            "solutions": [{
                "edge_ids": item["edge_ids"].split(";") if item["edge_ids"] else [],
                "score": float(item["kc"]),
                "budget": float(item["budget"]),
                "arrival_min": int(float(item["arrival_time_sec"])),
            } for item in solutions],
        })

        checks = validation_by_execution.get((record["query_id"], algorithm), [])
        if not route_sequences_available:
            validation_status = "not_available"
            feasible = distinct = score_recomputed = score_match = ""
            checked = returned = ""
        elif not checks:
            validation_status = "no_route_record"
            feasible = distinct = score_recomputed = score_match = ""
            checked = returned = 0
        else:
            feasible = int(all(item["structural_validation_pass"] == "1" for item in checks))
            distinct = int(all(item["route_distinct_within_query_algorithm"] == "1"
                               for item in checks))
            score_recomputed = 1
            score_match = int(all(item["score_match_query_dwell"] == "1"
                                  for item in checks))
            validation_status = ("passed" if feasible and distinct and score_match
                                 else "score_mismatch" if feasible and distinct
                                 else "failed")
            checked = returned = len(checks)
        validation_records.append({
            "city": record["city"],
            "parameter": record["parameter"],
            "value": record["value"],
            "query_id": record["query_id"],
            "algorithm": algorithm,
            "execution_status": normalized_status,
            "validation_status": validation_status,
            "route_records_checked": checked,
            "returned_routes": returned,
            "requested_k": query["k"],
            "all_returned_routes_feasible": feasible,
            "all_routes_distinct": distinct,
            "scores_recomputed": score_recomputed,
            "all_recomputed_scores_match": score_match,
            "exhaustive_oracle_checked": 0,
            "record_origin": "independent_audit_of_author_route_archive",
        })

    runtime_fields = [
        "query_id", "city", "parameter", "value", "query_no", "algorithm",
        "status", "runtime_sec", "timeout_threshold_sec", "dwell_seconds",
        "query_seed", "requested_k", "returned_distinct_routes", "best_kc",
        "route_sequences_available", "record_origin",
    ]
    write_csv(output / "main_study.runtime.csv", runtime_fields, runtime_records)
    with (output / "main_study.jsonl").open("w", encoding="utf-8") as stream:
        for record in json_records:
            stream.write(json.dumps(record, ensure_ascii=False) + "\n")

    validation_fields = [
        "city", "parameter", "value", "query_id", "algorithm",
        "execution_status", "validation_status", "route_records_checked",
        "returned_routes", "requested_k", "all_returned_routes_feasible",
        "all_routes_distinct", "scores_recomputed", "all_recomputed_scores_match",
        "exhaustive_oracle_checked", "record_origin",
    ]
    write_csv(output / "main_study.validation.csv", validation_fields,
              validation_records)

    query_summary = []
    coverage_quality = []
    for record in query_summary_source:
        key = (record["city"], normalized_sweep(record["sweep"]),
               record["param_value"], str(int(float(record["query_no"]))))
        query_id = query_id_by_key[key]
        copied = {"query_id": query_id, **record}
        query_summary.append(copied)
        if record["ratio"].strip():
            coverage_quality.append({
                "query_id": query_id,
                "city": record["city"],
                "parameter": key[1],
                "value": record["param_value"],
                "query_no": record["query_no"],
                "astar_completed": record["astar_completed"],
                "astar_optimal": record["astar_optimal"],
                "astar_best_kc": record["astar_kc"],
                "exact_best_kc": record["exact_kc"],
                "ap_best_kc": record["ap_kc"],
                "ap_astar_ratio": record["ratio"],
                "score_source": "recorded_author_workstation_scores",
            })
    write_csv(output / "route_level.query_summary.csv",
              list(query_summary[0]), query_summary)
    write_csv(output / "coverage_quality.csv",
              list(coverage_quality[0]), coverage_quality)
    write_csv(output / "route_level.group_summary.csv",
              list(group_summary_source[0]), group_summary_source)

    status_counts = Counter((record["algorithm"], record["status"])
                            for record in runtime_records)
    ratios = [float(record["ap_astar_ratio"]) for record in coverage_quality]
    score_matches = sum(record["score_match_query_dwell"] == "1"
                        for record in validation_source)
    summary = {
        "formal_runtime_records": len(runtime_records),
        "main_query_ids": len(main_query_ids),
        "runtime_status_counts": {
            f"{algorithm}:{status}": count
            for (algorithm, status), count in sorted(status_counts.items())
        },
        "route_level_queries": len(queries),
        "route_records": len(route_source),
        "route_records_by_algorithm": dict(Counter(
            normalized_algorithm(record["algorithm"]) for record in route_source)),
        "structural_validation_pass": sum(
            record["structural_validation_pass"] == "1"
            for record in validation_source),
        "score_match_query_dwell": score_matches,
        "score_mismatch_query_dwell": len(validation_source) - score_matches,
        "quality_queries_recorded": len(ratios),
        "quality_ratio_mean_recorded": sum(ratios) / len(ratios),
        "quality_ratio_equal_one_recorded": sum(abs(value - 1.0) <= 1e-12
                                                 for value in ratios),
        "quality_ratio_zero_recorded": sum(abs(value) <= 1e-12 for value in ratios),
        "quality_ratio_between_zero_and_one_recorded": sum(
            0.0 < value < 1.0 for value in ratios),
        "quality_ratio_above_one_recorded": sum(value > 1.0 for value in ratios),
        "bl_route_sequences_available": False,
    }
    (output / "main_study.summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")

    source_paths = [
        args.queries, args.runtime, args.routes, args.route_validation,
        args.query_summary, args.group_summary,
    ]
    for optional in (args.runtime_workbook, args.runtime_summary_workbook):
        if optional:
            source_paths.append(optional)
    def published_path(path):
        path = Path(path)
        if path.name == Path(args.queries).name:
            return "queries/final_queries_1260.csv"
        return f"results/final/source/{path.name}"

    repository_root = Path(__file__).resolve().parents[1]
    implementation_files = [
        repository_root / "src/main.cpp",
        repository_root / "baseline_bl/fixed_query_runner.cpp",
        repository_root / "baseline_bl/bl_time_expanded.hpp",
        repository_root / "baseline_bl/transit_router.hpp",
        repository_root / "scripts/run_experiments.py",
        repository_root / "scripts/verify_results.py",
    ]
    manifest = {
        "record_origin": "author_workstation_formal_rerun",
        "normalization_only": True,
        "normalizer": "scripts/import_author_results.py",
        "normalizer_sha256": digest(Path(__file__)),
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "reported_hardware": {
            "cpu": "Intel Core i7-9700K, 3.60 GHz",
            "memory": "32 GB RAM",
            "operating_system": "Ubuntu 18.04",
        },
        "timeout_seconds": 300,
        "source_files": [{
            "path": published_path(path),
            "sha256": digest(path),
        } for path in source_paths],
        "published_implementation_files": [{
            "path": str(path.relative_to(repository_root)),
            "sha256": digest(path),
        } for path in implementation_files],
        "limitations": [
            "The author runtime workbook contains BL times but not BL route sequences.",
            "Unavailable BL route fields are blank/null and are never interpreted as zero returned routes.",
            "Coverage-quality rows preserve the recorded author-workstation scores.",
        ],
    }
    (output / "main_study.manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")

    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
