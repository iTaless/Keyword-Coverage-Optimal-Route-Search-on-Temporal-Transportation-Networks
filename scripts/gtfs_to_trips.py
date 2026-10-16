#!/usr/bin/env python3
"""Explicit GTFS service-day projection; exact feed snapshot and costs are caller inputs."""
import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path
from zipfile import ZipFile

def elapsed_seconds(s):
    h, m, sec = map(int, s.split(":"))
    if not (0 <= m < 60 and 0 <= sec < 60 and h >= 0):
        raise ValueError(f"bad GTFS time: {s}")
    # GTFS permits service-day hours above 23. Keep the legacy CSV column
    # names, but emit seconds exactly like every production timetable loader.
    return 3600 * h + 60 * m + sec

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--feed", required=True, help="GTFS zip")
    p.add_argument("--service-id", required=True, help="explicit calendar/service selection")
    p.add_argument("--cost-csv", required=True, help="trip_id,stop_sequence,cost per directed segment")
    p.add_argument("--out", required=True)
    a = p.parse_args()
    with ZipFile(a.feed) as z:
        def read(name):
            with z.open(name) as raw:
                import io
                return list(csv.DictReader(io.TextIOWrapper(raw, encoding="utf-8-sig", newline="")))
        trip_ids = {r["trip_id"] for r in read("trips.txt") if r["service_id"] == a.service_id}
        stops = {r["stop_id"] for r in read("stops.txt")}
        times = defaultdict(list)
        for r in read("stop_times.txt"):
            if r["trip_id"] in trip_ids:
                times[r["trip_id"]].append(r)
    with open(a.cost_csv, newline="", encoding="utf-8-sig") as f:
        fares = {(r["trip_id"], int(r["stop_sequence"])): float(r["cost"])
                 for r in csv.DictReader(f)}
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    count = 0
    with open(a.out, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["edge_id", "from", "to", "departure_min", "arrival_min", "cost"])
        for trip_id in sorted(times):
            rows = sorted(times[trip_id], key=lambda r: int(r["stop_sequence"]))
            for i, (u, v) in enumerate(zip(rows, rows[1:])):
                dep, arr = elapsed_seconds(u["departure_time"]), elapsed_seconds(v["arrival_time"])
                if dep > arr or u["stop_id"] not in stops or v["stop_id"] not in stops:
                    raise ValueError(f"invalid consecutive stop pair in {trip_id}")
                key = (trip_id, int(u["stop_sequence"]))
                if key not in fares or fares[key] < 0 or not math.isfinite(fares[key]):
                    raise ValueError(f"missing or negative segment cost: {key}")
                w.writerow([f"{trip_id}:{i}", u["stop_id"], v["stop_id"], dep, arr, fares[key]])
                count += 1
    print(f"wrote {count} scheduled edges")

if __name__ == "__main__":
    main()
