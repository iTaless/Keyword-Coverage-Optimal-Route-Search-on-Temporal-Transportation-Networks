#!/usr/bin/env python3
"""Two city panels per row, 300 s timeout symbols, no invented timeout runtimes."""
import argparse
import csv
from collections import defaultdict
from pathlib import Path

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--summary", required=True)
    p.add_argument("--out-dir", required=True)
    a = p.parse_args()
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    with open(a.summary, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    cities = sorted({r["city"] for r in rows})
    factors = ["keywords", "budget", "window", "k"]
    colors = {"A": "#2563eb", "BL": "#e07b22", "AP": "#138f73"}
    out = Path(a.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for factor in factors:
        panels = max(1, len(cities))
        fig, axes = plt.subplots((panels+1)//2, 2, figsize=(10.2, 3.6*((panels+1)//2)), squeeze=False,
            constrained_layout=True)
        for index, city in enumerate(cities):
            ax = axes[index//2][index%2]
            subset = [r for r in rows if r["city"] == city and r["parameter"] == factor]
            xs = sorted({float(r["value"]) for r in subset})
            for alg in ("BL", "A", "AP"):
                by_x = {float(r["value"]): r for r in subset if r["algorithm"] == alg}
                yy = [float(by_x[x]["median_runtime_sec_completed"]) if x in by_x and by_x[x]["median_runtime_sec_completed"] else float("nan") for x in xs]
                lower = [yy[i] - float(by_x[x]["q1_runtime_sec_completed"])
                    if x in by_x and by_x[x]["q1_runtime_sec_completed"] else 0 for i, x in enumerate(xs)]
                upper = [float(by_x[x]["q3_runtime_sec_completed"]) - yy[i]
                    if x in by_x and by_x[x]["q3_runtime_sec_completed"] else 0 for i, x in enumerate(xs)]
                ax.errorbar(xs, yy, yerr=[lower, upper], fmt="o-", capsize=3,
                    color=colors[alg], label=alg, linewidth=1.5)
                for x in xs:
                    if x in by_x and int(by_x[x]["timeout"]):
                        ax.scatter([x], [300], marker="x", color=colors[alg], s=30)
                        ax.annotate(str(by_x[x]["timeout"]), (x,300), xytext=(3,3),
                            textcoords="offset points", fontsize=7)
            ax.axhline(300, color="#777", linewidth=.7, linestyle="--")
            ax.set(title=city, xlabel=factor, ylabel="Median completed runtime (s)", xticks=xs)
            ax.grid(axis="y", alpha=.2)
            ax.legend(fontsize=8)
        for index in range(len(cities), len(axes.flat)):
            axes.flat[index].axis("off")
        fig.savefig(out/f"runtime_{factor}.pdf")
        fig.savefig(out/f"runtime_{factor}.png", dpi=180)
        plt.close(fig)
    print(f"saved runtime panels to {out}; crosses at 300 s show censored counts")

if __name__ == "__main__":
    main()
