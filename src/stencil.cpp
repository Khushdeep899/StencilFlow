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

    // Interior cells: 5-point stencil. Loop bounds use i + 1 < rows
    // rather than i < rows - 1 so the comparison is safe when rows is
    // zero or one (unsigned subtraction would underflow).
    const double center_weight = 1.0 - 4.0 * c;
    for (std::size_t i = 1; i + 1 < rows; ++i) {
        for (std::size_t j = 1; j + 1 < cols; ++j) {
            out(i, j) = center_weight * in(i, j)
                      + c * (in(i - 1, j) + in(i + 1, j)
                           + in(i, j - 1) + in(i, j + 1));
        }
    }
}
