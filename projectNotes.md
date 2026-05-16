# StencilFlow Project Notes

> Personal study guide for CMG round 2 interview prep. Grows slice by slice
> as the project advances. Read top to bottom, or skim using the table of
> contents. **Bold** = commit to memory.

## How to use this doc

* Each section starts with a one-line **TL;DR**.
* "Possible interview Q&A" at the end of each section is your rehearsal
  list. Try to answer out loud without peeking.
* Code blocks are the exact snippets used in the repo.
* This file is for KD only. The public-facing README is separate and lives
  in `README.md`.

## Table of contents

1. [Day 1: toolchain and the conda vs Homebrew MPI story](#1-day-1-toolchain-and-the-conda-vs-homebrew-mpi-story)
2. [Modern CMake build system](#2-modern-cmake-build-system)
3. [C++17 choice](#3-c17-choice)
4. [MPI fundamentals (from Hello MPI)](#4-mpi-fundamentals-from-hello-mpi)
5. [`MPI_Get_processor_name` and the C-style buffer pattern](#5-mpi_get_processor_name-and-the-c-style-buffer-pattern)
6. [OpenMPI version vs MPI standard version](#6-openmpi-version-vs-mpi-standard-version)
7. [The `Grid` class: scaffolding (slice 1)](#7-the-grid-class-scaffolding-slice-1)

---

## 1. Day 1: toolchain and the conda vs Homebrew MPI story

**TL;DR:** Conda shipped a broken `mpicxx` on Apple Silicon. Fixed by
installing OpenMPI via Homebrew and forcing it onto PATH via zsh aliases,
plus a CMake hint inside the project.

### What was broken

Conda's `mpicxx` at `~/opt/anaconda3/bin/mpicxx` is a shell-script wrapper
that tries to invoke a compiler called
`x86_64-apple-darwin13.4.0-clang++`. That compiler binary does not exist
on this machine because:

* The wrapper was packaged for **old Intel Macs** (`darwin13` is OS X
  Mavericks era, around 2014).
* This Mac is **Apple Silicon (arm64)**, Darwin 25.

So any `mpicxx hello.cpp` call from the conda copy fails immediately:
`command not found: x86_64-apple-darwin13.4.0-clang++`.

### How it was fixed

1. `brew install open-mpi libomp` to get **OpenMPI 5.0.9** and the OpenMP
   runtime, both correctly built for arm64.
2. Aliased `mpicc`, `mpicxx`, `mpirun`, `mpiexec` to the Homebrew paths
   in `~/.zshrc` so future interactive shells cannot accidentally use the
   broken conda copies.
3. Added an Apple-only block inside `CMakeLists.txt`:

   ```cmake
   if(APPLE AND NOT DEFINED MPI_CXX_COMPILER AND EXISTS "/opt/homebrew/bin/mpicxx")
       set(MPI_CXX_COMPILER "/opt/homebrew/bin/mpicxx" CACHE FILEPATH "MPI C++ compiler")
       set(MPI_C_COMPILER   "/opt/homebrew/bin/mpicc"  CACHE FILEPATH "MPI C compiler")
   endif()
   ```

   This pins the MPI compiler for CMake **even when PATH ordering is wrong**,
   for example in a non-interactive build script where aliases do not apply.
   On Linux (CI) the check is skipped.

### Why this is good interview material

It is a textbook "Tell me about a debugging story" answer:

> When I tried to set up the toolchain on Apple Silicon, `mpicxx` failed
> because conda was shipping an x86_64-darwin13 wrapper that pointed at a
> compiler that did not exist on my machine. I diagnosed it by reading
> the wrapper script and noticing the hardcoded target triple, then
> fixed it by installing OpenMPI via Homebrew and pinning the MPI
> compiler in CMake so the build is reproducible regardless of PATH.

### Possible interview Q&A

* **Q:** Why two layers of fix (aliases + CMake hint) instead of one?
* **A:** Aliases solve interactive `mpirun` calls in the terminal. CMake
  hint solves non-interactive builds (CI, scripts) where shell aliases do
  not apply. They cover different ambush vectors.

---

## 2. Modern CMake build system

**TL;DR:** Use `find_package(MPI REQUIRED)` and link the **imported target**
`MPI::MPI_CXX`. No manual `-I` or `-L` flags. This is "modern CMake" and is
the answer to expect at interview.

### The whole CMakeLists.txt in one glance

```cmake
cmake_minimum_required(VERSION 3.16)
project(stencilflow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# Apple-only MPI hint (see section 1)
if(APPLE AND NOT DEFINED MPI_CXX_COMPILER AND EXISTS "/opt/homebrew/bin/mpicxx")
    set(MPI_CXX_COMPILER "/opt/homebrew/bin/mpicxx" CACHE FILEPATH "MPI C++ compiler")
    set(MPI_C_COMPILER   "/opt/homebrew/bin/mpicc"  CACHE FILEPATH "MPI C compiler")
endif()

find_package(MPI REQUIRED)

add_executable(stencilflow src/main.cpp)
target_link_libraries(stencilflow PRIVATE MPI::MPI_CXX)
```

### Why each line matters

| Line | Why |
| --- | --- |
| `cmake_minimum_required(3.16)` | Guarantees modern target-based CMake features. 3.16 is widely available (Ubuntu 20.04+). |
| `set(CMAKE_CXX_STANDARD 17)` + `STANDARD_REQUIRED ON` + `EXTENSIONS OFF` | Force C++17, error out if the compiler cannot do it, disable GNU extensions for portability. |
| Default Release build | **Critical for benchmarking.** Without this, single-config generators default to "no optimization" and your scaling plots will be lies. |
| `-Wall -Wextra -Wpedantic` | Catch nonstandard extensions and most common slips. |
| `find_package(MPI REQUIRED)` | CMake runs the FindMPI module which calls `mpicxx -showme`, parses out includes and libs, packages them into the imported target `MPI::MPI_CXX`. |
| `target_link_libraries(... PRIVATE MPI::MPI_CXX)` | Modern target-based linkage. Propagates include dirs and link libs automatically. |

### The old (bad) way

```cmake
include_directories(${MPI_INCLUDE_PATH})       # 2008-era, directory-scoped globals
link_directories(${MPI_LIBRARY_DIRS})          # ditto
target_link_libraries(stencilflow ${MPI_LIBRARIES})   # no PRIVATE/PUBLIC keyword
```

**Why it is bad:** `include_directories` and `link_directories` apply to
every target in the directory and below. They are global state. They have
the same maintenance problems as global variables in any other language:
unintended cross-target leakage and surprises during refactors.

Modern CMake (3.x+) is **target-scoped**. Each target declares what it
needs. CMake propagates dependencies transitively via the `PRIVATE` /
`PUBLIC` / `INTERFACE` keywords. **This is the answer Lyle will want.**

### Possible interview Q&A

* **Q:** How does your build find MPI?
* **A:** I call `find_package(MPI REQUIRED)`, then link my executable
  against the imported target `MPI::MPI_CXX`. CMake's FindMPI module
  shells out to `mpicxx -showme` to discover include and library paths,
  so I never write `-I` or `-L` flags by hand.
* **Q:** Why default to Release?
* **A:** Single-config generators like Make and Ninja default to a debug
  build with no optimization. If I forgot to set Release I would be
  benchmarking unoptimized code, which would invalidate every scaling
  plot in the README.
* **Q:** What does the `PRIVATE` keyword in `target_link_libraries` do?
* **A:** It means consumers of `stencilflow` do not transitively inherit
  the dependency on `MPI::MPI_CXX`. Since `stencilflow` is an executable
  with no consumers, `PRIVATE` is the right choice. For a library
  exposing MPI types in its public headers, you would use `PUBLIC`.

---

## 3. C++17 choice

**TL;DR:** C++17 gives us `std::filesystem`, `std::optional`, structured
bindings, and inline variables. C++20 requires a newer compiler and we do
not need its features here.

### What C++17 gives us that we will actually use

* **Structured bindings.** `auto [rows, cols] = grid.shape();` for cleaner
  multi-return.
* **`std::optional`.** Useful for "this parse may have failed."
* **`std::filesystem`.** Cross-platform path handling for the PGM frame
  output directory.
* **Inline variables.** Header-only constants without ODR issues.

### Why not C++20 or newer

* Modules and concepts are nice but not load-bearing for this project.
* Requires newer compilers; Ubuntu 20.04 GCC 9 and Apple clang 14 are
  shaky on C++20.
* CMG's interview repo is a **portfolio piece**, not a compiler torture
  test. C++17 signals taste, not novelty hunger.

### Possible interview Q&A

* **Q:** Why C++17 specifically?
* **A:** I wanted modern features that are stable across the compilers
  I care about (Apple clang on Mac, GCC on Linux in CI) without paying
  the C++20 toolchain tax. The features I actually use are structured
  bindings, `std::optional`, and `std::filesystem`.

---

## 4. MPI fundamentals (from Hello MPI)

**TL;DR:** MPI runs N copies of your binary as N separate OS processes,
each with its own memory. They communicate only via MPI calls. Each one
has a numeric **rank** (0 to N-1). The default group is
**`MPI_COMM_WORLD`**.

### The four calls every MPI program needs

```cpp
MPI_Init(&argc, &argv);                       // start MPI runtime
MPI_Comm_rank(MPI_COMM_WORLD, &rank);         // who am I?
MPI_Comm_size(MPI_COMM_WORLD, &size);         // how many of us?
MPI_Finalize();                               // clean shutdown
```

### The five things to lodge in your head

1. **Each rank is a separate OS process with its own memory.** They
   cannot share a pointer. The only way they talk is through MPI calls.
   This is the opposite of `std::thread`, which shares one address space.
2. **`MPI_COMM_WORLD` is a communicator**: a labeled group of processes
   that can address each other by rank. Every program has it for free.
   You can carve sub-communicators; we do not.
3. **Rank is a 0-indexed integer ID** within a communicator. Most
   programs branch on rank: rank 0 is often the I/O coordinator.
4. **`mpirun -n 4 ./prog` launches 4 OS processes.** Each one runs the
   same binary, gets a different rank, and joins `MPI_COMM_WORLD`.
5. **Output ordering is non-deterministic.** Multiple ranks printing
   concurrently can interleave or arrive in any order. There is no
   shared clock and no implicit ordering. That non-determinism is
   why we will need `MPI_Sendrecv` and barriers when ordering actually
   matters.

### Why `MPI_Init` takes `&argc, &argv`

So the runtime can strip MPI-specific args (such as `--mca`) out of your
`argv` before your code sees it. It is a C-era idiom from before
`std::span`. Lives with us forever.

### Possible interview Q&A

* **Q:** What is the difference between MPI and OpenMP?
* **A:** MPI is **multi-process**: each rank is a separate OS process
  with its own memory, and they communicate by passing messages. OpenMP
  is **multi-thread within a single process**: threads share memory and
  synchronize on the same address space. They compose: a hybrid program
  uses MPI across machines or sockets, and OpenMP within each rank.
* **Q:** Why is rank-0 special in many programs?
* **A:** It is a convention, not a language feature. Rank 0 is often the
  process designated to do single-writer tasks like reading config,
  writing the final output, or printing a banner. There is nothing
  technically privileged about it.

---

## 5. `MPI_Get_processor_name` and the C-style buffer pattern

**TL;DR:** Each rank can report which physical host it runs on. The API
follows the **C buffer pattern**: caller allocates the buffer, callee
writes into it.

### The code

```cpp
char hostname[MPI_MAX_PROCESSOR_NAME] = {};
int hostname_len = 0;
MPI_Get_processor_name(hostname, &hostname_len);
```

### The pattern in plain English

1. **You** create a fixed-size buffer (`MPI_MAX_PROCESSOR_NAME` is about
   256).
2. **You** pass the buffer's address to the function.
3. **The function** writes into your buffer and tells you how many
   characters it wrote.

Modern C++ would return a `std::string`. MPI's API was frozen in the
1990s for backward compatibility, so we live with the C idiom.

### Why this is useful

On a single machine all ranks report the same hostname. **On a real
cluster, ranks 0 and 1 might land on `node-04`, ranks 2 and 3 on
`node-05`.** Knowing the rank-to-node mapping matters for:

* Diagnosing performance: ranks on the same node communicate via shared
  memory; ranks on different nodes go over the network.
* Reproducing bugs: "rank 7 hangs but only when it is on node-12" is the
  kind of clue a hostname dump exposes.

### Possible interview Q&A

* **Q:** Why does MPI use C-style buffer-passing instead of just returning
  a string?
* **A:** Backward compatibility with C, which is the language MPI was
  designed for in the early 1990s. The API has been frozen at the
  binary level so existing programs keep linking against new MPI
  implementations.

---

## 6. OpenMPI version vs MPI standard version

**TL;DR:** **OpenMPI 5.0.9** is the implementation (the actual library on
disk). **MPI 3.1** is the version of the MPI standard (the spec) that
this implementation conforms to. Two different version numbers.

### Why this matters

* There are multiple MPI implementations: **OpenMPI**, **MPICH**, **Intel
  MPI**, **MVAPICH**. They all conform to the MPI standard but are
  separate codebases with separate features and version histories.
* CMG might use a different implementation. The standard guarantees your
  source code is portable; the implementation version controls things
  like performance, bug fixes, and non-standard extensions.

### Possible interview Q&A

* **Q:** What MPI version does your code require?
* **A:** It uses only MPI 3.1 features (point-to-point, collectives,
  `MPI_Sendrecv`, processor name). My local implementation is OpenMPI
  5.0.9, but the code should build against any MPI 3.1 conformant
  implementation, including MPICH or Intel MPI.

---

## 7. The `Grid` class: scaffolding (slice 1)

**TL;DR:** A C++ class that owns a flat row-major `std::vector<double>` of
size `rows * cols`. RAII handles cleanup. No indexing or I/O yet.

### The whole file

```cpp
#pragma once

#include <cstddef>
#include <vector>

class Grid {
public:
    Grid(std::size_t rows, std::size_t cols)
        : rows_(rows), cols_(cols), data_(rows * cols, 0.0) {}

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    std::size_t size() const { return data_.size(); }

private:
    std::size_t rows_;
    std::size_t cols_;
    std::vector<double> data_;
};
```

### Design decisions (interview gold)

* **Flat `std::vector<double>` instead of `std::vector<std::vector<double>>`.**
  Three reasons:
  1. **Cache locality.** All `rows * cols` doubles sit in one contiguous
     memory block. Stencil access touches neighboring cells; contiguous
     storage means those neighbors are already in L1 cache.
  2. **One allocation, not `rows` allocations.** Allocation has a cost
     (kernel call, fragmentation, bookkeeping). One big malloc is
     cheaper than many small ones.
  3. **MPI-friendly.** When we ship a slab to another rank, we hand MPI
     **one pointer and one count**. Vector-of-vectors would force a
     loop with per-row send calls or a manual flatten step.
* **Cell (i, j) lives at offset `i * cols + j`.** Row-major. This is the
  C/C++ convention and matches how `T arr[rows][cols]` is laid out in
  memory. (Fortran is column-major, which is one reason CMG's Fortran-
  to-C++ port has to think about layout.)
* **`std::size_t` for dimensions** because dimensions cannot be negative,
  and `std::vector::size()` returns `size_t`, so consistent typing
  avoids signed/unsigned warnings everywhere.

### New language features used and why

#### 1. Member initializer list `: rows_(rows), cols_(cols), data_(rows * cols, 0.0)`

**Definition:** the colon-and-commas thing after the parameter list and
before the body `{}`. It initializes members directly.

**Vs the wrong-feeling alternative:**

```cpp
Grid(std::size_t rows, std::size_t cols) {
    rows_ = rows;                                       // assignment
    cols_ = cols;
    data_ = std::vector<double>(rows * cols, 0.0);
}
```

Both compile. The initializer list is **strictly better**:

* **Required for `const` and reference members.** You can only
  initialize those, never assign.
* **Faster for non-trivial members.** With the body version, `data_` is
  first default-constructed (empty vector), then assigned (new vector
  replaces it). Two operations. The initializer list constructs it
  directly with the right size, in one.
* **It is the idiom.** Every senior C++ engineer expects it.

**Trap:** members are always initialized **in declaration order**, not
in the order they appear in the list. Compilers warn if you reorder.

#### 2. `const` member functions: `std::size_t rows() const`

The trailing `const` is a contract: **this method promises not to modify
the object**. It is the only thing that lets you call the method on a
`const` object:

```cpp
void print_dims(const Grid& g) {
    std::printf("%zu x %zu\n", g.rows(), g.cols());   // OK because const
}
```

**Rule of thumb:** every method that only reads should be `const`. Burn
this in. Senior reviewers always flag a missing `const` on a getter.

#### 3. RAII in concrete form

The `Grid` object owns a `std::vector<double>`. When a `Grid` goes out of
scope, the compiler-generated destructor runs, which destroys all
members. `std::vector`'s destructor frees its heap memory.

```cpp
{
    Grid g(1000, 1000);  // allocates 8 MB
    // ... use g ...
}  // <- here, vector frees the 8 MB automatically. No delete needed.
```

The pattern: **resource lifetime = object lifetime = scope lifetime**.
Acquire on construction, release on destruction. The C version would
require `malloc` + `free` and a careful programmer. The C++ version is
free of leaks **by construction**.

This is **the single most important C++ idiom** for the interview. If
you can explain RAII cleanly, you have demonstrated you understand
modern C++.

#### 4. `std::size_t` vs `int`

`std::size_t` is an **unsigned** integer type wide enough to hold the
size of any object. On a 64-bit Mac it is 64 bits.

* `std::vector::size()` returns `size_t`.
* Sizes cannot be negative; using a signed type misrepresents intent.
* Avoids signed/unsigned compare warnings in `for (... ; i < grid.rows(); ...)`.

#### 5. `#pragma once`

Replaces the traditional include-guard boilerplate:

```cpp
// old:
#ifndef GRID_HPP
#define GRID_HPP
// ... contents ...
#endif

// new:
#pragma once
// ... contents ...
```

`#pragma once` is a compiler directive supported by every major modern
compiler. It is simpler, shorter, and immune to typos in the macro name.

#### 6. Trailing underscore on `rows_`, `cols_`, `data_`

Convention only; the compiler does not care. It disambiguates the member
from the constructor parameter so `rows_(rows)` reads cleanly. Google's
style guide uses it; many large codebases do too.

### What this slice actually added

A class you can construct and ask its size. That is all:

```cpp
Grid g(100, 200);
g.rows();   // -> 100
g.cols();   // -> 200
g.size();   // -> 20000
```

No cell access, no I/O, no math. Those land in slices 2 and beyond.

### Possible interview Q&A

* **Q:** Why flat storage instead of vector-of-vectors?
* **A:** Three reasons. Cache locality (stencil neighbors live next to
  each other in memory), one allocation instead of N, and MPI-friendly
  contiguous layout so I can send the whole slab as a single
  pointer-plus-count.
* **Q:** Why an initializer list in the constructor?
* **A:** Required for `const` or reference members, strictly faster for
  non-trivial members because it skips the default-construct then assign
  dance, and it is the idiomatic C++ form.
* **Q:** What is RAII?
* **A:** Resource Acquisition Is Initialization. The lifetime of a
  resource is tied to the lifetime of an object. The constructor
  acquires (here, the vector allocates memory), the destructor releases
  (the vector frees it). Because destructors run automatically when
  objects go out of scope, you never leak resources by forgetting a
  cleanup call.
* **Q:** Why row-major?
* **A:** It matches C/C++ array layout, where `T arr[rows][cols]` stores
  row 0 first, then row 1, in memory. Fortran is column-major, which is
  worth knowing because CMG is converting Fortran simulators to C++ and
  has to make a layout decision in every kernel.

---

*This doc grows as the project does. Next entry will cover GoogleTest
integration into CMake and the first three Grid tests.*
