# Tests subdirectory. The parent CMakeLists.txt is responsible for fetching
# GoogleTest via FetchContent and calling enable_testing(); here we just
# define test executables and register them with CTest.

add_executable(grid_tests
    test_grid.cpp
    ${CMAKE_SOURCE_DIR}/src/grid.cpp
)

# Grid is currently a header-only class, so we only need its include path,
# not a link against a "grid" target. When grid.cpp lands, we will instead
# link against a stencilflow_lib target that bundles the project's
# non-main translation units.
target_include_directories(grid_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# GTest::gtest_main provides a default main() that runs every registered
# test, so we do not write our own. It already depends on GTest::gtest
# (which carries the TEST / EXPECT_EQ macros and the runtime), so we do
# not need to list gtest separately. Listing both produces a harmless
# "duplicate library" warning from Apple's linker.
target_link_libraries(grid_tests PRIVATE
    GTest::gtest_main
)

# gtest_discover_tests scans the binary at build time and registers each
# TEST() block as its own CTest entry. Beats add_test(NAME grid_tests ...)
# because failures show up per-test in ctest output, not lumped into one.
include(GoogleTest)
gtest_discover_tests(grid_tests)
