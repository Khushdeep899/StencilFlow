// A regular Cartesian 2D grid storing one double per cell.
//
// Storage is row-major and contiguous in a single std::vector, so a
// cell at (i, j) lives at offset i * cols + j. Indexing, fill, and PGM
// output methods land in later slices.

#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

class Grid {
public:
    Grid(std::size_t rows, std::size_t cols)
        : rows_(rows), cols_(cols), data_(rows * cols, 0.0) {}

    std::size_t rows() const { return rows_; }
    std::size_t cols() const { return cols_; }
    std::size_t size() const { return data_.size(); }

    // Row-major indexing: cell (i, j) sits at offset i * cols + j in the
    // flat data_ buffer. Two overloads so that a non-const Grid can be
    // written to and a const Grid can still be read from.
    double& operator()(std::size_t i, std::size_t j) {
        assert(i < rows_ && j < cols_);
        return data_[i * cols_ + j];
    }

    const double& operator()(std::size_t i, std::size_t j) const {
        assert(i < rows_ && j < cols_);
        return data_[i * cols_ + j];
    }

private:
    std::size_t rows_;
    std::size_t cols_;
    std::vector<double> data_;
};
