// Tests for the Grid class. GoogleTest is wired in through CMake's
// FetchContent; see tests/CMakeLists.txt. Run with:
//
//     cmake --build build && ctest --test-dir build --output-on-failure

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

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

TEST(GridTest, FillSetsAllCells) {
    Grid g(3, 4);
    g.fill(7.5);
    for (std::size_t i = 0; i < g.rows(); ++i) {
        for (std::size_t j = 0; j < g.cols(); ++j) {
            EXPECT_EQ(g(i, j), 7.5);
        }
    }
}

TEST(GridTest, WritePgmHasValidHeaderAndSize) {
    Grid g(4, 5);
    g(0, 0) = 0.0;
    g(3, 4) = 1.0;   // make sure normalization has a real range to work with
    const std::string filename = "test_output.pgm";
    g.write_pgm(filename);

    std::ifstream in(filename, std::ios::binary);
    ASSERT_TRUE(in.good()) << "PGM file did not open";

    std::string magic;
    int cols = 0;
    int rows = 0;
    int maxval = 0;
    in >> magic >> cols >> rows >> maxval;

    EXPECT_EQ(magic, "P5");
    EXPECT_EQ(cols, 5);          // PGM is cols (width) before rows (height)
    EXPECT_EQ(rows, 4);
    EXPECT_EQ(maxval, 255);

    in.get();                    // skip the single whitespace after maxval
    const std::streampos header_end = in.tellg();
    in.seekg(0, std::ios::end);
    const std::streampos file_end = in.tellg();
    EXPECT_EQ(static_cast<std::size_t>(file_end - header_end), 4u * 5u);

    in.close();
    std::remove(filename.c_str());
}
