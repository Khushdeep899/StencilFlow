// Tests for the 1D MPI domain decomposition.
//
// These run as a single-process unit test (no mpirun) because
// decompose() is pure arithmetic; it does not call any MPI function,
// it only refers to the MPI_PROC_NULL constant for the neighbor
// sentinel.

#include <gtest/gtest.h>

#include "decomp.hpp"

TEST(DecompTest, EvenSplitFourRanks) {
    auto d0 = decompose(100, 0, 4);
    auto d1 = decompose(100, 1, 4);
    auto d2 = decompose(100, 2, 4);
    auto d3 = decompose(100, 3, 4);

    EXPECT_EQ(d0.start_row, 0u);   EXPECT_EQ(d0.num_rows, 25u);
    EXPECT_EQ(d1.start_row, 25u);  EXPECT_EQ(d1.num_rows, 25u);
    EXPECT_EQ(d2.start_row, 50u);  EXPECT_EQ(d2.num_rows, 25u);
    EXPECT_EQ(d3.start_row, 75u);  EXPECT_EQ(d3.num_rows, 25u);
}

TEST(DecompTest, UnevenSplitSpreadsRemainder) {
    // 10 rows across 3 ranks: 10 = 3*3 + 1, so the first rank gets
    // an extra row and the others get the base.
    auto d0 = decompose(10, 0, 3);
    auto d1 = decompose(10, 1, 3);
    auto d2 = decompose(10, 2, 3);

    EXPECT_EQ(d0.start_row, 0u);  EXPECT_EQ(d0.num_rows, 4u);
    EXPECT_EQ(d1.start_row, 4u);  EXPECT_EQ(d1.num_rows, 3u);
    EXPECT_EQ(d2.start_row, 7u);  EXPECT_EQ(d2.num_rows, 3u);

    // Owned rows cover [0, 10) with no overlap or gap.
    EXPECT_EQ(d0.num_rows + d1.num_rows + d2.num_rows, 10u);
}

TEST(DecompTest, EdgeRanksHaveOneFewerNeighbor) {
    auto d0    = decompose(100, 0, 4);    // first rank
    auto last  = decompose(100, 3, 4);    // last rank
    auto inner = decompose(100, 1, 4);    // middle rank

    EXPECT_FALSE(d0.has_north);  EXPECT_TRUE(d0.has_south);
    EXPECT_EQ(d0.north_rank, MPI_PROC_NULL);
    EXPECT_EQ(d0.south_rank, 1);

    EXPECT_TRUE(last.has_north); EXPECT_FALSE(last.has_south);
    EXPECT_EQ(last.north_rank, 2);
    EXPECT_EQ(last.south_rank, MPI_PROC_NULL);

    EXPECT_TRUE(inner.has_north); EXPECT_TRUE(inner.has_south);
}

TEST(DecompTest, LocalBufferIncludesHalos) {
    auto d0 = decompose(100, 0, 4);
    auto d1 = decompose(100, 1, 4);
    auto d3 = decompose(100, 3, 4);

    // Edge rank 0: 25 owned + 1 south halo = 26 local rows.
    EXPECT_EQ(d0.local_rows_with_halo(), 26u);
    EXPECT_EQ(d0.local_owned_start(), 0u);  // owned starts at local row 0

    // Inner rank 1: 25 owned + 2 halos = 27 local rows.
    EXPECT_EQ(d1.local_rows_with_halo(), 27u);
    EXPECT_EQ(d1.local_owned_start(), 1u);  // top halo is local row 0

    // Edge rank 3: 25 owned + 1 north halo = 26 local rows.
    EXPECT_EQ(d3.local_rows_with_halo(), 26u);
    EXPECT_EQ(d3.local_owned_start(), 1u);
}

TEST(DecompTest, SingleRankOwnsEverything) {
    auto d = decompose(100, 0, 1);
    EXPECT_EQ(d.start_row, 0u);
    EXPECT_EQ(d.num_rows, 100u);
    EXPECT_FALSE(d.has_north);
    EXPECT_FALSE(d.has_south);
    EXPECT_EQ(d.local_rows_with_halo(), 100u);
}
