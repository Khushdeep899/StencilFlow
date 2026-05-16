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

TEST(GridTest, AllCellsZeroByDefault) {
    Grid g(3, 5);
    for (std::size_t i = 0; i < g.rows(); ++i) {
        for (std::size_t j = 0; j < g.cols(); ++j) {
            EXPECT_EQ(g(i, j), 0.0);
        }
    }
}

TEST(GridTest, WriteThenReadRoundTrips) {
    Grid g(3, 5);
    g(1, 2) = 3.14;
    g(2, 4) = -7.5;
    EXPECT_EQ(g(1, 2), 3.14);
    EXPECT_EQ(g(2, 4), -7.5);
    EXPECT_EQ(g(0, 0), 0.0);  // untouched cells stay zero
}

TEST(GridTest, ConstGridIsReadable) {
    Grid g(2, 2);
    g(0, 0) = 9.0;
    const Grid& cg = g;          // const reference to the same object
    EXPECT_EQ(cg(0, 0), 9.0);    // only compiles if a const overload exists
}
