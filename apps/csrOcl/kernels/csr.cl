// Full-device CSR SpMV + PCG helpers (unstructured-ready matrix format).
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// y = A * x  (CSR: rowPtr[n+1], colInd[nnz], vals[nnz])
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
    const int beg = rowPtr[i];
    const int end = rowPtr[i + 1];
    for (int k = beg; k < end; ++k) {
        acc += vals[k] * x[colInd[k]];
    }
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

// z := 2*t - invDiag * (A t)   poly2 combine (z enters as A*t)
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

__kernel void vec_axpy(
    __global double* y,
    __global const double* x,
    const double a,
    const int n)
{
    const int i = get_global_id(0);
    if (i >= n) return;
    y[i] = a * x[i] + y[i];
}

__kernel void vec_xpay(
    __global double* y,
    __global const double* x,
    const double a,
    const int n)
{
    const int i = get_global_id(0);
    if (i >= n) return;
    y[i] = x[i] + a * y[i];
}

__kernel void vec_copy(
    __global double* dst,
    __global const double* src,
    const int n)
{
    const int i = get_global_id(0);
    if (i >= n) return;
    dst[i] = src[i];
}

__kernel void vec_set(
    __global double* x,
    const double a,
    const int n)
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
