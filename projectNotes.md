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
8. [GoogleTest in CMake and the first Grid tests (slice 2)](#8-googletest-in-cmake-and-the-first-grid-tests-slice-2)
9. [Indexing operator with const overloads, real TDD (slice 3)](#9-indexing-operator-with-const-overloads-real-tdd-slice-3)
10. [Grid fill and PGM output (slice 4)](#10-grid-fill-and-pgm-output-slice-4)
11. [The 5-point stencil and double-buffering (slice 5)](#11-the-5-point-stencil-and-double-buffering-slice-5)
12. [Serial time loop and initial conditions (slice 6)](#12-serial-time-loop-and-initial-conditions-slice-6)
13. [OpenMP on the stencil (slice 7)](#13-openmp-on-the-stencil-slice-7)

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

## 8. GoogleTest in CMake and the first Grid tests (slice 2)

**TL;DR:** GoogleTest is wired in via **`FetchContent`** (a CMake module
that downloads dependencies at configure time) rather than
`find_package`. Tests live in `tests/` as their own subdirectory.
`gtest_discover_tests` registers each `TEST()` block as its own CTest
entry. Three smoke tests cover the Grid's dimensions and size.

### Files touched

* Root `CMakeLists.txt`: added `FetchContent` for GoogleTest,
  `enable_testing()`, `add_subdirectory(tests)`.
* `tests/CMakeLists.txt`: new file. Defines the `grid_tests` executable,
  includes `src/`, links against `GTest::gtest_main`, calls
  `gtest_discover_tests`.
* `tests/test_grid.cpp`: new file. Three tests.

### The root CMakeLists.txt additions

```cmake
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(tests)
```

### The test file

```cpp
#include <gtest/gtest.h>
#include "grid.hpp"

TEST(GridTest, RecordsDimensions) {
    Grid g(4, 8);
    EXPECT_EQ(g.rows(), 4u);
    EXPECT_EQ(g.cols(), 8u);
}

TEST(GridTest, SizeIsRowsTimesCols) {
    Grid g(4, 8);
    EXPECT_EQ(g.size(), 32u);
}

TEST(GridTest, EmptyGridIsValid) {
    Grid g(0, 0);
    EXPECT_EQ(g.rows(), 0u);
    EXPECT_EQ(g.cols(), 0u);
    EXPECT_EQ(g.size(), 0u);
}
```

### New concepts and why each matters

#### FetchContent (CMake dependency downloader)

**`FetchContent`** is a CMake module that downloads source from GitHub at
configure time and builds it as part of your project.

**Why over `find_package(GTest)`:**

* `find_package` requires the dependency to be installed on the system
  (`brew install googletest`, `apt install libgtest-dev`). **Fragile in
  CI**: a fresh Docker container has no GTest unless an extra install
  step is added.
* `find_package` picks up whatever version is on the system, which can
  differ between developers and break reproducibly. **`FetchContent`
  pins a specific tag** (here, `v1.14.0`).

The price: the first `cmake -B build` after adding `FetchContent` is
slower because the dependency is downloaded and compiled. Subsequent
configures reuse the cache.

#### `enable_testing()` and CTest

`enable_testing()` turns on CTest, CMake's bundled test runner. After
the build, `ctest --test-dir build` runs every test that was registered
with `add_test()` or `gtest_discover_tests()`.

**Why use CTest at all and not just run the test binary directly?**

* `ctest` runs tests in parallel and aggregates pass/fail counts.
* It hides per-test stdout unless `--output-on-failure` is passed, which
  keeps CI logs short.
* It integrates with build systems and CI dashboards.

#### `gtest_discover_tests` vs `add_test`

The old way:

```cmake
add_test(NAME grid_tests COMMAND grid_tests)
```

One CTest entry for the whole test binary. If any one of the three
tests fails, you see "grid_tests failed" with no clue which one.

The new way:

```cmake
include(GoogleTest)
gtest_discover_tests(grid_tests)
```

CMake scans the binary at build time, finds every `TEST()` block, and
registers each as its own CTest entry. The ctest output shows
`GridTest.RecordsDimensions` and `GridTest.EmptyGridIsValid` as
separate lines, each with its own pass/fail status.

#### `add_subdirectory(tests)`

This tells CMake to descend into the `tests/` directory and process
**its** `CMakeLists.txt` as part of the build. **Why split it out:**

* The root `CMakeLists.txt` stays focused on the main `stencilflow`
  executable. Test wiring is its own concern.
* When the project grows (later: `benchmarks/` directory), each gets
  its own `CMakeLists.txt` and `add_subdirectory` call.
* Lyle will recognize this as the standard project layout. A monolithic
  root CMakeLists is a smell.

#### GoogleTest basics

* `TEST(TestSuite, TestName) { ... }` declares a test. **TestSuite** is a
  grouping label (`GridTest` here); **TestName** is the individual
  case (`RecordsDimensions`).
* `EXPECT_EQ(a, b)` asserts `a == b`. On failure, the test fails but
  **continues** executing remaining assertions in the same TEST block.
* `ASSERT_EQ(a, b)` asserts `a == b`. On failure, the test **stops
  immediately**. Use `ASSERT_` for preconditions where continuing makes
  no sense (e.g., a pointer is non-null before dereferencing it).
* **`GTest::gtest_main`** is a target that provides a default
  `main()` function which runs every registered test. We do not write
  our own `main()` in test files.

#### The `u` suffix: `4u`, `8u`, `32u`

`4` is an `int` (signed). `4u` is `unsigned int`. Why care here?

`g.rows()` returns `std::size_t`, which is unsigned. Comparing it to a
signed `int` literal works but emits a signed/unsigned mismatch warning
under `-Wpedantic`. The suffix tells the compiler "this literal is
unsigned" so the comparison is between two unsigned values, no warning.

It is a small but visible habit. Sprinkle `u` on integer literals
whenever you compare against `size_t` or any other unsigned type.

### When this is TDD and when it is not

Strict TDD says: **write the failing test first, then implement code to
make it pass.** This slice is **not** TDD: the Grid class already
existed (from slice 1), and we are retroactively writing tests for it.
The tests passed on first run.

**Slice 3 (the indexing operator) will be TDD:** I will write a failing
test against `grid(i, j)` first, see it fail to compile (no such
operator), then add the operator and watch the test go green. That is
the proper red-green-refactor cycle.

### Possible interview Q&A

* **Q:** Why FetchContent instead of `find_package` for GoogleTest?
* **A:** Reproducibility. `FetchContent` pins a specific GoogleTest
  version (v1.14.0) and downloads it at configure time, so the build
  works on any machine with internet and no separate install step.
  `find_package` depends on whatever version the system happens to have
  installed, which is fragile across developers and CI containers.
* **Q:** Why `gtest_discover_tests` over `add_test`?
* **A:** It registers each `TEST()` block as its own CTest entry, so
  the ctest output shows which specific test failed. Plain `add_test`
  lumps the whole binary into one entry and hides the failing test
  name.
* **Q:** Difference between `EXPECT_EQ` and `ASSERT_EQ`?
* **A:** Both check equality. On failure, `EXPECT_EQ` reports and
  continues running remaining assertions in the same test block;
  `ASSERT_EQ` halts the test immediately. Use `ASSERT_` for
  preconditions where continuing would crash or be meaningless, like
  asserting a pointer is non-null before dereferencing.

---

## 9. Indexing operator with const overloads, real TDD (slice 3)

**TL;DR:** Added `operator()(i, j)` in two flavors. A non-const version
returning `double&` (for write), a const version returning `const double&`
(for read on const Grids). Each does `assert(i < rows_ && j < cols_)` for
debug-time bounds checking, then returns `data_[i * cols_ + j]` to honor
the row-major layout. Followed strict TDD: failing test first, then
implementation.

### The TDD cycle in three observations

1. **Red.** Tests were added that called `g(i, j) = ...` and read it
   back. The build failed with `type 'Grid' does not provide a call
   operator`. The test caught the absence of the feature.
2. **Green.** Two `operator()` overloads were added to grid.hpp. Tests
   passed on the next build.
3. **Refactor.** None needed; the implementation is already as small as
   it can be.

This cycle is the point of TDD: **prove the test detects the missing
feature before the feature is implemented**. If you wrote the operator
first and the test passed on its first run, you would not know whether
the test would have caught a regression.

### The code that landed

```cpp
double& operator()(std::size_t i, std::size_t j) {
    assert(i < rows_ && j < cols_);
    return data_[i * cols_ + j];
}

const double& operator()(std::size_t i, std::size_t j) const {
    assert(i < rows_ && j < cols_);
    return data_[i * cols_ + j];
}
```

### New concepts and why each matters

#### `operator()` is the call operator, repurposed for indexing

`operator()` is the **function call operator**. When you write `g(i, j)`,
the compiler asks: is `g` callable? If `g` is a function, normal call.
If `g` is an object whose class defines `operator()`, that operator is
invoked with `(i, j)`. The same mechanism is what makes **lambdas
callable**: a lambda is a compiler-generated class with `operator()`.

**Why not `operator[]` for 2D?** In C++17, `operator[]` accepts only one
argument: `g[i]`. You cannot write `g[i, j]` (well, you can, but the
comma is treated as the comma operator, not as two arguments). C++23
finally allowed multi-argument `operator[]`, but we are on C++17. So
`operator()` is the conventional choice for `n`-dimensional indexing.

**Alternative:** some libraries use `g[i][j]` by returning a proxy
"row" object from `operator[]` that itself defines `operator[]`. It
works but adds complexity. Eigen, the most popular C++ linear-algebra
library, uses `operator()` for matrix indexing for exactly the reasons
above. Good company.

#### Function overloading on `const`

C++ allows two methods with the same name and same argument types if
they differ only in their **const-ness**:

```cpp
double& operator()(...) { ... }                // chosen when 'this' is non-const
const double& operator()(...) const { ... }    // chosen when 'this' is const
```

The compiler dispatches based on whether the object you call it on is
itself const:

```cpp
Grid g(3, 3);
g(0, 0) = 1.0;          // calls non-const overload, gets writeable ref

const Grid& cg = g;
double x = cg(0, 0);    // calls const overload, gets read-only ref
// cg(0, 0) = 5.0;      // compile error: cannot assign to const double&
```

This is one of the cleanest examples in C++ of "the const overload
exists so the compiler can enforce read-only access on const objects."
**It is also the answer to "why two near-duplicate function bodies"**:
the duplication has a real purpose, and a senior reviewer will see it
as a sign of `const`-correctness discipline.

There is a clever idiom to write the non-const version in terms of the
const one using `const_cast`, but it is a level-up trick. Two clean
bodies is fine.

#### References as return types

`double& operator()` returns a **reference** to the cell, not a copy.
Returning a reference is what makes assignment work:

```cpp
g(1, 2) = 3.14;
```

If `operator()` returned `double` (by value), `g(1, 2)` would yield a
**copy** of the double, and assigning to that copy would be meaningless
(in fact, would not even compile because temporaries are not l-values).
Returning `double&` says "here is the actual cell; do what you want
with it." That is what lets us write to the grid.

**Lifetime caveat:** never return a reference to a local variable. The
reference would dangle as soon as the function returns. In `operator()`
we are returning a reference into `data_`, which lives as long as the
Grid lives, so the reference is safe.

#### `assert` for debug-only bounds checking

```cpp
#include <cassert>
...
assert(i < rows_ && j < cols_);
```

`assert(expr)` is a macro from `<cassert>`:

* **Debug builds** (no `-DNDEBUG`): if `expr` is false, the program
  prints a message and calls `abort()`. The failure message includes
  the file, line, and the failed expression. Excellent for catching
  out-of-bounds indexing during development.
* **Release builds** (with `-DNDEBUG`, which CMake's Release config
  defines): the macro expands to nothing. **Zero runtime cost.**

This is the classic C/C++ pattern: pay for safety while developing,
pay nothing in production. Reservoir simulators do this all over their
hot loops.

**Trade-off:** a stencil kernel running 10 million times per second
cannot afford runtime range checks in production. `assert` lets us
have the checks during development and erases them in Release. If we
wanted *always-on* range checking we would use `if (...) throw
std::out_of_range(...)` or `std::vector::at()` (which throws on
out-of-range).

### What this slice added functionally

The Grid is now a real 2D array you can read from and write to:

```cpp
Grid g(100, 200);
g(10, 20) = 3.14;
double x = g(10, 20);   // x == 3.14
```

Three new tests cover write-then-read round-trips, default-zero state,
and read-from-const access. The serial heat equation step in slice 5
will lean heavily on this operator.

### Possible interview Q&A

* **Q:** Why two `operator()` overloads instead of one?
* **A:** The non-const overload returns `double&` so a writeable Grid
  supports `g(i, j) = x`. The const overload returns `const double&`
  so a `const Grid&` can still be read from. Without the const
  overload, you could not pass a `Grid` by const reference and read
  its cells, which is the natural way to take a function argument
  that only needs to read.
* **Q:** Why `operator()` instead of `operator[]` for 2D indexing?
* **A:** In C++17, `operator[]` takes only one argument, so `g[i, j]`
  does not work. `operator()` accepts any number of arguments, which
  is why Eigen and most linear-algebra libraries use it for matrix
  indexing. C++23 lifted the restriction on `operator[]`.
* **Q:** What does `assert` cost in production?
* **A:** Nothing. With `-DNDEBUG` set (which CMake's Release config
  defines), the `assert` macro expands to nothing and the bounds
  checks compile out completely. The checks help me catch bugs during
  development without slowing the production hot path.
* **Q:** Why return a reference and not a value?
* **A:** Returning a reference is what allows `g(i, j) = 3.14` to
  modify the actual cell. A by-value return would yield a copy, and
  assigning to that copy is either a compile error (because temporaries
  are not l-values) or meaningless (you would be writing to a discarded
  copy). The reference also avoids copying a `double` unnecessarily on
  every read.

---

## 10. Grid fill and PGM output (slice 4)

**TL;DR:** Added a `fill(value)` helper and a `write_pgm(filename)`
method that dumps the grid as a binary PGM image. Auto-normalizes
values from `[min, max]` to `[0, 255]` so any temperature range
becomes visible grayscale. Implementation lives in `src/grid.cpp` so
heavy headers stay out of `grid.hpp`. Shipped ship-first.

### Why split into `grid.hpp` and `grid.cpp` now

The fill is trivial enough to live inline in the header. The PGM
writer needs `<fstream>`. **Putting `<fstream>` in a header would
force every translation unit that includes `grid.hpp` to also parse
`<fstream>`,** which is one of the heaviest standard headers and
dramatically slows compile times.

The idiom: **headers declare the interface; .cpp files implement and
own the heavy includes.** A header should pull in only what its
**public signatures** need.

### PGM format in 30 seconds

```
P5
<cols> <rows>
<maxval>
<raw bytes, one per cell, row-major>
```

* `P5` is the magic header for **binary** PGM. (`P2` is ASCII, larger
  and slower.)
* `<maxval>` is the max gray value, here `255`, so each cell is one
  byte.
* PGM puts **cols before rows** in the header, the opposite of how we
  index `(i, j)`. Burn this in.

We auto-normalize so the brightest cell becomes 255 and the darkest 0.
A simulator can dump any temperature range and still produce a
visible image.

### Topics that landed

* **`std::fill`** to set every element of a range.
* **`std::minmax_element`** returns `(min_iter, max_iter)` in one pass
  over the data.
* **Structured bindings** (`auto [min_it, max_it] = ...`) to unpack
  the pair cleanly. C++17 feature.
* **`std::ofstream` and `std::ios::binary`** for raw byte writes.
* **`static_cast<unsigned char>(...)`**: the modern, type-safe
  alternative to C-style casts. Pinpoints intent ("I am converting a
  double to a byte, on purpose").
* **`std::runtime_error`** thrown on file-open failure. Caller can
  catch; default behavior is `terminate()` with a printed `what()`.
* **`Grid::data()` raw pointer accessor** (two overloads, const and
  non-const). Needed for `MPI_Send` and friends, which take a raw
  pointer plus count.
* **Compile firewall**: header/.cpp split keeps heavy STL headers
  scoped.

### Possible interview Q&A

* **Q:** Why is the PGM output in a .cpp file rather than the header?
* **A:** `<fstream>` is one of the heaviest standard headers. If
  `grid.hpp` included it, every file that uses `Grid` would pay the
  compile-time cost of parsing fstream even if it never writes PGM.
  The .cpp split limits that cost to one translation unit.
* **Q:** Why auto-normalize instead of fixing a temperature range?
* **A:** PGM stores one byte per cell, so we must map our doubles
  into `[0, 255]`. Auto-normalization gives a visible image regardless
  of the simulation's actual range. A real production tool would
  expose a fixed range as a CLI option so animation frames stay on
  the same color scale; that is a polish item, not load-bearing for
  the interview demo.

---

## 11. The 5-point stencil and double-buffering (slice 5)

**TL;DR:** One explicit forward-Euler timestep of the 2D heat
equation. Reads from `in`, writes to `out`. Interior cells use the
5-point Laplacian stencil; boundary cells are copied unchanged
(Dirichlet BC). Three tests prove the math is correct to the bit.

### The math, end to end

The heat equation:

```
du/dt = alpha * (d^2 u/dx^2 + d^2 u/dy^2)
```

Discretize time with **forward Euler**:

```
u(t + dt) = u(t) + dt * du/dt
```

Discretize space with the **5-point Laplacian**:

```
d^2 u/dx^2 + d^2 u/dy^2  ~=
    (u[i-1,j] + u[i+1,j] + u[i,j-1] + u[i,j+1] - 4 * u[i,j]) / dx^2
```

assuming `dx = dy`. Absorb everything into one dimensionless
**diffusion number** `c = alpha * dt / dx^2`:

```
u_new[i,j] = (1 - 4c) * u[i,j]
             + c * (u[i-1,j] + u[i+1,j] + u[i,j-1] + u[i,j+1])
```

Every cell becomes a weighted average of itself and its four
neighbors. **Memorize that single line. It is the physics of the
project.**

### Stability: `c <= 0.25` in 2D

Forward Euler is **conditionally stable**. If `c` exceeds 0.25,
rounding errors blow up exponentially and the simulation explodes
into infinity. We hardcode `c = 0.2` as a safe default.

**Implicit schemes** (used by CMG's real solvers) are
**unconditionally stable** but require solving a linear system every
step (CG, GMRES). Explicitly out of scope here.

### Why double-buffering

Naive in-place:

```cpp
for each (i, j):
    u[i,j] = (1 - 4c)*u[i,j] + c*(u[i-1,j] + u[i+1,j] + u[i,j-1] + u[i,j+1]);
```

By the time you reach cell `(1, 5)`, you have already overwritten
`(0, 5)` and `(1, 4)`. Those updated values then leak into the math
for `(1, 5)`. **You are mixing old and new timestep values in the
same step. Wrong answer.**

Fix: two grids, `in` (read-only this step) and `out` (destination).
After the step, swap them. **The `step()` function signature**:

```cpp
void step(const Grid& in, Grid& out, double c);
```

Note `const Grid&` for input. That is `const`-correctness paying off:
the compiler enforces that `step` cannot accidentally mutate `in`.

### Dirichlet boundary conditions, why and how

A cell on the edge has fewer than 4 neighbors. We pick **Dirichlet**:
the edge values are fixed, copied verbatim from `in` to `out`. Heat
cannot leave the domain through the edges.

Other choices exist (**Neumann** = zero flux through edges,
**absorbing** = heat is removed at edges, **periodic** = the grid
wraps around). Dirichlet is the simplest and fine for our demo.

### New language idioms in this slice

#### Forward declaration `class Grid;`

`stencil.hpp` says:

```cpp
class Grid;          // <-- forward declaration, not a definition
void step(const Grid& in, Grid& out, double c);
```

We do **not** `#include "grid.hpp"` here. Why does this work?

The signatures take `Grid` only by **reference**. A reference is
implementable as a pointer under the hood, and the compiler does not
need to know the size or layout of `Grid` to take its address. Only
that "`Grid` is some type." Forward declaration tells the compiler
exactly that.

**Why bother?** Two reasons:

1. **Faster compilation.** `grid.hpp` includes `<algorithm>`,
   `<cassert>`, `<cstddef>`, `<string>`, `<vector>`. Every file that
   includes `stencil.hpp` would inherit all of that. Forward
   declaration limits the pull-through.
2. **Breaks include cycles.** If A.hpp and B.hpp both need to mention
   each other's types, forward declaration is how you avoid an
   infinite include loop.

The actual `#include "grid.hpp"` lives in `stencil.cpp` because the
implementation calls `in.rows()`, `in(i, j)`, etc., which need the
full class definition. **This is the textbook header/.cpp
separation.**

#### Loop bound idiom `i + 1 < rows`

Compare:

```cpp
for (std::size_t i = 1; i + 1 < rows; ++i) { ... }     // correct
for (std::size_t i = 1; i < rows - 1; ++i) { ... }     // buggy when rows == 0
```

`std::size_t` is **unsigned**. If `rows == 0`, then `rows - 1`
underflows to `SIZE_MAX` (the largest possible size_t), and the loop
runs ~18 quintillion times before crashing or producing garbage.
The `i + 1 < rows` form is safe because there is no subtraction.

**This trap is one of the top bugs in unsigned-loop code.** Memorize
the safe form.

#### `EXPECT_DOUBLE_EQ` vs `EXPECT_EQ`

The stencil tests use `EXPECT_DOUBLE_EQ`. Why?

`EXPECT_EQ(a, b)` for doubles compares with `==`. That is too strict:
`0.1 + 0.2 != 0.3` in IEEE 754. **`EXPECT_DOUBLE_EQ`** instead checks
that two doubles are within **4 ULPs** (units-in-the-last-place) of
each other, which is GoogleTest's standard for "essentially equal."

Our specific tests pass even with `EXPECT_EQ` because we picked values
(`c = 0.2`, exactly representable arithmetic at small grid sizes)
where the math is exact. But it is bad practice to rely on that for
floating point. Use `_DOUBLE_EQ` for any float comparison.

### What this slice added functionally

One full timestep of the heat equation, running on a single CPU
core, single process. No OpenMP yet, no MPI. Pure serial reference.

This `step()` function is what slice 7 will parallelize with OpenMP
(`#pragma omp parallel for collapse(2)`) and what slice 9 will run
in parallel across MPI ranks with halo exchange between them.

### Possible interview Q&A

* **Q:** Why double-buffering instead of in-place update?
* **A:** The 5-point stencil reads four neighbors. If you update the
  grid in place, by the time you reach `(i, j)` you have already
  overwritten its north and west neighbors with new-timestep values,
  so your update mixes old and new data. That is mathematically
  wrong. Two grids (read-only `in`, write `out`) keep the entire
  step in one consistent timestep snapshot. After the step, swap.
* **Q:** Why is `c = 0.25` the stability limit?
* **A:** That is the CFL-like condition for forward-Euler explicit
  time stepping on a 2D 5-point Laplacian. Above 0.25 the
  discrete eigenvalues of the update operator exceed 1 in magnitude,
  so errors grow exponentially every step. The fix is either a
  smaller time step (lower `c`) or an implicit scheme that is
  unconditionally stable.
* **Q:** Why forward-declare `Grid` in `stencil.hpp` instead of
  including `grid.hpp`?
* **A:** The signatures use `Grid` only by reference, so the
  compiler does not need the full class definition. Forward
  declaration cuts compile time (no transitive pull of vector,
  algorithm, etc.) and avoids potential include cycles.
* **Q:** Why use `i + 1 < rows` instead of `i < rows - 1`?
* **A:** `rows` is `size_t`, which is unsigned. If `rows == 0`,
  `rows - 1` underflows to `SIZE_MAX` and the loop runs effectively
  forever. The `i + 1 < rows` form has no subtraction and is safe
  for any `rows`.

---

## 12. Serial time loop and initial conditions (slice 6)

**TL;DR:** `main.cpp` is now a real serial driver. Initial condition
is a hot square at the center of the grid; the loop calls `step()`,
swaps the two buffers, and writes a PGM frame every N steps into
`frames/`. MPI is initialized but only rank 0 does work; the full
decomposition lands in slice 8.

### The loop

```cpp
for (int t = 0; t < steps; ++t) {
    if (t % save_every == 0) {
        write_frame(u, t);
    }
    step(u, u_next, c);
    std::swap(u, u_next);
}
write_frame(u, steps);   // final
```

### Topics that landed

* **`std::swap` on `Grid`.** This is a free function in `<utility>`
  that exchanges two objects. Under the hood it uses **move
  semantics**: it does not copy the underlying vectors, it swaps the
  three pointers each vector holds internally. **Cost is O(1) per
  swap**, no matter how big the grid. This is what makes
  double-buffering practical for large simulations.
* **Anonymous namespace** (`namespace { ... }`). Everything inside
  has **internal linkage**, meaning it is visible only within this
  translation unit. The modern C++ replacement for `static` at file
  scope. Used here for helper functions like `initial_hot_spot` and
  `write_frame` that are not part of any public API.
* **`constexpr double kDiffusionNumber = 0.2;`** A compile-time
  constant. `constexpr` is stricter than `const`: it guarantees the
  value can be computed at compile time, so the compiler can inline
  it everywhere. Naming convention `k` prefix is Google style for
  constants.
* **`std::filesystem::create_directories("frames")`** creates the
  directory if it does not exist, no error if it already does.
  Cross-platform; C++17 standard.
* **`std::atoi`** is the simplest possible string-to-int. It returns
  0 on parse failure with no error reporting. **For a real CLI we
  would use `std::stoi` (throws on failure) or a proper parser like
  CLI11.** Here we trade safety for brevity.

### The "rank 0 only" pattern

```cpp
if (rank == 0) {
    run_serial(...);
}
MPI_Finalize();
```

Common idiom for **single-writer** tasks (reading config, writing
output) inside an MPI program. Other ranks idle until `MPI_Finalize`.
Slice 8 will replace this with actual work distributed across ranks.

### Possible interview Q&A

* **Q:** Why `std::swap(u, u_next)` instead of `u = u_next`?
* **A:** `std::swap` is O(1) move; it exchanges the two vectors'
  internal pointers. Copy assignment would do an O(N) memcpy of every
  cell, which is unacceptable in a hot loop. Swapping also leaves
  `u_next` as a scratch buffer for the next iteration, no allocation
  needed.
* **Q:** Why an anonymous namespace?
* **A:** To give helper functions internal linkage so they do not
  pollute the global symbol table. Replaces the C-era `static` at
  file scope; modern C++ idiom.

---

## 13. OpenMP on the stencil (slice 7)

**TL;DR:** Added one `#pragma omp parallel for collapse(2)` line to
the stencil interior loops. Tests still pass bit-identically; the
solver is **2.76x** faster at 4 threads on Apple Silicon.

### The fork-join model

When the compiler sees `#pragma omp parallel for`, it generates code
that:

1. **Forks** N threads at loop entry (N defaults to physical cores,
   overridable via `OMP_NUM_THREADS`).
2. **Splits** loop iterations across the threads.
3. **Joins** at the closing brace (every thread waits at an implicit
   barrier).

The threads share the parent process's memory. No message passing,
no copies, just multiple stacks reading from and writing into the
same heap.

### MPI vs OpenMP, the canonical contrast

| | MPI | OpenMP |
|---|---|---|
| Unit | Process | Thread |
| Memory | Separate per process | Shared within process |
| Communication | Explicit messages | Shared variables |
| Scope | Across nodes and cores | Within one process |
| Cost | High setup (network) | Low (no network) |

**Hybrid (MPI + OpenMP)** means each MPI rank is itself a
multi-threaded process. MPI handles cross-node parallelism; OpenMP
handles within-node parallelism. That is the architecture this
project demonstrates.

### Why our stencil has no race condition

```cpp
out(i, j) = c1 * in(i, j) + c2 * (in(i-1,j) + in(i+1,j) + in(i,j-1) + in(i,j+1));
```

* `in` is **read-only** in this step. Many threads can read the same
  cell, no problem.
* `out(i, j)` is written by **exactly one thread** for each `(i, j)`,
  because every iteration of the doubled loop corresponds to a unique
  cell.

**No two threads ever target the same `out(i, j)`. No race condition.
No `#pragma omp atomic`, no mutex, no synchronization needed.**

This is **why double-buffering pays off twice**: once for correctness
across timesteps, once for free parallelism within a step. If we did
naive in-place updates, OpenMP would produce non-deterministic
garbage because thread 0 might overwrite a cell that thread 1 was
about to read.

### `collapse(2)` and why we need it

Without `collapse`, only the outer `i` loop is parallelized. A
256x256 grid gives 254 outer iterations to share, fine for 8
threads. But a 16x256 grid gives only 14 outer iterations: two
threads run twice, six run once, no balance.

`collapse(2)` fuses the two loops into one virtual range of
`(rows - 2) * (cols - 2)` iterations. Suddenly even a tall-and-skinny
grid has thousands of iterations to share.

**Cost:** the inner loop iteration variable becomes unavailable for
thread-private bookkeeping, and auto-vectorization gets slightly
harder. For our stencil shape it is the right trade-off.

### The OpenMP loop-form restriction

This C++17 idiom is unsigned-safe and elegant:

```cpp
for (std::size_t i = 1; i + 1 < rows; ++i) { ... }
```

**OpenMP rejects it.** The OpenMP spec requires the loop condition to
be a direct comparison `var op limit` where `op` is `<`, `<=`, `>`,
`>=`, `!=`, and `limit` does not depend on `var`. So we must
**precompute the bound**:

```cpp
const std::size_t i_end = (rows >= 2) ? rows - 1 : 1;
#pragma omp parallel for collapse(2)
for (std::size_t i = 1; i < i_end; ++i) { ... }
```

The ternary keeps it unsigned-safe: if `rows < 2`, `i_end = 1`, the
loop does not execute. **Memorize this gotcha.** It is one of the
top OpenMP newbie traps.

### CMake hint for Apple's libomp

Apple's clang knows the `#pragma omp` syntax but does not ship the
OpenMP runtime. Homebrew installs `libomp` as a **keg-only** formula,
meaning the headers and library are not symlinked into
`/opt/homebrew/include` and `/opt/homebrew/lib`. CMake's `FindOpenMP`
module cannot find them on its own.

The hint:

```cmake
if(APPLE AND EXISTS "/opt/homebrew/opt/libomp")
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include")
    set(OpenMP_CXX_LIB_NAMES "omp")
    set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")
endif()
find_package(OpenMP REQUIRED)
```

* `-Xpreprocessor -fopenmp` tells Apple clang's **preprocessor** to
  recognize `#pragma omp`. Apple stripped `-fopenmp` from the
  compiler-driver layer because it ships no runtime; the
  `-Xpreprocessor` form passes it through anyway.
* The include and library paths point at the keg-only install.

On Linux this entire block is skipped because `APPLE` is false.
Linux's `find_package(OpenMP)` finds GCC's built-in OpenMP support
automatically.

### Observed scaling on Apple Silicon

512x512 grid, 500 steps:

| Threads | Time | Speedup |
| --- | --- | --- |
| 1 | 0.240 s | 1.00x |
| 2 | 0.132 s | **1.82x** |
| 4 | 0.087 s | **2.76x** |
| 8 | 0.089 s | 2.70x (plateau) |

Two stories explain the plateau, **both worth saying out loud**:

1. **Heterogeneous cores.** Apple Silicon has 4 performance cores
   and 4 efficiency cores. Once threads exhaust the perf cores,
   scheduling onto efficiency cores adds work but barely adds
   throughput.
2. **Memory bandwidth saturation.** Stencil codes have low
   arithmetic intensity (5 reads, 1 write per cell, only ~10 FLOPs
   of arithmetic). Once enough threads are reading the grid, the
   memory bus is full and adding more threads cannot help.

This is **the classic stencil-scaling discussion** and exactly the
kind of trade-off CMG cares about.

### Possible interview Q&A

* **Q:** Why does your code have no race condition even though
  multiple threads run the stencil concurrently?
* **A:** Each iteration writes to a unique `out(i, j)`, and `in` is
  read-only this step. No two threads ever target the same memory
  location, so there is no race. The double-buffered design is
  what makes parallelization safe by construction.
* **Q:** What does `collapse(2)` do?
* **A:** It tells OpenMP to treat the two nested loops as one fused
  iteration space of size `(rows - 2) * (cols - 2)`. Without
  collapse, only the outer loop is divided across threads, which
  load-balances poorly on tall-and-skinny grids. With collapse,
  there are always plenty of iterations to share.
* **Q:** Why does the speedup plateau at 4 threads on your Mac?
* **A:** Two reasons. Apple Silicon mixes performance and efficiency
  cores; once we exhaust the 4 perf cores, adding efficiency cores
  doesn't add much. Stencil codes are also memory-bound: low
  arithmetic intensity means we saturate memory bandwidth before
  we saturate compute.
* **Q:** What is the difference between MPI and OpenMP?
* **A:** MPI is multi-process with explicit message passing,
  designed for cross-node parallelism. OpenMP is multi-thread within
  a process, sharing memory, for within-node parallelism. Hybrid
  programs use MPI between nodes and OpenMP inside each rank,
  matching the hardware hierarchy.

---

*Slice 8 next: MPI 1D domain decomposition. Each rank allocates only
its slab. Teach-loop. Then slice 9 wires up `MPI_Sendrecv` halos
which is the hardest single thing in the project.*
