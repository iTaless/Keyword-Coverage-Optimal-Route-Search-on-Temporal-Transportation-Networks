#!/usr/bin/env python3
"""Small random instances checked against an independent exhaustive enumerator."""
import csv
import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from verify_results import data, verify

def write(path, header, rows):
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)

def main(binary):
    rng = random.Random(83726)
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        st, tr, qu = (root / name for name in ("stations.csv", "trips.csv", "queries.csv"))
        for trial in range(45):
            names = ["S", "A", "B", "T"]
            dwell = {v: rng.randrange(0, 9) for v in names}
            station_rows = [(v, dwell[v], key, rng.choice([0, .25, .6, 1]))
                for v in names for key in ("x", "y", "z")]
            edges = []
            for i in range(9):
                u, v = rng.sample(names, 2)
                dep = rng.randint(0, 8)
                arr = dep + rng.randint(0, 4)
                edges.append((f"e{i:02d}", u, v, dep, arr, rng.randint(0, 3)))
            write(st, ["station_id", "dwell_min", "keyword", "coverage"], station_rows)
            write(tr, ["edge_id", "from", "to", "departure_min", "arrival_min", "cost"], edges)
            write(qu, ["query_id", "city", "parameter", "value", "source", "target", "start_min",
                "end_min", "budget", "keywords", "weights", "k", "seed"],
                [("q", "Tiny", "manual", trial, "S", "T", 0, 12, rng.randint(2, 10),
                  "x|y|z", "0.2|0.3|0.5", rng.randint(1, 8), 123)])
            cached = data(st, tr, qu)
            for alg in ("A", "BL_enum", "AP"):
                proc = subprocess.run([binary, "--stations", str(st), "--trips", str(tr),
                    "--queries", str(qu), "--query-id", "q", "--algorithm", alg,
                    "--rho", "3", "--rounds", "4", "--runs", "5"],
                    capture_output=True, text=True)
                if proc.returncode:
                    raise AssertionError(f"trial {trial} {alg}: {proc.stderr}; station_header={st.read_text()[:110]!r}; edges={edges}")
                result = json.loads(proc.stdout)
                verify(result, *cached, exhaustive=True)
    print("45 randomized instances: exact/BL_enum agree with independent oracle; AP feasible")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build/route_search")
