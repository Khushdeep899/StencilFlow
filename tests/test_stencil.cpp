// Tests for the 5-point heat-equation stencil. The math being tested is:
//
//     out[i,j] = (1 - 4c) * in[i,j]
//                + c * (in[i-1,j] + in[i+1,j] + in[i,j-1] + in[i,j+1])
//
// on interior cells, with edge cells copied verbatim from in to out.

#include <gtest/gtest.h>

#include "grid.hpp"
#include "stencil.hpp"

TEST(StencilTest, UniformGridStaysUniform) {
    Grid in(5, 5);
    Grid out(5, 5);
    in.fill(5.0);

    step(in, out, 0.2);

    for (std::size_t i = 0; i < out.rows(); ++i) {
        for (std::size_t j = 0; j < out.cols(); ++j) {
            EXPECT_DOUBLE_EQ(out(i, j), 5.0);
        }
    }
}

TEST(StencilTest, PointSourceDiffusesExactly) {
    Grid in(5, 5);
    Grid out(5, 5);
    in(2, 2) = 1.0;

    const double c = 0.2;
    step(in, out, c);

    // Center loses 4c, gains nothing back this step.
    EXPECT_DOUBLE_EQ(out(2, 2), 1.0 - 4.0 * c);

    // Four cardinal neighbors gain c each from the source.
    EXPECT_DOUBLE_EQ(out(1, 2), c);
    EXPECT_DOUBLE_EQ(out(3, 2), c);
    EXPECT_DOUBLE_EQ(out(2, 1), c);
    EXPECT_DOUBLE_EQ(out(2, 3), c);

    // Diagonals are untouched (they are not in the 5-point stencil).
    EXPECT_DOUBLE_EQ(out(1, 1), 0.0);
    EXPECT_DOUBLE_EQ(out(3, 3), 0.0);

    // Total heat is conserved: (1 - 4c) + 4 * c == 1.
    double total = 0.0;
    for (std::size_t i = 0; i < out.rows(); ++i)
        for (std::size_t j = 0; j < out.cols(); ++j)
            total += out(i, j);
    EXPECT_DOUBLE_EQ(total, 1.0);
}

TEST(StencilTest, EdgesArePreservedByDirichletBC) {
    Grid in(4, 4);
    Grid out(4, 4);
    in.fill(0.0);
    // Set the entire top row to 100.0 to mimic a hot boundary.
    for (std::size_t j = 0; j < in.cols(); ++j) in(0, j) = 100.0;

    step(in, out, 0.2);

    // Top row should be unchanged (boundary copy).
    for (std::size_t j = 0; j < out.cols(); ++j) {
        EXPECT_DOUBLE_EQ(out(0, j), 100.0);
    }
}
