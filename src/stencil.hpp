// 5-point heat-equation stencil, one explicit forward-Euler timestep.
//
//     out[i,j] = (1 - 4c) * in[i,j]
//                + c * (in[i-1,j] + in[i+1,j] + in[i,j-1] + in[i,j+1])
//
// Interior cells only. Boundary cells are copied from in to out
// (Dirichlet BC: edges stay frozen).
//
// c is the dimensionless diffusion number c = alpha * dt / dx^2.
// For 2D forward Euler, stability requires c <= 0.25.

#pragma once

class Grid;

void step(const Grid& in, Grid& out, double c);
