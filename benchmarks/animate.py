#!/usr/bin/env python3
"""Render the PGM frame sequence in frames/ as a colorized GIF.

Loads every frames/frame_*.pgm in step order, applies the perceptually
uniform 'inferno' colormap (black to purple to red to yellow, which
reads visually as heat), and writes docs/diffusion.gif.

Run from project root:

    python3 benchmarks/animate.py

Output is also embedded in the README.
"""
from __future__ import annotations

import os
import re
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib import animation  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent
FRAMES_DIR = PROJECT_DIR / "frames"
DOCS_DIR = PROJECT_DIR / "docs"


def step_from_name(path: Path) -> int:
    m = re.search(r"frame_(\d+)\.pgm$", path.name)
    return int(m.group(1)) if m else -1


def load_frame(path: Path) -> np.ndarray:
    """P5 PGM -> 2D float array in [0, 1]."""
    with Image.open(path) as img:
        return np.asarray(img, dtype=np.float64) / 255.0


def main() -> int:
    frame_paths = sorted(FRAMES_DIR.glob("frame_*.pgm"), key=step_from_name)
    if not frame_paths:
        print(f"animate: no frames in {FRAMES_DIR}", file=sys.stderr)
        print("animate: run mpirun -n N ./build/stencilflow ... first",
              file=sys.stderr)
        return 1

    # Skip empty / malformed frames just in case.
    frame_paths = [p for p in frame_paths if p.stat().st_size > 0]
    print(f"animate: loading {len(frame_paths)} frames")

    frames = [load_frame(p) for p in frame_paths]
    steps  = [step_from_name(p) for p in frame_paths]

    fig, ax = plt.subplots(figsize=(5, 5), dpi=100)
    ax.set_xticks([])
    ax.set_yticks([])
    im = ax.imshow(frames[0], cmap="inferno", vmin=0.0, vmax=1.0,
                   interpolation="nearest")
    title = ax.set_title(f"step {steps[0]}", fontsize=11)
    fig.tight_layout()

    def update(i):
        im.set_array(frames[i])
        title.set_text(f"step {steps[i]}")
        return [im, title]

    anim = animation.FuncAnimation(
        fig, update, frames=len(frames), interval=200, blit=False
    )

    DOCS_DIR.mkdir(exist_ok=True)
    out = DOCS_DIR / "diffusion.gif"
    anim.save(out, writer=animation.PillowWriter(fps=6))
    print(f"animate: wrote {out}")
    print(f"  frames:   {len(frames)}")
    print(f"  size:     {frames[0].shape[1]}x{frames[0].shape[0]}")
    print(f"  step lo:  {steps[0]}")
    print(f"  step hi:  {steps[-1]}")
    print(f"  bytes:    {out.stat().st_size}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
