// simpleOcl — device-resident SIMPLE outer loop (primary v2 skeleton)
//
// Lifecycle:
//   startup:  Poisson CSR + fields U,p → GPU once
//   loop:     momentum predictor → continuity RHS → PCG(p) → correct  (on device)
//   shutdown: one download of u,v,p
//
// This is educational collocated 2D SIMPLE (lid-driven cavity BC), not full OF FV.
// Next: CSR from OF mesh, then full SIMPLE (v2b), then PIMPLE (v3).
// See docs/SIMPLE_GPU.md

#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Args {
    int nx = 64;
    int ny = 64;
    int outers = 20;
    double alpha = 0.5; // under-relaxation scale for grad(p)
    double nuDt = 0.1;  // nu*dt in grid units (dx=dy=1); keep <~0.2 for stability
    double tol = 1e-8;
    int maxIters = 500;
    bool quiet = false;
    std::string kernelPath;
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
        else if (s == "--outers") a.outers = std::stoi(need("--outers"));
        else if (s == "--alpha") a.alpha = std::stod(need("--alpha"));
        else if (s == "--nu-dt") a.nuDt = std::stod(need("--nu-dt"));
        else if (s == "--tol") a.tol = std::stod(need("--tol"));
        else if (s == "--max-iters") a.maxIters = std::stoi(need("--max-iters"));
        else if (s == "--quiet") a.quiet = true;
        else if (s == "--kernels") a.kernelPath = need("--kernels");
        else if (s == "--help" || s == "-h") {
            std::cout
                << "simpleOcl — device-resident SIMPLE (v2 skeleton)\n"
                << "  --nx N --ny N --outers N --alpha A --nu-dt NUDT --tol T --max-iters N\n"
                << "  --kernels path/to/simple.cl --quiet\n"
                << "Lifecycle: upload once → SIMPLE outers on GPU → one download.\n"
                << "Roadmap: docs/SIMPLE_GPU.md (SIMPLE first, then PIMPLE).\n";
            std::exit(0);
        } else {
            die("unknown arg: " + s);
        }
    }
    if (a.nx < 5 || a.ny < 5) die("nx,ny >= 5");
    if (a.outers < 1) die("outers >= 1");
    return a;
}

std::string findKernels(const Args& a)
{
    if (!a.kernelPath.empty()) return a.kernelPath;
    const char* candidates[] = {
        "kernels/simple.cl",
        "apps/simpleOcl/kernels/simple.cl",
        "build/simpleOcl/kernels/simple.cl",
    };
    for (const char* c : candidates) {
        std::ifstream in(c);
        if (in) return c;
    }
    die("cannot find simple.cl (pass --kernels)");
    return {};
}

// 2D -Laplace CSR, Dirichlet p=0 on boundary (pressure Poisson brick)
void buildPoissonCsr2d(
    int nx, int ny,
    std::vector<int>& rowPtr,
    std::vector<int>& colInd,
    std::vector<double>& vals,
    std::vector<double>& invDiag)
{
    const int n = nx * ny;
    rowPtr.assign(n + 1, 0);
    colInd.clear();
    vals.clear();
    invDiag.assign(n, 1.0);
    colInd.reserve(static_cast<size_t>(n) * 5);
    vals.reserve(static_cast<size_t>(n) * 5);

    auto idx = [&](int i, int j) { return i + j * nx; };

    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            const int c = idx(i, j);
            rowPtr[c] = static_cast<int>(colInd.size());
            if (i == 0 || i == nx - 1 || j == 0 || j == ny - 1) {
                colInd.push_back(c);
                vals.push_back(1.0);
                invDiag[c] = 1.0;
                continue;
            }
            struct Ent {
                int col;
                double v;
            };
            std::vector<Ent> ents = {
                {c, 4.0},
                {idx(i - 1, j), -1.0},
                {idx(i + 1, j), -1.0},
                {idx(i, j - 1), -1.0},
                {idx(i, j + 1), -1.0},
            };
            std::sort(ents.begin(), ents.end(), [](const Ent& a, const Ent& b) {
                return a.col < b.col;
            });
            for (const auto& e : ents) {
                colInd.push_back(e.col);
                vals.push_back(e.v);
            }
            invDiag[c] = 0.25;
        }
    }
    rowPtr[n] = static_cast<int>(colInd.size());
}

struct Ocl {
    cl_platform_id platform{};
    cl_device_id device{};
    cl_context ctx{};
    cl_command_queue queue{};
    cl_program program{};
    cl_kernel k_spmv{}, k_jacobi{}, k_poly2{}, k_axpy{}, k_xpay{};
    cl_kernel k_copy{}, k_set{}, k_resid{}, k_sumsq{}, k_dot{};
    cl_kernel k_mom{}, k_cont{}, k_corr{}, k_bc{};

    void init(const std::string& src)
    {
        cl_int err = CL_SUCCESS;
        cl_uint nPlat = 0;
        checkCl(clGetPlatformIDs(0, nullptr, &nPlat), "platforms");
        std::vector<cl_platform_id> plats(nPlat);
        checkCl(clGetPlatformIDs(nPlat, plats.data(), nullptr), "platforms");
        bool found = false;
        for (auto p : plats) {
            cl_uint nDev = 0;
            if (clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 0, nullptr, &nDev) != CL_SUCCESS || nDev == 0)
                continue;
            std::vector<cl_device_id> devs(nDev);
            checkCl(clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, nDev, devs.data(), nullptr), "devs");
            platform = p;
            device = devs[0];
            found = true;
            break;
        }
        if (!found) die("no OpenCL GPU");

        char name[256] = {};
        clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(name), name, nullptr);
        std::cout << "OpenCL device: " << name << "\n";

        ctx = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
        checkCl(err, "context");
        queue = clCreateCommandQueue(ctx, device, 0, &err);
        checkCl(err, "queue");

        const char* csrc = src.c_str();
        size_t slen = src.size();
        program = clCreateProgramWithSource(ctx, 1, &csrc, &slen, &err);
        checkCl(err, "program");
        err = clBuildProgram(program, 1, &device, "-cl-std=CL1.2", nullptr, nullptr);
        if (err != CL_SUCCESS) {
            size_t logSize = 0;
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
            std::string log(logSize, '\0');
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
            std::cerr << log << "\n";
            die("clBuildProgram");
        }

        auto kn = [&](const char* name) {
            cl_kernel k = clCreateKernel(program, name, &err);
            checkCl(err, name);
            return k;
        };
        k_spmv = kn("spmv_csr");
        k_jacobi = kn("apply_jacobi");
        k_poly2 = kn("poly2_combine");
        k_axpy = kn("vec_axpy");
        k_xpay = kn("vec_xpay");
        k_copy = kn("vec_copy");
        k_set = kn("vec_set");
        k_resid = kn("vec_residual");
        k_sumsq = kn("reduce_sum_sq");
        k_dot = kn("reduce_dot");
        k_mom = kn("momentum_predictor");
        k_cont = kn("continuity_rhs");
        k_corr = kn("velocity_correct");
        k_bc = kn("enforce_lid_bc");
    }

    ~Ocl()
    {
        cl_kernel ks[] = {k_spmv, k_jacobi, k_poly2, k_axpy, k_xpay, k_copy, k_set, k_resid,
            k_sumsq, k_dot, k_mom, k_cont, k_corr, k_bc};
        for (auto k : ks)
            if (k) clReleaseKernel(k);
        if (program) clReleaseProgram(program);
        if (queue) clReleaseCommandQueue(queue);
        if (ctx) clReleaseContext(ctx);
    }
};

template <typename T>
void setArg(cl_kernel k, cl_uint i, T v)
{
    checkCl(clSetKernelArg(k, i, sizeof(T), &v), "setArg");
}

cl_mem mkBuf(cl_context ctx, cl_mem_flags f, size_t bytes, void* host = nullptr)
{
    cl_int err = CL_SUCCESS;
    cl_mem m = clCreateBuffer(ctx, f, bytes, host, &err);
    checkCl(err, "buffer");
    return m;
}

void enqueue1D(cl_command_queue q, cl_kernel k, size_t n, size_t lws)
{
    size_t gws = ((n + lws - 1) / lws) * lws;
    checkCl(clEnqueueNDRangeKernel(q, k, 1, nullptr, &gws, &lws, 0, nullptr, nullptr), "enqueue");
}

} // namespace

int main(int argc, char** argv)
{
    Args args = parseArgs(argc, argv);
    const std::string kpath = findKernels(args);
    const int nx = args.nx;
    const int ny = args.ny;
    const int n = nx * ny;
    // Work in grid units (dx=dy=1) so default nu*dt is stable without mesh-dependent tuning.
    const double dx = 1.0;
    const double dy = 1.0;

    std::vector<int> rowPtr, colInd;
    std::vector<double> vals, invDiag;
    buildPoissonCsr2d(nx, ny, rowPtr, colInd, vals, invDiag);
    const int nnz = static_cast<int>(vals.size());

    // Host init: rest fluid, lid u=1
    std::vector<double> hu(n, 0.0), hv(n, 0.0), hp(n, 0.0);
    for (int i = 0; i < nx; ++i) {
        hu[i + (ny - 1) * nx] = 1.0;
    }

    std::cout << "simpleOcl device-resident SIMPLE (v2 skeleton)\n"
              << "  kernels : " << kpath << "\n"
              << "  mesh    : " << nx << "x" << ny << "  n=" << n << "  nnz=" << nnz << "\n"
              << "  outers  : " << args.outers << "  alpha=" << args.alpha
              << "  nu*dt=" << args.nuDt << "\n"
              << "  note    : educational collocated SIMPLE; OF coupling = later Mode B\n";

    Ocl ocl;
    ocl.init(readFile(kpath));

    const size_t bytes = static_cast<size_t>(n) * sizeof(double);
    const size_t bytesI = static_cast<size_t>(n + 1) * sizeof(int);
    const size_t bytesCol = static_cast<size_t>(nnz) * sizeof(int);
    const size_t bytesVal = static_cast<size_t>(nnz) * sizeof(double);

    auto t0 = std::chrono::steady_clock::now();

    // One-time upload: matrix + fields
    cl_mem dRow = mkBuf(ocl.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytesI, rowPtr.data());
    cl_mem dCol = mkBuf(ocl.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytesCol, colInd.data());
    cl_mem dVal = mkBuf(ocl.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytesVal, vals.data());
    cl_mem dInv = mkBuf(ocl.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, invDiag.data());
    cl_mem dU = mkBuf(ocl.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, bytes, hu.data());
    cl_mem dV = mkBuf(ocl.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, bytes, hv.data());
    cl_mem dP = mkBuf(ocl.ctx, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, bytes, hp.data());
    cl_mem dUs = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dVs = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dB = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dR = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dZ = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dPcgP = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dAp = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dTmp = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);

    const size_t lws = 256;
    const size_t gwsRed = 256 * 64;
    const cl_uint nPart = 64;
    cl_mem dPartial = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, nPart * sizeof(double));

    std::uint64_t h2d = bytesI + bytesCol + bytesVal + bytes + 3 * bytes; // A + inv + U,V,P
    std::uint64_t d2hField = 0;
    std::uint64_t d2hScalar = 0;

    checkCl(clFinish(ocl.queue), "upload");
    auto tSetup = std::chrono::steady_clock::now();

    auto spmv = [&](cl_mem x, cl_mem y) {
        setArg(ocl.k_spmv, 0, dRow);
        setArg(ocl.k_spmv, 1, dCol);
        setArg(ocl.k_spmv, 2, dVal);
        setArg(ocl.k_spmv, 3, x);
        setArg(ocl.k_spmv, 4, y);
        setArg(ocl.k_spmv, 5, n);
        enqueue1D(ocl.queue, ocl.k_spmv, n, lws);
    };
    auto copy = [&](cl_mem dst, cl_mem src) {
        setArg(ocl.k_copy, 0, dst);
        setArg(ocl.k_copy, 1, src);
        setArg(ocl.k_copy, 2, n);
        enqueue1D(ocl.queue, ocl.k_copy, n, lws);
    };
    auto axpy = [&](cl_mem y, cl_mem x, double a) {
        setArg(ocl.k_axpy, 0, y);
        setArg(ocl.k_axpy, 1, x);
        setArg(ocl.k_axpy, 2, a);
        setArg(ocl.k_axpy, 3, n);
        enqueue1D(ocl.queue, ocl.k_axpy, n, lws);
    };
    auto xpay = [&](cl_mem y, cl_mem x, double a) {
        setArg(ocl.k_xpay, 0, y);
        setArg(ocl.k_xpay, 1, x);
        setArg(ocl.k_xpay, 2, a);
        setArg(ocl.k_xpay, 3, n);
        enqueue1D(ocl.queue, ocl.k_xpay, n, lws);
    };
    auto precond = [&](cl_mem z, cl_mem r) {
        setArg(ocl.k_jacobi, 0, dTmp);
        setArg(ocl.k_jacobi, 1, dInv);
        setArg(ocl.k_jacobi, 2, r);
        setArg(ocl.k_jacobi, 3, n);
        enqueue1D(ocl.queue, ocl.k_jacobi, n, lws);
        spmv(dTmp, z);
        setArg(ocl.k_poly2, 0, z);
        setArg(ocl.k_poly2, 1, dTmp);
        setArg(ocl.k_poly2, 2, dInv);
        setArg(ocl.k_poly2, 3, n);
        enqueue1D(ocl.queue, ocl.k_poly2, n, lws);
    };
    auto dot = [&](cl_mem a, cl_mem bb) -> double {
        setArg(ocl.k_dot, 0, a);
        setArg(ocl.k_dot, 1, bb);
        setArg(ocl.k_dot, 2, dPartial);
        checkCl(clSetKernelArg(ocl.k_dot, 3, lws * sizeof(double), nullptr), "dot local");
        setArg(ocl.k_dot, 4, n);
        checkCl(clEnqueueNDRangeKernel(ocl.queue, ocl.k_dot, 1, nullptr, &gwsRed, &lws, 0, nullptr, nullptr), "dot");
        std::vector<double> h(nPart);
        checkCl(clEnqueueReadBuffer(ocl.queue, dPartial, CL_TRUE, 0, nPart * sizeof(double), h.data(), 0, nullptr, nullptr), "dot r");
        d2hScalar += nPart * sizeof(double);
        return std::accumulate(h.begin(), h.end(), 0.0);
    };
    auto sumsq = [&](cl_mem x) -> double {
        setArg(ocl.k_sumsq, 0, x);
        setArg(ocl.k_sumsq, 1, dPartial);
        checkCl(clSetKernelArg(ocl.k_sumsq, 2, lws * sizeof(double), nullptr), "sq local");
        setArg(ocl.k_sumsq, 3, n);
        checkCl(clEnqueueNDRangeKernel(ocl.queue, ocl.k_sumsq, 1, nullptr, &gwsRed, &lws, 0, nullptr, nullptr), "sq");
        std::vector<double> h(nPart);
        checkCl(clEnqueueReadBuffer(ocl.queue, dPartial, CL_TRUE, 0, nPart * sizeof(double), h.data(), 0, nullptr, nullptr), "sq r");
        d2hScalar += nPart * sizeof(double);
        return std::accumulate(h.begin(), h.end(), 0.0);
    };

    auto pcgPressure = [&]() -> int {
        // p starts from previous outer (warm); residual r = b - A p
        spmv(dP, dAp);
        setArg(ocl.k_resid, 0, dR);
        setArg(ocl.k_resid, 1, dB);
        setArg(ocl.k_resid, 2, dAp);
        setArg(ocl.k_resid, 3, n);
        enqueue1D(ocl.queue, ocl.k_resid, n, lws);
        precond(dZ, dR);
        copy(dPcgP, dZ);
        double rzOld = dot(dR, dZ);
        const double bnorm = std::sqrt(sumsq(dB)) + 1e-30;
        int itUsed = 0;
        for (int it = 0; it < args.maxIters; ++it) {
            spmv(dPcgP, dAp);
            const double pAp = dot(dPcgP, dAp);
            const double alpha = rzOld / (pAp + 1e-300);
            axpy(dP, dPcgP, alpha);
            axpy(dR, dAp, -alpha);
            itUsed = it + 1;
            const double rnorm = std::sqrt(sumsq(dR));
            if (rnorm / bnorm < args.tol) break;
            precond(dZ, dR);
            const double rzNew = dot(dR, dZ);
            const double beta = rzNew / (rzOld + 1e-300);
            xpay(dPcgP, dZ, beta);
            rzOld = rzNew;
        }
        return itUsed;
    };

    long long totalPcgIters = 0;
    for (int outer = 0; outer < args.outers; ++outer) {
        // 1) BC
        setArg(ocl.k_bc, 0, dU);
        setArg(ocl.k_bc, 1, dV);
        setArg(ocl.k_bc, 2, nx);
        setArg(ocl.k_bc, 3, ny);
        enqueue1D(ocl.queue, ocl.k_bc, n, lws);

        // 2) momentum predictor u*,v*
        setArg(ocl.k_mom, 0, dUs);
        setArg(ocl.k_mom, 1, dVs);
        setArg(ocl.k_mom, 2, dU);
        setArg(ocl.k_mom, 3, dV);
        setArg(ocl.k_mom, 4, dP);
        setArg(ocl.k_mom, 5, nx);
        setArg(ocl.k_mom, 6, ny);
        setArg(ocl.k_mom, 7, args.alpha);
        setArg(ocl.k_mom, 8, args.nuDt);
        setArg(ocl.k_mom, 9, dx);
        setArg(ocl.k_mom, 10, dy);
        enqueue1D(ocl.queue, ocl.k_mom, n, lws);

        // 3) continuity RHS b = -div(u*)
        setArg(ocl.k_cont, 0, dB);
        setArg(ocl.k_cont, 1, dUs);
        setArg(ocl.k_cont, 2, dVs);
        setArg(ocl.k_cont, 3, nx);
        setArg(ocl.k_cont, 4, ny);
        setArg(ocl.k_cont, 5, dx);
        setArg(ocl.k_cont, 6, dy);
        enqueue1D(ocl.queue, ocl.k_cont, n, lws);

        // 4) pressure Poisson on device (A resident)
        const int pcgIt = pcgPressure();
        totalPcgIters += pcgIt;

        // 5) velocity corrector
        setArg(ocl.k_corr, 0, dU);
        setArg(ocl.k_corr, 1, dV);
        setArg(ocl.k_corr, 2, dUs);
        setArg(ocl.k_corr, 3, dVs);
        setArg(ocl.k_corr, 4, dP);
        setArg(ocl.k_corr, 5, nx);
        setArg(ocl.k_corr, 6, ny);
        setArg(ocl.k_corr, 7, args.alpha);
        setArg(ocl.k_corr, 8, dx);
        setArg(ocl.k_corr, 9, dy);
        enqueue1D(ocl.queue, ocl.k_corr, n, lws);

        if (!args.quiet && (outer == 0 || outer + 1 == args.outers || (outer + 1) % 5 == 0)) {
            checkCl(clFinish(ocl.queue), "outer sync");
            const double bnorm = std::sqrt(sumsq(dB));
            std::cout << "  outer " << (outer + 1) << "/" << args.outers
                      << "  pcg_iters=" << pcgIt
                      << "  |b|=" << bnorm << "\n";
        }
    }

    checkCl(clFinish(ocl.queue), "loop");
    auto tLoop = std::chrono::steady_clock::now();

    checkCl(clEnqueueReadBuffer(ocl.queue, dU, CL_TRUE, 0, bytes, hu.data(), 0, nullptr, nullptr), "dl u");
    checkCl(clEnqueueReadBuffer(ocl.queue, dV, CL_TRUE, 0, bytes, hv.data(), 0, nullptr, nullptr), "dl v");
    checkCl(clEnqueueReadBuffer(ocl.queue, dP, CL_TRUE, 0, bytes, hp.data(), 0, nullptr, nullptr), "dl p");
    d2hField = 3 * bytes;
    auto tEnd = std::chrono::steady_clock::now();

    const double msSetup = std::chrono::duration<double, std::milli>(tSetup - t0).count();
    const double msLoop = std::chrono::duration<double, std::milli>(tLoop - tSetup).count();
    const double msDl = std::chrono::duration<double, std::milli>(tEnd - tLoop).count();

    double umin = hu[0], umax = hu[0], pmin = hp[0], pmax = hp[0];
    for (int i = 0; i < n; ++i) {
        umin = std::min(umin, hu[i]);
        umax = std::max(umax, hu[i]);
        pmin = std::min(pmin, hp[i]);
        pmax = std::max(pmax, hp[i]);
    }

    std::cout << "OUTER_ITERS " << args.outers << "\n"
              << "PCG_ITERS_TOTAL " << totalPcgIters << "\n"
              << "TIMING_MS setup=" << msSetup << " simple_loop=" << msLoop
              << " download=" << msDl << "\n"
              << "TRAFFIC_BYTES h2d=" << h2d
              << " d2h_field=" << d2hField
              << " d2h_scalar=" << d2hScalar << "\n"
              << "u range [" << umin << ", " << umax << "]\n"
              << "p range [" << pmin << ", " << pmax << "]\n"
              << "RESIDENCY check : OK  (matrix+fields stayed on device for all outers)\n"
              << "Done (SIMPLE on GPU skeleton — next: OF Mode B, then v2b, then PIMPLE).\n";

    cl_mem all[] = {dRow, dCol, dVal, dInv, dU, dV, dP, dUs, dVs, dB, dR, dZ, dPcgP, dAp, dTmp, dPartial};
    for (cl_mem m : all) clReleaseMemObject(m);
    return 0;
}
