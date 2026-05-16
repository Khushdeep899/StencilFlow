#include "stencil.hpp"

#include <cassert>
#include <cstddef>

#include "grid.hpp"

void step(const Grid& in, Grid& out, double c) {
    assert(in.rows() == out.rows());
    assert(in.cols() == out.cols());

    const std::size_t rows = in.rows();
    const std::size_t cols = in.cols();

    // Dirichlet boundary: copy edges verbatim. Doing this first keeps
    // the interior loop branch-free and the edge logic in one place.
    for (std::size_t j = 0; j < cols; ++j) {
        out(0, j) = in(0, j);
        if (rows > 1) out(rows - 1, j) = in(rows - 1, j);
    }
    for (std::size_t i = 0; i < rows; ++i) {
        out(i, 0) = in(i, 0);
        if (cols > 1) out(i, cols - 1) = in(i, cols - 1);
    }

    // Interior cells: 5-point stencil.
    //
    // OpenMP requires the loop condition to be a direct comparison
    // var < limit, so we precompute the upper bound rather than
    // writing i + 1 < rows in the loop header. The ternary keeps the
    // computation unsigned-safe when rows or cols is < 2 (no
    // underflow of rows - 1).
    //
    // No race: each iteration writes to a unique out(i, j) and in is
    // read-only. collapse(2) fuses the i/j loops so OpenMP can
    // balance the work across threads even on tall-and-skinny grids.
    const std::size_t i_end = (rows >= 2) ? rows - 1 : 1;
    const std::size_t j_end = (cols >= 2) ? cols - 1 : 1;
    const double center_weight = 1.0 - 4.0 * c;
    #pragma omp parallel for collapse(2)
    for (std::size_t i = 1; i < i_end; ++i) {
        for (std::size_t j = 1; j < j_end; ++j) {
            out(i, j) = center_weight * in(i, j)
                      + c * (in(i - 1, j) + in(i + 1, j)
                           + in(i, j - 1) + in(i, j + 1));
        }
    }
}
