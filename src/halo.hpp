// MPI halo exchange for the 1D horizontal-strip decomposition.
//
// Each rank's local Grid has one halo row at the top (if a north
// neighbor exists) and one at the bottom (if a south neighbor
// exists). Before each stencil step the halos must be filled with
// the neighbor's most recent boundary row. This function does that
// via two MPI_Sendrecv calls.

#pragma once

class Grid;
struct Decomposition;

void exchange_halos(Grid& u_local, const Decomposition& d);
