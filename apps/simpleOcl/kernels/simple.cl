// simpleOcl — device-resident SIMPLE helpers (CSR PCG + structured FD correctors)
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

__kernel void spmv_csr(
    __global const int* rowPtr,
    __global const int* colInd,
    __global const double* vals,
    __global const double* x,
    __global double* y,
    const int n)
{
    const int i = get_global_id(0);
    if (i >= n) return;
    double acc = 0.0;
    for (int k = rowPtr[i]; k < rowPtr[i + 1]; ++k)
        acc += vals[k] * x[colInd[k]];
    y[i] = acc;
}

__kernel void apply_jacobi(
    __global double* z,
    __global const double* invDiag,
    __global const double* r,
    const int n)
{
    const int i = get_global_id(0);
    if (i >= n) return;
    z[i] = invDiag[i] * r[i];
}

__kernel void poly2_combine(
    __global double* z,
    __global const double* t,
    __global const double* invDiag,
    const int n)
{
    const int i = get_global_id(0);
    if (i >= n) return;
    z[i] = 2.0 * t[i] - invDiag[i] * z[i];
}

__kernel void vec_axpy(__global double* y, __global const double* x, const double a, const int n)
{
    const int i = get_global_id(0);
    if (i >= n) return;
    y[i] = a * x[i] + y[i];
}

__kernel void vec_xpay(__global double* y, __global const double* x, const double a, const int n)
{
    const int i = get_global_id(0);
    if (i >= n) return;
    y[i] = x[i] + a * y[i];
}

__kernel void vec_copy(__global double* dst, __global const double* src, const int n)
{
    const int i = get_global_id(0);
    if (i >= n) return;
    dst[i] = src[i];
}

__kernel void vec_set(__global double* x, const double a, const int n)
{
    const int i = get_global_id(0);
    if (i >= n) return;
    x[i] = a;
}

__kernel void vec_residual(
    __global double* r,
    __global const double* b,
    __global const double* Ax,
    const int n)
{
    const int i = get_global_id(0);
    if (i >= n) return;
    r[i] = b[i] - Ax[i];
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
    for (int i = gid; i < n; i += gsize)
        sum += a[i] * b[i];
    scratch[lid] = sum;
    barrier(CLK_LOCAL_MEM_FENCE);
    for (int offset = get_local_size(0) / 2; offset > 0; offset >>= 1) {
        if (lid < offset) scratch[lid] += scratch[lid + offset];
        barrier(CLK_LOCAL_MEM_FENCE);
    }
    if (lid == 0) partial[get_group_id(0)] = scratch[0];
}

// ---- SIMPLE structured 2D (collocated, educational) ----
// idx = i + j * nx

// u* = u + nu*dt*Laplace(u) - alpha * grad(p)   (diffusive momentum + pressure gradient)
__kernel void momentum_predictor(
    __global double* ustar,
    __global double* vstar,
    __global const double* u,
    __global const double* v,
    __global const double* p,
    const int nx,
    const int ny,
    const double alpha,
    const double nu_dt,
    const double dx,
    const double dy)
{
    const int i = get_global_id(0) % nx;
    const int j = get_global_id(0) / nx;
    if (j >= ny) return;
    const int c = i + j * nx;
    // boundaries: keep velocity (lid BC applied via enforce_lid_bc)
    if (i == 0 || i == nx - 1 || j == 0 || j == ny - 1) {
        ustar[c] = u[c];
        vstar[c] = v[c];
        return;
    }
    const double invdx2 = 1.0 / (dx * dx);
    const double invdy2 = 1.0 / (dy * dy);
    const double lap_u =
        (u[c + 1] - 2.0 * u[c] + u[c - 1]) * invdx2 +
        (u[c + nx] - 2.0 * u[c] + u[c - nx]) * invdy2;
    const double lap_v =
        (v[c + 1] - 2.0 * v[c] + v[c - 1]) * invdx2 +
        (v[c + nx] - 2.0 * v[c] + v[c - nx]) * invdy2;
    const double dpx = (p[c + 1] - p[c - 1]) / (2.0 * dx);
    const double dpy = (p[c + nx] - p[c - nx]) / (2.0 * dy);
    ustar[c] = u[c] + nu_dt * lap_u - alpha * dpx;
    vstar[c] = v[c] + nu_dt * lap_v - alpha * dpy;
}

// b = div(u*) ; boundary rows of Poisson are Dirichlet p=0 → b=0
__kernel void continuity_rhs(
    __global double* b,
    __global const double* ustar,
    __global const double* vstar,
    const int nx,
    const int ny,
    const double dx,
    const double dy)
{
    const int i = get_global_id(0) % nx;
    const int j = get_global_id(0) / nx;
    if (j >= ny) return;
    const int c = i + j * nx;
    if (i == 0 || i == nx - 1 || j == 0 || j == ny - 1) {
        b[c] = 0.0;
        return;
    }
    const double dudx = (ustar[c + 1] - ustar[c - 1]) / (2.0 * dx);
    const double dvdy = (vstar[c + nx] - vstar[c - nx]) / (2.0 * dy);
    b[c] = -(dudx + dvdy);
}

// u := u* - alpha * grad(p)  (pressure correction)
__kernel void velocity_correct(
    __global double* u,
    __global double* v,
    __global const double* ustar,
    __global const double* vstar,
    __global const double* p,
    const int nx,
    const int ny,
    const double alpha,
    const double dx,
    const double dy)
{
    const int i = get_global_id(0) % nx;
    const int j = get_global_id(0) / nx;
    if (j >= ny) return;
    const int c = i + j * nx;
    if (i == 0 || i == nx - 1 || j == 0 || j == ny - 1) {
        u[c] = ustar[c];
        v[c] = vstar[c];
        return;
    }
    const double dpx = (p[c + 1] - p[c - 1]) / (2.0 * dx);
    const double dpy = (p[c + nx] - p[c - nx]) / (2.0 * dy);
    u[c] = ustar[c] - alpha * dpx;
    v[c] = vstar[c] - alpha * dpy;
}

// Enforce lid-driven cavity BC every outer (top lid u=1)
__kernel void enforce_lid_bc(
    __global double* u,
    __global double* v,
    const int nx,
    const int ny)
{
    const int i = get_global_id(0) % nx;
    const int j = get_global_id(0) / nx;
    if (j >= ny) return;
    const int c = i + j * nx;
    if (j == ny - 1) {
        u[c] = 1.0;
        v[c] = 0.0;
    } else if (i == 0 || i == nx - 1 || j == 0) {
        u[c] = 0.0;
        v[c] = 0.0;
    }
}
