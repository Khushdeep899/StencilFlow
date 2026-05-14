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

add_executable(stencilflow
    src/main.cpp
)

target_link_libraries(stencilflow PRIVATE MPI::MPI_CXX)

# Status banner printed at configure time so we can spot mismatches
# (e.g. CMake picking up the wrong MPI) before they confuse a build.
message(STATUS "StencilFlow ${PROJECT_VERSION}")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  MPI mpicxx:   ${MPI_CXX_COMPILER}")
message(STATUS "  MPI version:  ${MPI_CXX_VERSION}")
