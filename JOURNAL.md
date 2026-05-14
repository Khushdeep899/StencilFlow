# StencilFlow Journal

## 2026-05-13 (Day 1, Wednesday)

### Toolchain
* Found conda-shipped `mpicxx` and `mpirun` were broken on Apple Silicon
  (they try to invoke an x86_64 darwin13 clang that does not exist).
* Installed `open-mpi 5.0.9` and `libomp 22.1.5` via Homebrew.
* Aliased `mpicc`, `mpicxx`, `mpirun`, `mpiexec` to the Homebrew binaries
  in `~/.zshrc` so the broken conda ones cannot win on PATH.
* Verified end to end: a 4-rank Hello MPI binary compiled with
  `mpicxx -std=c++17` runs cleanly under `mpirun -n 4`.

### Scaffolding
* Created the repo layout: `src/`, `tests/`, `benchmarks/`, `docs/`,
  `.github/workflows/`.
* Wrote `CMakeLists.txt` targeting CMake 3.16, C++17, MPI via
  `find_package(MPI REQUIRED)` and link against `MPI::MPI_CXX`.
* Default build type is Release so we never benchmark a debug build by
  accident.
* `src/main.cpp` prints the rank banner; will grow into the time loop
  and CLI parser on Day 2.
