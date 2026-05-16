#!/usr/bin/env python3
"""Plot strong and weak scaling from benchmarks/results.csv.

Outputs docs/strong_scaling.png and docs/weak_scaling.png.

Strong scaling:
  * x: MPI ranks
  * y: speedup = T1 / Tn
  * ideal line: y = x

Weak scaling:
  * x: MPI ranks
  * y: efficiency = T1 / Tn (problem grows linearly with ranks)
  * ideal line: y = 1.0

Run from project root: python3 benchmarks/plot.py
"""
from __future__ import annotations

import csv
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR)
CSV_PATH = os.path.join(SCRIPT_DIR, "results.csv")
DOCS_DIR = os.path.join(PROJECT_DIR, "docs")


def load_rows() -> list[dict]:
    with open(CSV_PATH, newline="") as f:
        reader = csv.DictReader(f)
        return [
            {
                "mode": r["mode"],
                "ranks": int(r["ranks"]),
                "rows": int(r["rows"]),
                "cols": int(r["cols"]),
                "steps": int(r["steps"]),
                "elapsed_s": float(r["elapsed_s"]),
            }
            for r in reader
        ]


def plot_strong(rows: list[dict]) -> None:
    strong = sorted([r for r in rows if r["mode"] == "strong"], key=lambda r: r["ranks"])
    if not strong:
        return
    baseline = strong[0]["elapsed_s"]
    ranks = [r["ranks"] for r in strong]
    speedup = [baseline / r["elapsed_s"] for r in strong]

    fig, ax = plt.subplots(figsize=(7, 5))
    ax.plot(ranks, speedup, "o-", linewidth=2, markersize=8, label="StencilFlow")
    ax.plot(ranks, ranks, "k--", linewidth=1, alpha=0.6, label="ideal (linear)")
    ax.set_xlabel("MPI ranks")
    ax.set_ylabel("speedup (T1 / Tn)")
    ax.set_title(
        f"Strong scaling: {strong[0]['rows']}x{strong[0]['cols']} grid, "
        f"{strong[0]['steps']} steps"
    )
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_xticks(ranks)
    out = os.path.join(DOCS_DIR, "strong_scaling.png")
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    print(f"wrote {out}")


def plot_weak(rows: list[dict]) -> None:
    weak = sorted([r for r in rows if r["mode"] == "weak"], key=lambda r: r["ranks"])
    if not weak:
        return
    baseline = weak[0]["elapsed_s"]
    ranks = [r["ranks"] for r in weak]
    # Efficiency: how close is Tn to T1? Ideal is constant 1.0.
    # Note: in our convention this is just T1/Tn since work per rank is constant.
    efficiency = [baseline / r["elapsed_s"] for r in weak]

    fig, ax = plt.subplots(figsize=(7, 5))
    ax.plot(ranks, efficiency, "o-", linewidth=2, markersize=8, label="StencilFlow")
    ax.axhline(1.0, color="k", linestyle="--", linewidth=1, alpha=0.6, label="ideal (1.0)")
    ax.set_xlabel("MPI ranks")
    ax.set_ylabel("efficiency (T1 / Tn)")
    ax.set_title(
        f"Weak scaling: {weak[0]['rows'] // weak[0]['ranks']} rows/rank x "
        f"{weak[0]['cols']} cols, {weak[0]['steps']} steps"
    )
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_xticks(ranks)
    ax.set_ylim(0, 1.2)
    out = os.path.join(DOCS_DIR, "weak_scaling.png")
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    print(f"wrote {out}")


def main() -> int:
    if not os.path.exists(CSV_PATH):
        print(f"plot: {CSV_PATH} not found, run benchmarks/run.sh first",
              file=sys.stderr)
        return 1
    os.makedirs(DOCS_DIR, exist_ok=True)
    rows = load_rows()
    plot_strong(rows)
    plot_weak(rows)
    return 0


if __name__ == "__main__":
    sys.exit(main())
