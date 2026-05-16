// Tests for the Grid class. GoogleTest is wired in through CMake's
// FetchContent; see tests/CMakeLists.txt. Run with:
//
//     cmake --build build && ctest --test-dir build --output-on-failure

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
