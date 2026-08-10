// Full-device Laplace / diffusion helpers for structured 2D/3D grid.
// Cell layout: i + j * nx + k * (nx*ny),  i in [0,nx), j in [0,ny), k in [0,nz)
// nz==1 → pure 2D (5-point). nz>1 → 7-point stencil.
// Boundary cells: Dirichlet (identity rows).

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// mark[c] = 1 interior, 0 boundary
__kernel void mark_interior(
    __global uchar* mark,
    const int nx,
    const int ny,
    const int nz)
{
    const int c = get_global_id(0);
    const int n = nx * ny * nz;
    if (c >= n) return;
    const int nxy = nx * ny;
    const int i = c % nx;
    const int j = (c / nx) % ny;
    const int k = c / nxy;
    int interior = (i > 0 && i < nx - 1 && j > 0 && j < ny - 1) ? 1 : 0;
    if (nz > 1) {
        if (k == 0 || k == nz - 1) interior = 0;
    }
    mark[c] = (uchar)interior;
}

// Initial T + boundary values matching cases/laplaceCpu (PFC geometry) in xy.
// hPatch (j=ny-1): 573, cPatch (i=0): 373, fixedWalls (i=nx-1 or j=0): 273
// For 3D, z=0 and z=nz-1 faces use T_wall (273) unless already set by xy edges.
__kernel void init_temperature(
    __global double* T,
    const int nx,
    const int ny,
    const int nz,
    const double T_init,
    const double T_h,
    const double T_c,
    const double T_wall)
{
    const int c = get_global_id(0);
    const int n = nx * ny * nz;
    if (c >= n) return;
    const int nxy = nx * ny;
    const int i = c % nx;
    const int j = (c / nx) % ny;
    const int k = c / nxy;

    double v = T_init;
    // Match 2D BC on every z-slice for continuity with laplaceCpu when nz==1
    if (j == 0 || i == nx - 1) v = T_wall;
    if (i == 0 && j != ny - 1) v = T_c;
    if (j == ny - 1) v = T_h;
    if ((j == 0 || i == nx - 1) && j != ny - 1 && i != 0) v = T_wall;

    if (nz > 1 && (k == 0 || k == nz - 1)) {
        // z faces: wall, but keep top/hot and left/cold where they own the edge
        if (j != ny - 1 && i != 0) v = T_wall;
        if (j == ny - 1) v = T_h;
        if (i == 0 && j != ny - 1) v = T_c;
    }

    T[c] = v;
}

// Assemble constant DIA matrix for implicit Euler:
// A = I/dt - DT * L,  L ≈ discrete Laplacian (negative diagonal).
// Offsets: 0:-nx  1:-1  2:0  3:+1  4:+nx  5:-nxy  6:+nxy
__kernel void assemble_A_dia(
    __global double* dia0,   // -nx
    __global double* dia1,   // -1
    __global double* dia2,   //  0
    __global double* dia3,   // +1
    __global double* dia4,   // +nx
    __global double* dia5,   // -nxy
    __global double* dia6,   // +nxy
    __global double* invDiag,
    __global const uchar* mark,
    const int nx,
    const int ny,
    const int nz,
    const double dx,
    const double dy,
    const double dz,
    const double dt,
    const double DT)
{
    const int c = get_global_id(0);
    const int nxy = nx * ny;
    const int n = nxy * nz;
    if (c >= n) return;

    dia0[c] = 0.0;
    dia1[c] = 0.0;
    dia2[c] = 0.0;
    dia3[c] = 0.0;
    dia4[c] = 0.0;
    dia5[c] = 0.0;
    dia6[c] = 0.0;

    if (!mark[c]) {
        dia2[c] = 1.0;
        invDiag[c] = 1.0;
        return;
    }

    const double cx = DT / (dx * dx);
    const double cy = DT / (dy * dy);
    const double cz = (nz > 1) ? (DT / (dz * dz)) : 0.0;
    const double center = (1.0 / dt) + 2.0 * cx + 2.0 * cy + 2.0 * cz;

    dia2[c] = center;
    dia1[c] = -cx;
    dia3[c] = -cx;
    dia0[c] = -cy;
    dia4[c] = -cy;
    if (nz > 1) {
        dia5[c] = -cz;
        dia6[c] = -cz;
    }
    invDiag[c] = 1.0 / center;
}

// b = T/dt interior; b = T_bc on boundary (T already holds BC values)
__kernel void build_rhs(
    __global double* b,
    __global const double* T,
    __global const uchar* mark,
    const int n,
    const double dt)
{
    const int c = get_global_id(0);
    if (c >= n) return;
    if (!mark[c]) {
        b[c] = T[c];
    } else {
        b[c] = T[c] / dt;
    }
}

__kernel void spmv_dia(
    __global const double* dia0,
    __global const double* dia1,
    __global const double* dia2,
    __global const double* dia3,
    __global const double* dia4,
    __global const double* dia5,
    __global const double* dia6,
    __global const double* x,
    __global double* y,
    const int nx,
    const int nxy,
    const int n)
{
    const int c = get_global_id(0);
    if (c >= n) return;

    double acc = dia2[c] * x[c];
    if (c >= nx)            acc += dia0[c] * x[c - nx];
    if ((c % nx) > 0)       acc += dia1[c] * x[c - 1];
    if ((c % nx) < nx - 1)  acc += dia3[c] * x[c + 1];
    if (c + nx < n)         acc += dia4[c] * x[c + nx];
    if (c >= nxy)           acc += dia5[c] * x[c - nxy];
    if (c + nxy < n)        acc += dia6[c] * x[c + nxy];
    y[c] = acc;
}

__kernel void vec_axpy(
    __global double* y,
    __global const double* x,
    const double a,
    const int n)
{
    const int c = get_global_id(0);
    if (c >= n) return;
    y[c] = a * x[c] + y[c];
}

__kernel void vec_xpay(
    __global double* y,
    __global const double* x,
    const double a,
    const int n)
{
    const int c = get_global_id(0);
    if (c >= n) return;
    y[c] = x[c] + a * y[c];
}

__kernel void vec_copy(
    __global double* dst,
    __global const double* src,
    const int n)
{
    const int c = get_global_id(0);
    if (c >= n) return;
    dst[c] = src[c];
}

__kernel void vec_scal(
    __global double* x,
    const double a,
    const int n)
{
    const int c = get_global_id(0);
    if (c >= n) return;
    x[c] *= a;
}

__kernel void vec_set(
    __global double* x,
    const double a,
    const int n)
{
    const int c = get_global_id(0);
    if (c >= n) return;
    x[c] = a;
}

// z = invDiag * r
__kernel void apply_jacobi(
    __global double* z,
    __global const double* invDiag,
    __global const double* r,
    const int n)
{
    const int c = get_global_id(0);
    if (c >= n) return;
    z[c] = invDiag[c] * r[c];
}

// z := 2*t - invDiag * z   (z enters as A*t) for poly2 preconditioner
__kernel void poly2_combine(
    __global double* z,
    __global const double* t,
    __global const double* invDiag,
    const int n)
{
    const int c = get_global_id(0);
    if (c >= n) return;
    z[c] = 2.0 * t[c] - invDiag[c] * z[c];
}

// One red-black Gauss-Seidel sweep color for approximate solve A z ≈ rhs.
// color: 0 = (i+j+k) even, 1 = odd.
__kernel void rbgs_sweep(
    __global double* z,
    __global const double* dia0,
    __global const double* dia1,
    __global const double* dia2,
    __global const double* dia3,
    __global const double* dia4,
    __global const double* dia5,
    __global const double* dia6,
    __global const double* rhs,
    __global const uchar* mark,
    const int nx,
    const int nxy,
    const int n,
    const int color)
{
    const int c = get_global_id(0);
    if (c >= n) return;

    const int i = c % nx;
    const int j = (c / nx) % (nxy / nx);
    const int k = c / nxy;
    if (((i + j + k) & 1) != color) return;

    if (!mark[c]) {
        z[c] = rhs[c];
        return;
    }

    double sigma = 0.0;
    if (c >= nx)           sigma += dia0[c] * z[c - nx];
    if (i > 0)             sigma += dia1[c] * z[c - 1];
    if (i < nx - 1)        sigma += dia3[c] * z[c + 1];
    if (c + nx < n)        sigma += dia4[c] * z[c + nx];
    if (c >= nxy)          sigma += dia5[c] * z[c - nxy];
    if (c + nxy < n)       sigma += dia6[c] * z[c + nxy];

    z[c] = (rhs[c] - sigma) / dia2[c];
}

// r = b - y  (y holds A*x)
__kernel void vec_residual(
    __global double* r,
    __global const double* b,
    __global const double* Ax,
    const int n)
{
    const int c = get_global_id(0);
    if (c >= n) return;
    r[c] = b[c] - Ax[c];
}

__kernel void reduce_sum_sq(
    __global const double* x,
    __global double* partial,
    __local double* scratch,
    const int n)
{
    const int gid = get_global_id(0);
    const int lid = get_local_id(0);
    const int gsize = get_global_size(0);
    double sum = 0.0;
    for (int i = gid; i < n; i += gsize) {
        const double v = x[i];
        sum += v * v;
    }
    scratch[lid] = sum;
    barrier(CLK_LOCAL_MEM_FENCE);
    for (int offset = get_local_size(0) / 2; offset > 0; offset >>= 1) {
        if (lid < offset) scratch[lid] += scratch[lid + offset];
        barrier(CLK_LOCAL_MEM_FENCE);
    }
    if (lid == 0) partial[get_group_id(0)] = scratch[0];
}

__kernel void reduce_dot(
    __global const double* a,
    __global const double* b,
    __global double* partial,
    __local double* scratch,
    const int n)
{
    const int gid = get_global_id(0);
    const int lid = get_local_id(0);
    const int gsize = get_global_size(0);
    double sum = 0.0;
    for (int i = gid; i < n; i += gsize) {
        sum += a[i] * b[i];
    }
    scratch[lid] = sum;
    barrier(CLK_LOCAL_MEM_FENCE);
    for (int offset = get_local_size(0) / 2; offset > 0; offset >>= 1) {
        if (lid < offset) scratch[lid] += scratch[lid + offset];
        barrier(CLK_LOCAL_MEM_FENCE);
    }
    if (lid == 0) partial[get_group_id(0)] = scratch[0];
}
