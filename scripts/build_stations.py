#!/usr/bin/env python3
"""Associate user-supplied OSM POI snapshot to GTFS stops within 500 m."""
import argparse
import csv
import io
import math
from collections import defaultdict
from pathlib import Path
from zipfile import ZipFile

def distance_m(lat1, lon1, lat2, lon2):
    a1, a2 = math.radians(lat1), math.radians(lat2)
    da, dl = a2-a1, math.radians(lon2-lon1)
    h = math.sin(da/2)**2 + math.cos(a1)*math.cos(a2)*math.sin(dl/2)**2
    return 6371000 * 2 * math.asin(min(1, math.sqrt(h)))

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--feed", required=True)
    p.add_argument("--pois", required=True, help="CSV poi_id,lat,lon,keyword,coverage")
    p.add_argument("--dwell-min", type=float, required=True, help="explicit modeling choice")
    p.add_argument("--radius-m", type=float, default=500)
    p.add_argument("--out", required=True)
    a = p.parse_args()
    if a.dwell_min < 0 or a.radius_m < 0:
        p.error("dwell and radius must be nonnegative")
    with ZipFile(a.feed) as z, z.open("stops.txt") as raw:
        stops = list(csv.DictReader(io.TextIOWrapper(raw, encoding="utf-8-sig", newline="")))
    with open(a.pois, newline="", encoding="utf-8-sig") as f:
        pois = list(csv.DictReader(f))
    for poi in pois:
        poi["lat"] = float(poi["lat"])
        poi["lon"] = float(poi["lon"])
        poi["coverage"] = float(poi["coverage"])
        if not 0 <= poi["coverage"] <= 1:
            raise ValueError("coverage must be in [0,1]")
    # Coarse grid bounds candidate tests without changing the exact 500 m check.
    cell = max(a.radius_m / 111_320, 1e-8)
    grid = defaultdict(list)
    for poi in pois:
        grid[(math.floor(poi["lat"] / cell), math.floor(poi["lon"] / cell))].append(poi)
    assignments = defaultdict(lambda: defaultdict(list))
    for s in stops:
        slat, slon = float(s["stop_lat"]), float(s["stop_lon"])
        lat_bin, lon_bin = math.floor(slat / cell), math.floor(slon / cell)
        lon_span = math.ceil(1 / max(abs(math.cos(math.radians(slat))), 1e-6)) + 1
        for x in range(lat_bin-2, lat_bin+3):
            for y in range(lon_bin-lon_span, lon_bin+lon_span+1):
                for poi in grid[(x, y)]:
                    if distance_m(slat, slon, poi["lat"], poi["lon"]) <= a.radius_m:
                        assignments[s["stop_id"]][poi["keyword"]].append(poi["coverage"])
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    with open(a.out, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["station_id", "dwell_min", "keyword", "coverage"])
        for s in stops:
            sid = s["stop_id"]
            if not assignments[sid]:
                w.writerow([sid, a.dwell_min, "", ""])
            for keyword, values in sorted(assignments[sid].items()):
                w.writerow([sid, a.dwell_min, keyword,
                    1 - math.prod(1 - value for value in values)])
    print(f"wrote {len(stops)} stations to {a.out}")

if __name__ == "__main__":
    main()
