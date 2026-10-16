#!/usr/bin/env python3
"""End-to-end check that BL, A*, and AP consume the same fixed query rows."""

import csv
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: test_fixed_query_interface.py ROUTE_SEARCH BL_BINARY")
    route_search = Path(sys.argv[1]).resolve()
    bl_binary = Path(sys.argv[2]).resolve()
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="fixed_query_test_") as directory:
        output = Path(directory) / "unified.jsonl"
        subprocess.run([
            sys.executable, str(root / "scripts/run_experiments.py"),
            "--binary", str(route_search),
            "--bl-binary", str(bl_binary),
            "--dataset", (
                f"Demo:{root / 'examples/toy/stations.csv'}:"
                f"{root / 'examples/toy/trips.csv'}"
            ),
            "--queries", str(root / "examples/toy/queries.csv"),
            "--out", str(output),
            "--timeout", "10",
            "--algorithms", "BL,A,AP",
            "--limit", "2",
            "--time-unit", "seconds",
            "--dwell-description", "query_level_dwell_seconds_from_fixed_csv",
        ], check=True, capture_output=True, text=True)

        with output.with_suffix(".runtime.csv").open(
                newline="", encoding="utf-8") as stream:
            runtime = list(csv.DictReader(stream))
        assert len(runtime) == 6
        assert {row["algorithm"] for row in runtime} == {"BL", "A", "AP"}
        for query_id, expected_dwell in (("toy", "15"), ("dwell", "30")):
            rows = [row for row in runtime if row["query_id"] == query_id]
            assert len(rows) == 3
            assert {row["dwell_seconds"] for row in rows} == {expected_dwell}

        with output.with_suffix(".validation.csv").open(
                newline="", encoding="utf-8") as stream:
            validation = list(csv.DictReader(stream))
        assert len(validation) == 6
        assert all(row["validation_status"] == "passed" for row in validation)
    print("fixed-query BL/A*/AP interface test passed")


if __name__ == "__main__":
    main()
