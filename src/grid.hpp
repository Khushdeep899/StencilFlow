// A regular Cartesian 2D grid storing one double per cell.
//
// Storage is row-major and contiguous in a single std::vector, so a
// cell at (i, j) lives at offset i * cols + j. Indexing, fill, and PGM
// output methods land in later slices.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <string>
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

    // Set every cell to value.
    void fill(double value) {
        std::fill(data_.begin(), data_.end(), value);
    }

    // Raw pointer access for MPI sends (slice 9) and bulk fills.
    double*       data()       { return data_.data(); }
    const double* data() const { return data_.data(); }

    // Write the grid as a P5 (binary) PGM image.
    //
    // Default behavior (scale_max <= scale_min) auto-normalizes the
    // grid's own [min, max] range to [0, 255], which is right for a
    // single snapshot but wrong across an animation because each
    // frame uses a different absolute scale.
    //
    // For animations, pass a fixed scale (e.g. 0.0, 100.0) so every
    // frame in the sequence shares the same color mapping. Values
    // below scale_min clamp to 0; above scale_max clamp to 255.
    void write_pgm(const std::string& filename,
                   double scale_min = 0.0,
                   double scale_max = -1.0) const;

private:
    std::size_t rows_;
    std::size_t cols_;
    std::vector<double> data_;
};
