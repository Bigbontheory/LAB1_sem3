"""Build timings.png from the three Release runs in timings.csv."""
import csv
from collections import defaultdict
from pathlib import Path
from statistics import median

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

directory = Path(__file__).resolve().parent
samples = defaultdict(list)
with (directory / "timings.csv").open(newline="") as source:
    for row in csv.DictReader(source):
        for operation in ("insert", "lookup", "traverse", "erase"):
            samples[row["container"], int(row["n"]), operation].append(float(row[operation + "_ms"]))

fig, axes = plt.subplots(2, 2, figsize=(10, 7), layout="constrained")
sizes = sorted({size for _, size, _ in samples})
panels = [("insert", "Insertion"), ("lookup", "Successful lookup"),
          ("traverse", "Full traversal"), ("erase", "Removal")]
for ax, (operation, title) in zip(axes.flat, panels):
    for name, marker in [("AVL", "o"), ("Set", "s")]:
        ax.loglog(sizes, [median(samples[name, size, operation]) for size in sizes],
                  marker=marker, label=name)
    ax.set(title=title, xlabel="Number of elements", ylabel="Time for the full dataset (ms)")
    ax.grid(True, which="both", alpha=0.25)
    ax.legend()
fig.suptitle("AVL tree and AVL-backed set · Release build · Median of three runs")
fig.savefig(directory / "timings.png", dpi=160)

