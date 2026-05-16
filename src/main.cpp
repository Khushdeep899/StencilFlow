// StencilFlow: hybrid MPI + OpenMP 2D heat equation solver.
//
// Slice 11: each rank holds only its strip plus halos. The time
// loop is:
//   exchange_halos -> step -> swap.
// Every save_every iterations, MPI_Gatherv collects all owned rows
// to rank 0 which writes one combined PGM frame.

#include <mpi.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <utility>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "decomp.hpp"
#include "grid.hpp"
#include "halo.hpp"
#include "stencil.hpp"

namespace {

constexpr double kDiffusionNumber = 0.2;

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

// Collect every rank's owned rows onto rank 0 with MPI_Gatherv.
// Returns a fully populated Grid on rank 0; on other ranks the
// returned Grid is empty (0 rows). MPI_Gatherv is needed (not the
// simpler MPI_Gather) because per-rank slab sizes can differ by 1
// when total_rows is not divisible by size.
Grid gather_to_root(const Grid& u_local,
                    const Decomposition& d,
                    std::size_t total_rows,
                    std::size_t total_cols,
                    int rank,
                    int size) {
    Grid global = (rank == 0) ? Grid(total_rows, total_cols) : Grid(0, 0);

    const double* send_buf = &u_local(d.local_owned_start(), 0);
    const int send_count   = static_cast<int>(d.num_rows * total_cols);

    std::vector<int> recvcounts;
    std::vector<int> displs;
    if (rank == 0) {
        recvcounts.resize(static_cast<std::size_t>(size));
        displs.resize(static_cast<std::size_t>(size));
        for (int r = 0; r < size; ++r) {
            const Decomposition dr = decompose(total_rows, r, size);
            recvcounts[static_cast<std::size_t>(r)] =
                static_cast<int>(dr.num_rows * total_cols);
            displs[static_cast<std::size_t>(r)] =
                static_cast<int>(dr.start_row * total_cols);
        }
    }

    MPI_Gatherv(send_buf, send_count, MPI_DOUBLE,
                global.data(),
                rank == 0 ? recvcounts.data() : nullptr,
                rank == 0 ? displs.data()     : nullptr,
                MPI_DOUBLE, 0, MPI_COMM_WORLD);

    return global;
}

void write_global_frame(const Grid& global, int t) {
    char fname[64];
    std::snprintf(fname, sizeof(fname), "frames/frame_%05d.pgm", t);
    global.write_pgm(fname);
    std::printf("  step %d -> %s\n", t, fname);
    std::fflush(stdout);
}

void run_hybrid(std::size_t total_rows,
                std::size_t total_cols,
                int steps,
                int save_every,
                int rank,
                int size) {
    const Decomposition d = decompose(total_rows, rank, size);

    Grid u_local(d.local_rows_with_halo(), total_cols);
    Grid u_local_next(d.local_rows_with_halo(), total_cols);
    initial_hot_spot_local(u_local, d, total_rows, total_cols);

    for (int r = 0; r < size; ++r) {
        if (r == rank) {
            std::printf("  rank %d: owns rows [%zu, %zu), local buffer %zu x %zu\n",
                        rank, d.start_row, d.start_row + d.num_rows,
                        d.local_rows_with_halo(), total_cols);
            std::fflush(stdout);
        }
        MPI_Barrier(MPI_COMM_WORLD);
    }

    if (rank == 0) {
        std::filesystem::create_directories("frames");
    }

    const auto t_start = std::chrono::steady_clock::now();
    for (int t = 0; t < steps; ++t) {
        if (t % save_every == 0) {
            Grid global = gather_to_root(u_local, d, total_rows, total_cols, rank, size);
            if (rank == 0) write_global_frame(global, t);
        }
        exchange_halos(u_local, d);
        step(u_local, u_local_next, kDiffusionNumber);
        std::swap(u_local, u_local_next);
    }

    // Final frame at t == steps.
    {
        Grid global = gather_to_root(u_local, d, total_rows, total_cols, rank, size);
        if (rank == 0) write_global_frame(global, steps);
    }

    const auto t_end = std::chrono::steady_clock::now();
    const double elapsed_s =
        std::chrono::duration<double>(t_end - t_start).count();

    if (rank == 0) {
        const double cell_updates =
            static_cast<double>(total_rows) * total_cols * steps;
        std::printf("elapsed: %.3fs, %.2e cell-updates/s\n",
                    elapsed_s, cell_updates / elapsed_s);
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
    const int save_every    = (argc > 4) ? std::atoi(argv[4]) : 100;

    int threads = 1;
#ifdef _OPENMP
    threads = omp_get_max_threads();
#endif

    if (rank == 0) {
        std::printf("StencilFlow 0.1.0: %zux%zu grid, %d steps, save every %d, c = %.2f, ranks = %d, threads/rank = %d\n",
                    rows, cols, steps, save_every, kDiffusionNumber, size, threads);
        std::fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);

    run_hybrid(rows, cols, steps, save_every, rank, size);

    MPI_Finalize();
    return 0;
}
