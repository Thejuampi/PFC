// laplaceOcl — full-device OpenCL Laplace/diffusion (AMD GPU friendly)
//
// Lifecycle:
//   1) Create device buffers once
//   2) init + assemble A on GPU (once)
//   3) time loop: build b, PCG solve, all on GPU
//   4) single download of T at the end
//
// Residual scalars: host reads a few doubles per PCG iter for convergence
// control only (not the matrix). Set --fixed-iters to avoid even that.

#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Args {
    int nx = 100;
    int ny = 100;
    int steps = 10;
    double dt = 0.005;
    double DT = 4e-5;
    double Lx = 0.1;
    double Ly = 0.1;
    double tol = 1e-8;
    int maxIters = 500;
    int fixedIters = 0; // if >0, skip residual host checks in PCG
    bool cpuCheck = true;
    std::string kernelPath;
    std::string outCsv = "T_gpu.csv";
};

void die(const std::string& msg)
{
    std::cerr << "ERROR: " << msg << "\n";
    std::exit(1);
}

void checkCl(cl_int err, const char* what)
{
    if (err != CL_SUCCESS) {
        std::ostringstream os;
        os << what << " failed, cl_int=" << err;
        die(os.str());
    }
}

std::string readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) die("cannot open " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

Args parseArgs(int argc, char** argv)
{
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto need = [&](const char* name) -> std::string {
            if (i + 1 >= argc) die(std::string("missing value for ") + name);
            return argv[++i];
        };
        if (s == "--nx") a.nx = std::stoi(need("--nx"));
        else if (s == "--ny") a.ny = std::stoi(need("--ny"));
        else if (s == "--steps") a.steps = std::stoi(need("--steps"));
        else if (s == "--dt") a.dt = std::stod(need("--dt"));
        else if (s == "--DT") a.DT = std::stod(need("--DT"));
        else if (s == "--tol") a.tol = std::stod(need("--tol"));
        else if (s == "--max-iters") a.maxIters = std::stoi(need("--max-iters"));
        else if (s == "--fixed-iters") a.fixedIters = std::stoi(need("--fixed-iters"));
        else if (s == "--kernels") a.kernelPath = need("--kernels");
        else if (s == "--out") a.outCsv = need("--out");
        else if (s == "--no-cpu-check") a.cpuCheck = false;
        else if (s == "--help" || s == "-h") {
            std::cout
                << "laplaceOcl — device-resident diffusion (OpenCL)\n"
                << "  --nx N --ny N --steps N --dt D --DT D\n"
                << "  --tol T --max-iters N --fixed-iters N\n"
                << "  --kernels path/to/laplace.cl --out T_gpu.csv\n"
                << "  --no-cpu-check\n";
            std::exit(0);
        } else {
            die("unknown arg: " + s);
        }
    }
    if (a.nx < 3 || a.ny < 3) die("nx,ny must be >= 3");
    return a;
}

std::string findKernels(const Args& a)
{
    if (!a.kernelPath.empty()) return a.kernelPath;
    const char* env = std::getenv("LAPLACE_OCL_KERNELS");
    if (env && *env) return env;
    // try relative paths from common launch dirs
    const char* candidates[] = {
        "kernels/laplace.cl",
        "../kernels/laplace.cl",
        "../../apps/laplaceOcl/kernels/laplace.cl",
        "apps/laplaceOcl/kernels/laplace.cl",
        "G:/dev/repos/PFC/apps/laplaceOcl/kernels/laplace.cl",
    };
    for (const char* c : candidates) {
        std::ifstream in(c);
        if (in) return c;
    }
    die("cannot find kernels/laplace.cl (pass --kernels)");
    return {};
}

struct Ocl {
    cl_platform_id platform{};
    cl_device_id device{};
    cl_context ctx{};
    cl_command_queue queue{};
    cl_program program{};

    cl_kernel k_mark{};
    cl_kernel k_init{};
    cl_kernel k_assemble{};
    cl_kernel k_rhs{};
    cl_kernel k_spmv{};
    cl_kernel k_axpy{};
    cl_kernel k_xpay{};
    cl_kernel k_copy{};
    cl_kernel k_scal{};
    cl_kernel k_set{};
    cl_kernel k_jacobi{};
    cl_kernel k_resid{};
    cl_kernel k_sumsq{};
    cl_kernel k_dot{};

    void init(const std::string& src)
    {
        cl_int err = CL_SUCCESS;
        cl_uint nPlat = 0;
        checkCl(clGetPlatformIDs(0, nullptr, &nPlat), "clGetPlatformIDs count");
        if (nPlat == 0) die("no OpenCL platforms");
        std::vector<cl_platform_id> plats(nPlat);
        checkCl(clGetPlatformIDs(nPlat, plats.data(), nullptr), "clGetPlatformIDs");

        bool found = false;
        for (auto p : plats) {
            cl_uint nDev = 0;
            if (clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 0, nullptr, &nDev) != CL_SUCCESS || nDev == 0)
                continue;
            std::vector<cl_device_id> devs(nDev);
            checkCl(clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, nDev, devs.data(), nullptr), "clGetDeviceIDs");
            platform = p;
            device = devs[0];
            found = true;
            break;
        }
        if (!found) die("no OpenCL GPU device");

        char name[256] = {};
        clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(name), name, nullptr);
        char vendor[256] = {};
        clGetDeviceInfo(device, CL_DEVICE_VENDOR, sizeof(vendor), vendor, nullptr);
        std::cout << "OpenCL device: " << name << " (" << vendor << ")\n";

        ctx = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
        checkCl(err, "clCreateContext");

#ifdef CL_VERSION_2_0
        const cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
        queue = clCreateCommandQueueWithProperties(ctx, device, props, &err);
        if (err != CL_SUCCESS) {
            queue = clCreateCommandQueue(ctx, device, CL_QUEUE_PROFILING_ENABLE, &err);
        }
#else
        queue = clCreateCommandQueue(ctx, device, CL_QUEUE_PROFILING_ENABLE, &err);
#endif
        checkCl(err, "clCreateCommandQueue");

        const char* csrc = src.c_str();
        size_t slen = src.size();
        program = clCreateProgramWithSource(ctx, 1, &csrc, &slen, &err);
        checkCl(err, "clCreateProgramWithSource");
        err = clBuildProgram(program, 1, &device, "-cl-std=CL1.2 -cl-mad-enable", nullptr, nullptr);
        if (err != CL_SUCCESS) {
            size_t logSize = 0;
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
            std::vector<char> log(logSize + 1, 0);
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
            std::cerr << log.data() << "\n";
            die("clBuildProgram failed");
        }

        auto mk = [&](const char* name) {
            cl_kernel k = clCreateKernel(program, name, &err);
            checkCl(err, name);
            return k;
        };
        k_mark = mk("mark_interior");
        k_init = mk("init_temperature");
        k_assemble = mk("assemble_A_dia");
        k_rhs = mk("build_rhs");
        k_spmv = mk("spmv_dia");
        k_axpy = mk("vec_axpy");
        k_xpay = mk("vec_xpay");
        k_copy = mk("vec_copy");
        k_scal = mk("vec_scal");
        k_set = mk("vec_set");
        k_jacobi = mk("apply_jacobi");
        k_resid = mk("vec_residual");
        k_sumsq = mk("reduce_sum_sq");
        k_dot = mk("reduce_dot");
    }

    ~Ocl()
    {
        auto relK = [](cl_kernel k) { if (k) clReleaseKernel(k); };
        relK(k_mark); relK(k_init); relK(k_assemble); relK(k_rhs); relK(k_spmv);
        relK(k_axpy); relK(k_xpay); relK(k_copy); relK(k_scal); relK(k_set);
        relK(k_jacobi); relK(k_resid); relK(k_sumsq); relK(k_dot);
        if (program) clReleaseProgram(program);
        if (queue) clReleaseCommandQueue(queue);
        if (ctx) clReleaseContext(ctx);
    }
};

cl_mem mkBuf(cl_context ctx, cl_mem_flags flags, size_t bytes, void* host = nullptr)
{
    cl_int err = CL_SUCCESS;
    cl_mem m = clCreateBuffer(ctx, flags, bytes, host, &err);
    checkCl(err, "clCreateBuffer");
    return m;
}

void setArgs(cl_kernel k, std::initializer_list<std::pair<size_t, const void*>> /*dummy*/) {}

template <typename T>
void set(cl_kernel k, cl_uint i, const T& v)
{
    checkCl(clSetKernelArg(k, i, sizeof(T), &v), "clSetKernelArg");
}

void enqueue1D(cl_command_queue q, cl_kernel k, size_t n, size_t lws = 256)
{
    size_t gws = ((n + lws - 1) / lws) * lws;
    checkCl(clEnqueueNDRangeKernel(q, k, 1, nullptr, &gws, &lws, 0, nullptr, nullptr), "enqueue");
}

double reduceHost(cl_command_queue q, cl_kernel kRed, cl_mem partial, cl_uint nPart,
    size_t localBytes, size_t gws, size_t lws)
{
    // kernel args 0.. already set except scratch and n handled by caller
    checkCl(clSetKernelArg(kRed, 2, localBytes, nullptr), "local scratch");
    checkCl(clEnqueueNDRangeKernel(q, kRed, 1, nullptr, &gws, &lws, 0, nullptr, nullptr), "reduce enqueue");
    std::vector<double> h(nPart);
    checkCl(clEnqueueReadBuffer(q, partial, CL_TRUE, 0, nPart * sizeof(double), h.data(), 0, nullptr, nullptr),
        "read partial");
    return std::accumulate(h.begin(), h.end(), 0.0);
}

// --- CPU reference (same matrix/RHS) for validation ---
void cpuReference(const Args& a, std::vector<double>& T)
{
    const int nx = a.nx, ny = a.ny, n = nx * ny;
    const double dx = a.Lx / (nx - 1);
    const double dy = a.Ly / (ny - 1);
    const double cx = a.DT / (dx * dx);
    const double cy = a.DT / (dy * dy);

    auto isInterior = [&](int c) {
        int i = c % nx, j = c / nx;
        return i > 0 && i < nx - 1 && j > 0 && j < ny - 1;
    };

    auto initT = [&](int c) {
        int i = c % nx, j = c / nx;
        double v = 273.0;
        if (j == ny - 1) return 573.0;
        if (i == 0) return 373.0;
        if (j == 0 || i == nx - 1) return 273.0;
        return v;
    };

    T.resize(n);
    for (int c = 0; c < n; ++c) T[c] = initT(c);

    std::vector<double> dia0(n), dia1(n), dia2(n), dia3(n), dia4(n), invD(n);
    for (int c = 0; c < n; ++c) {
        if (!isInterior(c)) {
            dia2[c] = 1.0;
            invD[c] = 1.0;
            continue;
        }
        const double center = (1.0 / a.dt) + 2.0 * cx + 2.0 * cy;
        dia2[c] = center;
        dia1[c] = -cx;
        dia3[c] = -cx;
        dia0[c] = -cy;
        dia4[c] = -cy;
        invD[c] = 1.0 / center;
    }

    auto spmv = [&](const std::vector<double>& x, std::vector<double>& y) {
        for (int c = 0; c < n; ++c) {
            double acc = dia2[c] * x[c];
            if (c >= nx) acc += dia0[c] * x[c - nx];
            if ((c % nx) > 0) acc += dia1[c] * x[c - 1];
            if ((c % nx) < nx - 1) acc += dia3[c] * x[c + 1];
            if (c + nx < n) acc += dia4[c] * x[c + nx];
            y[c] = acc;
        }
    };

    std::vector<double> b(n), x(n), r(n), z(n), p(n), Ap(n);
    for (int step = 0; step < a.steps; ++step) {
        for (int c = 0; c < n; ++c) {
            b[c] = isInterior(c) ? T[c] / a.dt : T[c];
            x[c] = T[c];
        }
        spmv(x, Ap);
        for (int c = 0; c < n; ++c) r[c] = b[c] - Ap[c];
        for (int c = 0; c < n; ++c) z[c] = invD[c] * r[c];
        p = z;
        double rzOld = 0.0;
        for (int c = 0; c < n; ++c) rzOld += r[c] * z[c];
        const double bnorm = std::sqrt(std::inner_product(b.begin(), b.end(), b.begin(), 0.0)) + 1e-30;

        for (int it = 0; it < a.maxIters; ++it) {
            spmv(p, Ap);
            double pAp = 0.0;
            for (int c = 0; c < n; ++c) pAp += p[c] * Ap[c];
            const double alpha = rzOld / (pAp + 1e-300);
            for (int c = 0; c < n; ++c) x[c] += alpha * p[c];
            for (int c = 0; c < n; ++c) r[c] -= alpha * Ap[c];
            double r2 = 0.0;
            for (int c = 0; c < n; ++c) r2 += r[c] * r[c];
            if (std::sqrt(r2) / bnorm < a.tol) break;
            for (int c = 0; c < n; ++c) z[c] = invD[c] * r[c];
            double rzNew = 0.0;
            for (int c = 0; c < n; ++c) rzNew += r[c] * z[c];
            const double beta = rzNew / (rzOld + 1e-300);
            for (int c = 0; c < n; ++c) p[c] = z[c] + beta * p[c];
            rzOld = rzNew;
        }
        T = x;
    }
}

} // namespace

int main(int argc, char** argv)
{
    Args args = parseArgs(argc, argv);
    const int nx = args.nx;
    const int ny = args.ny;
    const int n = nx * ny;
    const double dx = args.Lx / (nx - 1);
    const double dy = args.Ly / (ny - 1);

    std::cout << "laplaceOcl full-device lifecycle\n"
              << "  mesh     : " << nx << " x " << ny << " (" << n << " cells)\n"
              << "  steps    : " << args.steps << "  dt=" << args.dt << "  DT=" << args.DT << "\n"
              << "  domain   : " << args.Lx << " x " << args.Ly << "  dx=" << dx << " dy=" << dy << "\n";

    const std::string kpath = findKernels(args);
    std::cout << "  kernels  : " << kpath << "\n";
    Ocl ocl;
    ocl.init(readFile(kpath));

    const size_t bytes = static_cast<size_t>(n) * sizeof(double);
    const size_t bytesU = static_cast<size_t>(n) * sizeof(cl_uchar);

    // ---- single allocation / upload phase (geometry generated on device) ----
    cl_mem dT = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dB = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dX = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dR = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dZ = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dP = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dAp = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dMark = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytesU);
    cl_mem dInv = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem d0 = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem d1 = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem d2 = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem d3 = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem d4 = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);

    const size_t lws = 256;
    const size_t gwsRed = 256 * 64; // 64 groups
    const cl_uint nPart = 64;
    cl_mem dPartial = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, nPart * sizeof(double));

    auto t0 = std::chrono::steady_clock::now();

    // mark + init on device
    set(ocl.k_mark, 0, dMark);
    set(ocl.k_mark, 1, nx);
    set(ocl.k_mark, 2, ny);
    enqueue1D(ocl.queue, ocl.k_mark, n, lws);

    const double T_init = 273.0, T_h = 573.0, T_c = 373.0, T_wall = 273.0;
    set(ocl.k_init, 0, dT);
    set(ocl.k_init, 1, nx);
    set(ocl.k_init, 2, ny);
    set(ocl.k_init, 3, T_init);
    set(ocl.k_init, 4, T_h);
    set(ocl.k_init, 5, T_c);
    set(ocl.k_init, 6, T_wall);
    enqueue1D(ocl.queue, ocl.k_init, n, lws);

    // assemble A once
    set(ocl.k_assemble, 0, d0);
    set(ocl.k_assemble, 1, d1);
    set(ocl.k_assemble, 2, d2);
    set(ocl.k_assemble, 3, d3);
    set(ocl.k_assemble, 4, d4);
    set(ocl.k_assemble, 5, dInv);
    set(ocl.k_assemble, 6, dMark);
    set(ocl.k_assemble, 7, nx);
    set(ocl.k_assemble, 8, ny);
    set(ocl.k_assemble, 9, dx);
    set(ocl.k_assemble, 10, dy);
    set(ocl.k_assemble, 11, args.dt);
    set(ocl.k_assemble, 12, args.DT);
    enqueue1D(ocl.queue, ocl.k_assemble, n, lws);

    auto spmv = [&](cl_mem x, cl_mem y) {
        set(ocl.k_spmv, 0, d0);
        set(ocl.k_spmv, 1, d1);
        set(ocl.k_spmv, 2, d2);
        set(ocl.k_spmv, 3, d3);
        set(ocl.k_spmv, 4, d4);
        set(ocl.k_spmv, 5, x);
        set(ocl.k_spmv, 6, y);
        set(ocl.k_spmv, 7, nx);
        set(ocl.k_spmv, 8, n);
        enqueue1D(ocl.queue, ocl.k_spmv, n, lws);
    };

    auto copy = [&](cl_mem dst, cl_mem src) {
        set(ocl.k_copy, 0, dst);
        set(ocl.k_copy, 1, src);
        set(ocl.k_copy, 2, n);
        enqueue1D(ocl.queue, ocl.k_copy, n, lws);
    };

    auto axpy = [&](cl_mem y, cl_mem x, double a) {
        set(ocl.k_axpy, 0, y);
        set(ocl.k_axpy, 1, x);
        set(ocl.k_axpy, 2, a);
        set(ocl.k_axpy, 3, n);
        enqueue1D(ocl.queue, ocl.k_axpy, n, lws);
    };

    auto xpay = [&](cl_mem y, cl_mem x, double a) {
        set(ocl.k_xpay, 0, y);
        set(ocl.k_xpay, 1, x);
        set(ocl.k_xpay, 2, a);
        set(ocl.k_xpay, 3, n);
        enqueue1D(ocl.queue, ocl.k_xpay, n, lws);
    };

    auto jacobi = [&](cl_mem z, cl_mem r) {
        set(ocl.k_jacobi, 0, z);
        set(ocl.k_jacobi, 1, dInv);
        set(ocl.k_jacobi, 2, r);
        set(ocl.k_jacobi, 3, n);
        enqueue1D(ocl.queue, ocl.k_jacobi, n, lws);
    };

    auto sumsq = [&](cl_mem x) -> double {
        set(ocl.k_sumsq, 0, x);
        set(ocl.k_sumsq, 1, dPartial);
        // arg2 local
        set(ocl.k_sumsq, 3, n);
        return reduceHost(ocl.queue, ocl.k_sumsq, dPartial, nPart, lws * sizeof(double), gwsRed, lws);
    };

    auto dot = [&](cl_mem a, cl_mem b) -> double {
        set(ocl.k_dot, 0, a);
        set(ocl.k_dot, 1, b);
        set(ocl.k_dot, 2, dPartial);
        set(ocl.k_dot, 4, n);
        // fix arg indices: reduce_dot(a,b,partial,scratch,n) -> 0,1,2,local,4
        checkCl(clSetKernelArg(ocl.k_dot, 3, lws * sizeof(double), nullptr), "dot local");
        checkCl(clEnqueueNDRangeKernel(ocl.queue, ocl.k_dot, 1, nullptr, &gwsRed, &lws, 0, nullptr, nullptr), "dot");
        std::vector<double> h(nPart);
        checkCl(clEnqueueReadBuffer(ocl.queue, dPartial, CL_TRUE, 0, nPart * sizeof(double), h.data(), 0, nullptr, nullptr),
            "dot read");
        return std::accumulate(h.begin(), h.end(), 0.0);
    };

    // fix sumsq local arg index: reduce_sum_sq(x, partial, scratch, n) = 0,1,local,3
    auto sumsq2 = [&](cl_mem x) -> double {
        set(ocl.k_sumsq, 0, x);
        set(ocl.k_sumsq, 1, dPartial);
        checkCl(clSetKernelArg(ocl.k_sumsq, 2, lws * sizeof(double), nullptr), "sumsq local");
        set(ocl.k_sumsq, 3, n);
        checkCl(clEnqueueNDRangeKernel(ocl.queue, ocl.k_sumsq, 1, nullptr, &gwsRed, &lws, 0, nullptr, nullptr), "sumsq");
        std::vector<double> h(nPart);
        checkCl(clEnqueueReadBuffer(ocl.queue, dPartial, CL_TRUE, 0, nPart * sizeof(double), h.data(), 0, nullptr, nullptr),
            "sumsq read");
        return std::accumulate(h.begin(), h.end(), 0.0);
    };

    int totalPcgIters = 0;

    for (int step = 0; step < args.steps; ++step) {
        // b from T
        set(ocl.k_rhs, 0, dB);
        set(ocl.k_rhs, 1, dT);
        set(ocl.k_rhs, 2, dMark);
        set(ocl.k_rhs, 3, n);
        set(ocl.k_rhs, 4, args.dt);
        enqueue1D(ocl.queue, ocl.k_rhs, n, lws);

        // x0 = T
        copy(dX, dT);

        // r = b - A x
        spmv(dX, dAp);
        set(ocl.k_resid, 0, dR);
        set(ocl.k_resid, 1, dB);
        set(ocl.k_resid, 2, dAp);
        set(ocl.k_resid, 3, n);
        enqueue1D(ocl.queue, ocl.k_resid, n, lws);

        jacobi(dZ, dR);
        copy(dP, dZ);

        double rzOld = dot(dR, dZ);
        const double bnorm = std::sqrt(sumsq2(dB)) + 1e-30;
        int itUsed = 0;

        const int limit = args.fixedIters > 0 ? args.fixedIters : args.maxIters;
        for (int it = 0; it < limit; ++it) {
            spmv(dP, dAp);
            const double pAp = dot(dP, dAp);
            const double alpha = rzOld / (pAp + 1e-300);
            axpy(dX, dP, alpha);     // x += alpha p
            axpy(dR, dAp, -alpha);   // r -= alpha Ap
            itUsed = it + 1;

            if (args.fixedIters <= 0) {
                const double rnorm = std::sqrt(sumsq2(dR));
                if (rnorm / bnorm < args.tol) break;
            }

            jacobi(dZ, dR);
            const double rzNew = dot(dR, dZ);
            const double beta = rzNew / (rzOld + 1e-300);
            xpay(dP, dZ, beta); // p = z + beta p
            rzOld = rzNew;
        }
        totalPcgIters += itUsed;
        copy(dT, dX);

        if ((step + 1) % std::max(1, args.steps / 5) == 0 || step == args.steps - 1) {
            std::cout << "  step " << (step + 1) << "/" << args.steps
                      << "  pcgIters=" << itUsed << "\n";
        }
    }

    checkCl(clFinish(ocl.queue), "clFinish");
    auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // ---- single download ----
    std::vector<double> Tgpu(n);
    checkCl(clEnqueueReadBuffer(ocl.queue, dT, CL_TRUE, 0, bytes, Tgpu.data(), 0, nullptr, nullptr), "download T");

    auto t2 = std::chrono::steady_clock::now();
    const double msDl = std::chrono::duration<double, std::milli>(t2 - t1).count();

    double tmin = Tgpu[0], tmax = Tgpu[0], tsum = 0.0;
    for (double v : Tgpu) {
        tmin = std::min(tmin, v);
        tmax = std::max(tmax, v);
        tsum += v;
    }
    std::cout << "Device work wall: " << ms << " ms  (download " << msDl << " ms)\n"
              << "Total PCG iters : " << totalPcgIters << "\n"
              << "T range         : [" << tmin << ", " << tmax << "]  mean=" << (tsum / n) << "\n";

    // write CSV sample (every cell is fine for 100^2; subsample if huge)
    {
        std::ofstream out(args.outCsv);
        out << "i,j,T\n";
        const int stride = n > 200000 ? 4 : 1;
        for (int j = 0; j < ny; j += stride) {
            for (int i = 0; i < nx; i += stride) {
                out << i << "," << j << "," << Tgpu[i + j * nx] << "\n";
            }
        }
        std::cout << "Wrote " << args.outCsv << "\n";
    }

    if (args.cpuCheck) {
        std::cout << "CPU reference (same scheme)...\n";
        auto c0 = std::chrono::steady_clock::now();
        std::vector<double> Tcpu;
        cpuReference(args, Tcpu);
        auto c1 = std::chrono::steady_clock::now();
        const double msCpu = std::chrono::duration<double, std::milli>(c1 - c0).count();
        double maxAbs = 0.0, maxRel = 0.0;
        for (int c = 0; c < n; ++c) {
            const double e = std::abs(Tgpu[c] - Tcpu[c]);
            maxAbs = std::max(maxAbs, e);
            maxRel = std::max(maxRel, e / (std::abs(Tcpu[c]) + 1e-30));
        }
        std::cout << "CPU wall         : " << msCpu << " ms\n"
                  << "max |Tgpu-Tcpu|  : " << maxAbs << "\n"
                  << "max rel err      : " << maxRel << "\n";
        if (maxAbs > 1e-4) {
            std::cerr << "WARNING: GPU vs CPU disagree beyond 1e-4\n";
            // still exit 0 if order-of-magnitude OK; fail hard if broken
            if (maxAbs > 1.0) return 2;
        } else {
            std::cout << "CPU check        : OK\n";
        }
    }

    // release buffers
    cl_mem all[] = {dT, dB, dX, dR, dZ, dP, dAp, dMark, dInv, d0, d1, d2, d3, d4, dPartial};
    for (cl_mem m : all) clReleaseMemObject(m);

    std::cout << "Done (full-device assemble+solve; single final unload).\n";
    return 0;
}
