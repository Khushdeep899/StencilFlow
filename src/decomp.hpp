// 1D horizontal-strip decomposition for the MPI domain split.
//
// Each rank owns a contiguous block of global rows. To run the
// 5-point stencil on the top and bottom rows of an owned block, each
// rank also allocates one halo row above and one below (the rows
// owned by its north and south neighbors). Halos are exchanged each
// timestep in slice 9.

#pragma once

#include <mpi.h>

#include <cstddef>

struct Decomposition {
    std::size_t start_row;  // global index of the first owned row
    std::size_t num_rows;   // number of owned rows on this rank

    // MPI ranks of the neighbors. MPI_PROC_NULL means "no neighbor";
    // passing it to MPI_Send / MPI_Recv makes the call a no-op,
    // which neatly handles the edges of the global grid.
    int north_rank;
    int south_rank;

    // True if this rank has the corresponding neighbor. Convenience
    // duplicates of (north_rank != MPI_PROC_NULL) etc.
    bool has_north;
    bool has_south;

    // Number of rows in the local buffer including halo rows on each
    // side. Equals num_rows + has_north + has_south.
    std::size_t local_rows_with_halo() const {
        return num_rows + (has_north ? 1u : 0u) + (has_south ? 1u : 0u);
    }

    // Local row index of the first owned row (skips the top halo if
    // there is one).
    std::size_t local_owned_start() const {
        return has_north ? 1u : 0u;
    }
};

// Compute the decomposition for a given total row count, rank, and
// MPI size. Even-as-possible: the first (total_rows % size) ranks
// each get one extra row.
Decomposition decompose(std::size_t total_rows, int rank, int size);
