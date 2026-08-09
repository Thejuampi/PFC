// Full-device Laplace / diffusion helpers for structured 2D grid.
// Cell layout: i + j * nx,  i in [0,nx), j in [0,ny)
// Boundary cells: Dirichlet (identity rows). Interior: 5-point stencil.

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

inline int idx(const int i, const int j, const int nx) { return i + j * nx; }

// mark[c] = 1 interior, 0 boundary
__kernel void mark_interior(
    __global uchar* mark,
    const int nx,
    const int ny)
{
    const int c = get_global_id(0);
    const int n = nx * ny;
    if (c >= n) return;
    const int i = c % nx;
    const int j = c / nx;
    const int interior = (i > 0 && i < nx - 1 && j > 0 && j < ny - 1) ? 1 : 0;
    mark[c] = (uchar)interior;
}

// Initial T + boundary values matching cases/laplaceCpu (PFC geometry).
// hPatch (j=ny-1): 573, cPatch (i=0): 373, fixedWalls (i=nx-1 or j=0): 273
__kernel void init_temperature(
    __global double* T,
    const int nx,
    const int ny,
    const double T_init,
    const double T_h,
    const double T_c,
    const double T_wall)
{
    const int c = get_global_id(0);
    const int n = nx * ny;
    if (c >= n) return;
    const int i = c % nx;
    const int j = c / nx;

    double v = T_init;
    // Corners: fixedWalls / cold / hot priority — walls & cold on edges
    if (j == 0 || i == nx - 1) {
        v = T_wall;
    }
    if (i == 0) {
        v = T_c;
    }
    if (j == ny - 1) {
        v = T_h;
    }
    // corner (0, ny-1): hot vs cold — use average-ish OpenFOAM order: last wins;
    // match fixedValue per face: hot top takes top edge including corners with i=0?
    // Use: top edge hot, left edge cold except top-left = hot (top overrides).
    if (j == ny - 1) v = T_h;
    if (i == 0 && j != ny - 1) v = T_c;
    if ((j == 0 || i == nx - 1) && j != ny - 1 && i != 0) v = T_wall;

    T[c] = v;
}

// Assemble constant DIA matrix for implicit Euler:
// A = I/dt - DT * L,  L ≈ discrete Laplacian (negative diagonal).
// Offsets order: 0:-nx  1:-1  2:0  3:+1  4:+nx
__kernel void assemble_A_dia(
    __global double* dia0,   // -nx
    __global double* dia1,   // -1
    __global double* dia2,   //  0
    __global double* dia3,   // +1
    __global double* dia4,   // +nx
    __global double* invDiag,
    __global const uchar* mark,
    const int nx,
    const int ny,
    const double dx,
    const double dy,
    const double dt,
    const double DT)
{
    const int c = get_global_id(0);
    const int n = nx * ny;
    if (c >= n) return;

    dia0[c] = 0.0;
    dia1[c] = 0.0;
    dia2[c] = 0.0;
    dia3[c] = 0.0;
    dia4[c] = 0.0;

    if (!mark[c]) {
        // Dirichlet row
        dia2[c] = 1.0;
        invDiag[c] = 1.0;
        return;
    }

    const double cx = DT / (dx * dx);
    const double cy = DT / (dy * dy);
    const double center = (1.0 / dt) + 2.0 * cx + 2.0 * cy;

    dia2[c] = center;
    dia1[c] = -cx; // west  (c-1)
    dia3[c] = -cx; // east  (c+1)
    dia0[c] = -cy; // south (c-nx)
    dia4[c] = -cy; // north (c+nx)
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
    __global const double* x,
    __global double* y,
    const int nx,
    const int n)
{
    const int c = get_global_id(0);
    if (c >= n) return;

    double acc = dia2[c] * x[c];
    if (c >= nx)           acc += dia0[c] * x[c - nx];
    if ((c % nx) > 0)      acc += dia1[c] * x[c - 1];
    if ((c % nx) < nx - 1) acc += dia3[c] * x[c + 1];
    if (c + nx < n)        acc += dia4[c] * x[c + nx];
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
// color: 0 = (i+j) even, 1 = (i+j) odd. Uses latest neighbor values of the other color.
// DIA offsets: 0:-nx  1:-1  2:0  3:+1  4:+nx
__kernel void rbgs_sweep(
    __global double* z,
    __global const double* dia0,
    __global const double* dia1,
    __global const double* dia2,
    __global const double* dia3,
    __global const double* dia4,
    __global const double* rhs,
    __global const uchar* mark,
    const int nx,
    const int n,
    const int color)
{
    const int c = get_global_id(0);
    if (c >= n) return;

    const int i = c % nx;
    const int j = c / nx;
    if (((i + j) & 1) != color) return;

    // Dirichlet / identity rows: z = rhs
    if (!mark[c]) {
        z[c] = rhs[c];
        return;
    }

    double sigma = 0.0;
    if (c >= nx)           sigma += dia0[c] * z[c - nx];
    if (i > 0)             sigma += dia1[c] * z[c - 1];
    if (i < nx - 1)        sigma += dia3[c] * z[c + 1];
    if (c + nx < n)        sigma += dia4[c] * z[c + nx];

    // A_ii * z_i + sigma = rhs_i  =>  z_i = (rhs - sigma) / A_ii
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

// Partial squares: partial[gid] = sum of x[i]^2 over work-group
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

// Partial dot: partial[gid] = sum a[i]*b[i]
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
