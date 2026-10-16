#!/usr/bin/env python3
"""Convert the supplied Austin whitespace files to the experiment CSV schema."""
import argparse
import csv
import random
from pathlib import Path


def hms_to_seconds(value: str) -> int:
    raw = int(value)
    hour, minute, second = raw // 10000, (raw // 100) % 100, raw % 100
    if minute >= 60 or second >= 60:
        raise ValueError(f"invalid HHMMSS time: {value}")
    return hour * 3600 + minute * 60 + second


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--edges", required=True)
    parser.add_argument("--labels", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--dwell-seed", type=int, default=20261004)
    parser.add_argument("--edge-prefix", default="edge_",
        help="stable city-specific prefix, e.g. austin_e or houston_e")
    args = parser.parse_args()

    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    label_rows = []
    with open(args.labels, encoding="ascii") as source:
        for line_number, line in enumerate(source, 1):
            values = [int(value) for value in line.split()]
            if len(values) < 2:
                raise ValueError(f"label line {line_number} has no label")
            label_rows.append((values[0], values[1:]))
    label_rows.sort()
    expected = list(range(len(label_rows)))
    if [node for node, _ in label_rows] != expected:
        raise ValueError("label node IDs must be consecutive from zero")

    rng = random.Random(args.dwell_seed)
    dwell_seconds = {node: rng.randint(15, 30) * 60 for node, _ in label_rows}
    with (out / "stations.csv").open("w", newline="", encoding="utf-8") as target:
        writer = csv.writer(target)
        writer.writerow(["station_id", "dwell_min", "keyword", "coverage"])
        for node, labels in label_rows:
            for label in labels:
                # The C++ engine uses one consistent time unit. Here all temporal
                # fields, including dwell_min, are seconds despite the legacy name.
                writer.writerow([node, dwell_seconds[node], label, 1.0])

    edge_count = 0
    with open(args.edges, encoding="ascii") as source, (out / "trips.csv").open(
        "w", newline="", encoding="utf-8"
    ) as target:
        writer = csv.writer(target)
        writer.writerow(["edge_id", "from", "to", "departure_min", "arrival_min", "cost"])
        for edge_count, line in enumerate(source, 1):
            fields = line.split()
            if len(fields) != 5:
                raise ValueError(f"edge line {edge_count} does not have five fields")
            start, end, departure, arrival, weight = fields
            departure_seconds = hms_to_seconds(departure)
            arrival_seconds = hms_to_seconds(arrival)
            if arrival_seconds < departure_seconds:
                raise ValueError(f"edge line {edge_count} arrives before departure")
            writer.writerow(
                [f"{args.edge_prefix}{edge_count:06d}", start, end,
                 departure_seconds, arrival_seconds, weight]
            )

    with (out / "dwell_times.csv").open("w", newline="", encoding="utf-8") as target:
        writer = csv.writer(target)
        writer.writerow(["station_id", "dwell_minutes", "dwell_seconds", "seed"])
        for node in expected:
            writer.writerow([node, dwell_seconds[node] // 60, dwell_seconds[node], args.dwell_seed])
    print(f"converted {len(label_rows)} stations and {edge_count} edge rows to {out}")


if __name__ == "__main__":
    main()
