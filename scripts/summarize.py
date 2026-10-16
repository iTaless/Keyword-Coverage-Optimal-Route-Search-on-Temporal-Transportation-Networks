#!/usr/bin/env python3
"""Per-setting aggregate and paired AP/exact quality; timeouts remain censored."""
import argparse
import csv
import json
from collections import defaultdict
from statistics import median

def percentile(values, p):
    values = sorted(values)
    if not values:
        return ""
    if len(values) == 1:
        return values[0]
    position = p * (len(values) - 1)
    lower = int(position)
    upper = min(lower + 1, len(values) - 1)
    fraction = position - lower
    return values[lower] * (1 - fraction) + values[upper] * fraction

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--results", required=True)
    p.add_argument("--out", required=True)
    a = p.parse_args()
    with open(a.results, encoding="utf-8") as f:
        records = [json.loads(line) for line in f]
    pairs = defaultdict(dict)
    groups = defaultdict(list)
    for r in records:
        key = (r["city"], r["parameter"], str(r["value"]), r["algorithm"])
        groups[key].append(r)
        pairs[r["query_id"]][r["algorithm"]] = r
    fields = ["city", "parameter", "value", "algorithm", "n", "ok", "no_route", "timeout", "resource_limit", "error",
        "completed", "archived_runtime_only",
        "median_runtime_sec_completed", "q1_runtime_sec_completed", "q3_runtime_sec_completed",
        "min_runtime_sec_completed", "max_runtime_sec_completed",
        "median_returned", "min_returned", "max_returned", "full_k_rate_completed",
        "mean_ap_exact_score_ratio_positive", "quality_pairs_positive", "zero_exact_pairs",
        "mean_score_gap_vs_exact", "gap_pairs"]
    with open(a.out, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for key, rows in sorted(groups.items()):
            complete = [r for r in rows if r["status"] in ("ok", "no_route", "completed")]
            archived = any(r.get("record_origin") == "author_workstation_formal_rerun" for r in rows)
            ratios, gaps = [], []
            zero = 0
            if key[3] == "AP" and not archived:
                for r in complete:
                    exact = pairs[r["query_id"]].get("A")
                    if exact is None or exact["status"] not in ("ok", "no_route"):
                        continue
                    if not exact["solutions"] or not r["solutions"]:
                        continue
                    x = max(s["score"] for s in exact["solutions"])
                    y = max(s["score"] for s in r["solutions"])
                    gaps.append(x-y)
                    if x > 0:
                        ratios.append(y/x)
                    else:
                        zero += 1
            record = dict(zip(fields[:4], key))
            runtimes = [(r["query_runtime_ms"] if "query_runtime_ms" in r else r["runtime_ms"] - r.get("load_ms", 0)) / 1000
                for r in complete]
            returned = [len(r["solutions"]) for r in complete] if not archived else []
            record.update(n=len(rows), ok=sum(r["status"] == "ok" for r in rows),
                no_route=sum(r["status"] == "no_route" for r in rows),
                timeout=sum(r["status"] == "timeout" for r in rows),
                resource_limit=sum(r["status"] == "resource_limit" for r in rows),
                error=sum(r["status"] == "error" for r in rows),
                completed=len(complete), archived_runtime_only=int(archived),
                median_runtime_sec_completed=median(runtimes) if complete else "",
                q1_runtime_sec_completed=percentile(runtimes, .25),
                q3_runtime_sec_completed=percentile(runtimes, .75),
                min_runtime_sec_completed=min(runtimes) if runtimes else "",
                max_runtime_sec_completed=max(runtimes) if runtimes else "",
                median_returned=median(returned) if returned else "",
                min_returned=min(returned) if returned else "",
                max_returned=max(returned) if returned else "",
                full_k_rate_completed=(sum(len(r["solutions"]) == r["requested_k"] for r in complete)/len(complete)) if complete and not archived else "",
                mean_ap_exact_score_ratio_positive=sum(ratios)/len(ratios) if ratios else "",
                quality_pairs_positive=len(ratios), zero_exact_pairs=zero,
                mean_score_gap_vs_exact=sum(gaps)/len(gaps) if gaps else "", gap_pairs=len(gaps))
            w.writerow(record)
    print(f"wrote {a.out}")

if __name__ == "__main__":
    main()
