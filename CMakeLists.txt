cmake_minimum_required(VERSION 3.16)

project(stencilflow
    VERSION 0.1.0
    DESCRIPTION "Hybrid MPI + OpenMP 2D heat equation solver"
    LANGUAGES CXX)

# Enforce C++17 without compiler-specific extensions.
# C++17 gives us structured bindings, std::optional, std::filesystem, and
# inline variables, all of which we will use. C++20 adds modules and concepts
# but would push the minimum compiler requirement higher than we need.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Release if the caller did not pick a build type.
# Without this, single-config generators (Make, Ninja) default to "no
# optimization" which would mislead our scaling benchmarks on Day 3.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Warning flags we want on every translation unit. -Wall -Wextra catches
# most of what an undergrad C++ course glosses over; -Wpedantic flags
# nonstandard extensions so we stay portable.
add_compile_options(-Wall -Wextra -Wpedantic)

# On macOS, conda may also ship an MPI on PATH whose mpicxx wrapper is
# broken (it invokes an x86_64 darwin13 clang that does not exist on
# Apple Silicon). Hint find_package to use the Homebrew MPI when it is
# present so fresh CMake configures do not pick up the conda copy. The
# checks are scoped to Apple, so Linux CI is unaffected.
if(APPLE AND NOT DEFINED MPI_CXX_COMPILER AND EXISTS "/opt/homebrew/bin/mpicxx")
    set(MPI_CXX_COMPILER "/opt/homebrew/bin/mpicxx" CACHE FILEPATH "MPI C++ compiler")
    set(MPI_C_COMPILER   "/opt/homebrew/bin/mpicc"  CACHE FILEPATH "MPI C compiler")
endif()

find_package(MPI REQUIRED)

# OpenMP. Apple clang does not ship libomp; it lives in Homebrew as a
# keg-only formula. Hint FindOpenMP at /opt/homebrew/opt/libomp before
# calling find_package so the search succeeds without manual env vars.
if(APPLE AND EXISTS "/opt/homebrew/opt/libomp")
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include")
    set(OpenMP_CXX_LIB_NAMES "omp")
    set(OpenMP_omp_LIBRARY "/opt/homebrew/opt/libomp/lib/libomp.dylib")
endif()
find_package(OpenMP REQUIRED)

add_executable(stencilflow
    src/main.cpp
    src/grid.cpp
    src/stencil.cpp
    src/decomp.cpp
    src/halo.cpp
)

target_link_libraries(stencilflow PRIVATE MPI::MPI_CXX OpenMP::OpenMP_CXX)

# Tests. FetchContent downloads a pinned GoogleTest at configure time so
# we never depend on a system-installed gtest, which makes CI Docker
# builds reproducible. enable_testing() turns on the CTest target so
# `ctest --test-dir build` works after the build.
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# Stop GoogleTest from clobbering our compile options on Windows builds.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(tests)

# Status banner printed at configure time so we can spot mismatches
# (e.g. CMake picking up the wrong MPI) before they confuse a build.
message(STATUS "StencilFlow ${PROJECT_VERSION}")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  MPI mpicxx:   ${MPI_CXX_COMPILER}")
message(STATUS "  MPI version:  ${MPI_CXX_VERSION}")
