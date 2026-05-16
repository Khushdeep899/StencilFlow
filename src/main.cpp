// StencilFlow: hybrid MPI + OpenMP 2D heat equation solver.
//
// Slice 8: each rank allocates only its horizontal strip of the
// global grid (plus one halo row above and below if a neighbor
// exists in that direction). The time loop runs step() on each
// rank's local buffer. Halos are still zero this slice; slice 9
// will fill them via MPI_Sendrecv each timestep.

#include <mpi.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <utility>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "decomp.hpp"
#include "grid.hpp"
#include "halo.hpp"
#include "stencil.hpp"

namespace {

constexpr double kDiffusionNumber = 0.2;

// Initialize the local grid so the global hot spot is reproduced on
// the owned portion of each rank. Halo rows are left at zero; they
// will be overwritten by MPI exchange in slice 9.
void initial_hot_spot_local(Grid& u_local,
                            const Decomposition& d,
                            std::size_t total_rows,
                            std::size_t total_cols) {
    const std::size_t cx     = total_rows / 2;
    const std::size_t cy     = total_cols / 2;
    const std::size_t radius = std::min(total_rows, total_cols) / 10;
    const std::size_t row_lo = (cx >= radius) ? cx - radius : 0;
    const std::size_t row_hi = cx + radius;
    const std::size_t col_lo = (cy >= radius) ? cy - radius : 0;
    const std::size_t col_hi = cy + radius;

    const std::size_t owned_start = d.local_owned_start();
    for (std::size_t k = 0; k < d.num_rows; ++k) {
        const std::size_t global_i = d.start_row + k;
        if (global_i < row_lo || global_i > row_hi) continue;
        for (std::size_t j = col_lo; j <= col_hi && j < total_cols; ++j) {
            u_local(owned_start + k, j) = 100.0;
        }
    }
}

// Scan only the owned rows for min and max so halo-driven zeros do
// not contaminate the report.
std::pair<double, double> owned_min_max(const Grid& u,
                                        const Decomposition& d) {
    double lo = +1e308;
    double hi = -1e308;
    const std::size_t owned_start = d.local_owned_start();
    for (std::size_t k = 0; k < d.num_rows; ++k) {
        for (std::size_t j = 0; j < u.cols(); ++j) {
            const double v = u(owned_start + k, j);
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    }
    return {lo, hi};
}

void run_hybrid(std::size_t total_rows,
                std::size_t total_cols,
                int steps,
                int rank,
                int size) {
    const Decomposition d = decompose(total_rows, rank, size);

    Grid u_local(d.local_rows_with_halo(), total_cols);
    Grid u_local_next(d.local_rows_with_halo(), total_cols);
    initial_hot_spot_local(u_local, d, total_rows, total_cols);

    // Rank-by-rank decomposition print, serialized so the lines do
    // not interleave on the terminal.
    for (int r = 0; r < size; ++r) {
        if (r == rank) {
            std::printf("  rank %d: owns rows [%zu, %zu), local buffer %zu x %zu (halo top=%d bottom=%d)\n",
                        rank, d.start_row, d.start_row + d.num_rows,
                        d.local_rows_with_halo(), total_cols,
                        d.has_north ? 1 : 0, d.has_south ? 1 : 0);
            std::fflush(stdout);
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    const auto t_start = std::chrono::steady_clock::now();
    for (int t = 0; t < steps; ++t) {
        exchange_halos(u_local, d);
        step(u_local, u_local_next, kDiffusionNumber);
        std::swap(u_local, u_local_next);
    }
    const auto t_end = std::chrono::steady_clock::now();

    const double elapsed_s =
        std::chrono::duration<double>(t_end - t_start).count();

    auto [lo, hi] = owned_min_max(u_local, d);
    for (int r = 0; r < size; ++r) {
        if (r == rank) {
            std::printf("  rank %d: owned min=%.4f max=%.4f, elapsed %.3fs\n",
                        rank, lo, hi, elapsed_s);
            std::fflush(stdout);
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }
}

}  // namespace

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank = 0;
    int size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const std::size_t rows  = (argc > 1) ? static_cast<std::size_t>(std::atoi(argv[1])) : 256u;
    const std::size_t cols  = (argc > 2) ? static_cast<std::size_t>(std::atoi(argv[2])) : 256u;
    const int steps         = (argc > 3) ? std::atoi(argv[3]) : 500;

    int threads = 1;
#ifdef _OPENMP
    threads = omp_get_max_threads();
#endif

    if (rank == 0) {
        std::printf("StencilFlow 0.1.0: %zux%zu grid, %d steps, c = %.2f, ranks = %d, threads/rank = %d\n",
                    rows, cols, steps, kDiffusionNumber, size, threads);
        std::fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    run_hybrid(rows, cols, steps, rank, size);

    MPI_Finalize();
    return 0;
}
