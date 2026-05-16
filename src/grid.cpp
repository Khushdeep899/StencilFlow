#include "grid.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

// PGM (Portable GrayMap) is the simplest possible image format. The P5
// variant is binary:
//
//     P5
//     <cols> <rows>
//     <maxval>
//     <raw bytes, one per cell, row-major>
//
// We pick maxval = 255 so each cell takes exactly one byte. The grid's
// real range can be anything (heat temperatures, etc.), so we scan for
// min and max and linearly map to [0, 255] before writing.

void Grid::write_pgm(const std::string& filename) const {
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Grid::write_pgm: failed to open " + filename);
    }

    out << "P5\n" << cols_ << " " << rows_ << "\n255\n";

    if (data_.empty()) {
        return;
    }

    const auto [min_it, max_it] =
        std::minmax_element(data_.begin(), data_.end());
    const double min_val = *min_it;
    const double range   = *max_it - min_val;

    for (double v : data_) {
        const double normalized = (range > 0.0) ? (v - min_val) / range : 0.0;
        const auto byte = static_cast<unsigned char>(normalized * 255.0);
        out.put(static_cast<char>(byte));
    }
}
