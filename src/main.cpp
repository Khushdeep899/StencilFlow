// StencilFlow: hybrid MPI + OpenMP 2D heat equation solver.
//
// Slice 6 driver: serial time loop. MPI is initialized so the binary
// can also be launched under mpirun, but only rank 0 does work. The
// real per-rank decomposition lands in slice 8.
//
// Usage:
//     ./stencilflow [rows] [cols] [steps] [save_every]
// Defaults: 256 256 500 50.

#include <mpi.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <utility>

#include "grid.hpp"
#include "stencil.hpp"

namespace {

constexpr double kDiffusionNumber = 0.2;  // c = alpha * dt / dx^2, c <= 0.25.

void initial_hot_spot(Grid& u) {
    const std::size_t rows   = u.rows();
    const std::size_t cols   = u.cols();
    const std::size_t cx     = rows / 2;
    const std::size_t cy     = cols / 2;
    const std::size_t radius = std::min(rows, cols) / 10;

    for (std::size_t i = cx - radius; i <= cx + radius; ++i) {
        for (std::size_t j = cy - radius; j <= cy + radius; ++j) {
            u(i, j) = 100.0;
        }
    }
}

void write_frame(const Grid& u, int t) {
    char fname[64];
    std::snprintf(fname, sizeof(fname), "frames/frame_%05d.pgm", t);
    u.write_pgm(fname);
    std::printf("  step %d -> %s\n", t, fname);
}

void run_serial(int rows, int cols, int steps, int save_every) {
    std::filesystem::create_directories("frames");

    Grid u(rows, cols);
    Grid u_next(rows, cols);
    initial_hot_spot(u);

    for (int t = 0; t < steps; ++t) {
        if (t % save_every == 0) {
            write_frame(u, t);
        }
        step(u, u_next, kDiffusionNumber);
        std::swap(u, u_next);
    }
    write_frame(u, steps);
}

}  // namespace

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank = 0;
    int size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int rows       = (argc > 1) ? std::atoi(argv[1]) : 256;
    const int cols       = (argc > 2) ? std::atoi(argv[2]) : 256;
    const int steps      = (argc > 3) ? std::atoi(argv[3]) : 500;
    const int save_every = (argc > 4) ? std::atoi(argv[4]) : 50;

    if (rank == 0) {
        std::printf("StencilFlow 0.1.0: %dx%d grid, %d steps, save every %d, c = %.2f, ranks = %d\n",
                    rows, cols, steps, save_every, kDiffusionNumber, size);
        if (size > 1) {
            std::printf("Note: serial driver, ranks 1..%d will idle until slice 8 adds decomposition.\n",
                        size - 1);
        }
        run_serial(rows, cols, steps, save_every);
    }

    MPI_Finalize();
    return 0;
}
