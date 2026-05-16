#include "decomp.hpp"

#include <algorithm>

Decomposition decompose(std::size_t total_rows, int rank, int size) {
    Decomposition d{};

    const std::size_t base      = total_rows / static_cast<std::size_t>(size);
    const std::size_t remainder = total_rows % static_cast<std::size_t>(size);
    const std::size_t r         = static_cast<std::size_t>(rank);

    // The first 'remainder' ranks get one extra row each.
    d.num_rows  = base + (r < remainder ? 1u : 0u);
    d.start_row = r * base + std::min(r, remainder);

    d.has_north = (rank > 0);
    d.has_south = (rank < size - 1);
    d.north_rank = d.has_north ? rank - 1 : MPI_PROC_NULL;
    d.south_rank = d.has_south ? rank + 1 : MPI_PROC_NULL;

    return d;
}
