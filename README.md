# StencilFlow

A hybrid MPI + OpenMP solver for the 2D heat equation in modern C++17.

> Work in progress. This README will be expanded with build instructions,
> usage, design rationale, and scaling results once the solver is complete.

## What it is

StencilFlow integrates the 2D heat equation

```
du/dt = alpha * (d^2 u/dx^2 + d^2 u/dy^2)
```

on a regular Cartesian grid using an explicit forward-Euler time step and a
five-point stencil in space. The domain is decomposed across MPI ranks in
horizontal strips; within each rank, OpenMP parallelizes the per-cell update
loop. Output frames are written as portable graymap (PGM) images.

## Status

| Day | Milestone |
| --- | --- |
| 1 | Toolchain, scaffolding, Hello MPI |
| 2 | Serial solver, OpenMP, MPI scaffolding |
| 3 | Halo exchange, hybrid validation, benchmarks, plots, Docker, CI |

See [JOURNAL.md](JOURNAL.md) for daily notes.

## License

MIT, see [LICENSE](LICENSE).
