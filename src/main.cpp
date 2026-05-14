// StencilFlow: hybrid MPI + OpenMP 2D heat equation solver.
//
// Day 1: this file currently does nothing but bring up MPI, report each
// rank's identity, and exit cleanly. It will grow on Day 2 to include CLI
// parsing, grid allocation, and the time-stepping loop.

#include <mpi.h>

#include <cstdio>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank = 0;
    int size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        std::printf("StencilFlow 0.1.0 starting on %d rank%s\n",
                    size, size == 1 ? "" : "s");
    }

    std::printf("  rank %d of %d reporting in\n", rank, size);

    MPI_Finalize();
    return 0;
}
