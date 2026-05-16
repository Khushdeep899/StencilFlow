#include "halo.hpp"

#include <mpi.h>

#include <cstddef>

#include "decomp.hpp"
#include "grid.hpp"

namespace {

// Distinct tags for the two halo phases so the runtime can never
// accidentally match a "send-down" with a "receive-up" or vice
// versa, even if ranks are interleaving phases on the wire.
constexpr int kTagDown = 0;  // I send south,   I receive from north
constexpr int kTagUp   = 1;  // I send north,   I receive from south

}  // namespace

void exchange_halos(Grid& u_local, const Decomposition& d) {
    const std::size_t cols       = u_local.cols();
    const std::size_t owned_lo   = d.local_owned_start();
    const std::size_t owned_hi   = owned_lo + d.num_rows;          // one past last owned
    const std::size_t local_rows = u_local.rows();

    // Row pointers. When a neighbor is MPI_PROC_NULL the runtime
    // ignores the buffer entirely, so the placeholder targets we
    // pick for edge ranks (which happen to be owned rows) are
    // never read or written by MPI.
    double* first_owned  = &u_local(owned_lo, 0);
    double* last_owned   = &u_local(owned_hi - 1, 0);
    double* top_halo     = &u_local(0, 0);
    double* bottom_halo  = &u_local(local_rows - 1, 0);

    // Phase 1: send my last owned row south; receive into my top
    // halo row from north.
    MPI_Sendrecv(
        last_owned, static_cast<int>(cols), MPI_DOUBLE,
        d.south_rank, kTagDown,
        top_halo,   static_cast<int>(cols), MPI_DOUBLE,
        d.north_rank, kTagDown,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Phase 2: send my first owned row north; receive into my
    // bottom halo row from south.
    MPI_Sendrecv(
        first_owned,  static_cast<int>(cols), MPI_DOUBLE,
        d.north_rank, kTagUp,
        bottom_halo,  static_cast<int>(cols), MPI_DOUBLE,
        d.south_rank, kTagUp,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}
