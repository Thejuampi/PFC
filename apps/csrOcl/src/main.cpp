// csrOcl — full-device OpenCL CSR SpMV + PCG (unstructured-ready format)
//
// Host builds a Poisson CSR once (structured connectivity, CSR storage),
// uploads matrix + b once, solves entirely on GPU, one x download.
// Next S5 step: replace host assemble with OpenFOAM LDU→CSR export.

#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
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
    int nx = 64;
    int ny = 64;
    int nz = 1;
    double tol = 1e-8;
    int maxIters = 2000;
    bool poly2 = true;
    bool cpuCheck = true;
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
        else if (s == "--nz") a.nz = std::stoi(need("--nz"));
        else if (s == "--tol") a.tol = std::stod(need("--tol"));
        else if (s == "--max-iters") a.maxIters = std::stoi(need("--max-iters"));
        else if (s == "--jacobi") a.poly2 = false;
        else if (s == "--poly2") a.poly2 = true;
        else if (s == "--no-cpu-check") a.cpuCheck = false;
        else if (s == "--kernels") a.kernelPath = need("--kernels");
        else if (s == "--help" || s == "-h") {
            std::cout
                << "csrOcl — device-resident CSR Poisson (OpenCL)\n"
                << "  --nx N --ny N --nz N   (nz=1 → 2D 5-pt; nz>=3 → 3D 7-pt as CSR)\n"
                << "  --tol T --max-iters N --poly2|--jacobi --no-cpu-check\n"
                << "  --kernels path/to/csr.cl\n"
                << "Lifecycle: host CSR assemble once → upload once → PCG on GPU → one x download.\n";
            std::exit(0);
        } else {
            die("unknown arg: " + s);
        }
    }
    if (a.nx < 3 || a.ny < 3) die("nx,ny >= 3");
    if (a.nz < 1 || (a.nz > 1 && a.nz < 3)) die("nz must be 1 or >= 3");
    return a;
}

std::string findKernels(const Args& a)
{
    if (!a.kernelPath.empty()) return a.kernelPath;
    const char* candidates[] = {
        "kernels/csr.cl",
        "../kernels/csr.cl",
        "apps/csrOcl/kernels/csr.cl",
        "G:/dev/repos/PFC/apps/csrOcl/kernels/csr.cl",
    };
    for (const char* c : candidates) {
        std::ifstream in(c);
        if (in) return c;
    }
    die("cannot find kernels/csr.cl (pass --kernels)");
    return {};
}

// Build -Laplace CSR with Dirichlet identity on boundary; b = 1 on interior.
void buildPoissonCsr(
    int nx, int ny, int nz,
    std::vector<int>& rowPtr,
    std::vector<int>& colInd,
    std::vector<double>& vals,
    std::vector<double>& invDiag,
    std::vector<double>& b)
{
    const int nxy = nx * ny;
    const int n = nxy * nz;
    rowPtr.assign(n + 1, 0);
    colInd.clear();
    vals.clear();
    invDiag.assign(n, 1.0);
    b.assign(n, 0.0);
    colInd.reserve(static_cast<size_t>(n) * 7);
    vals.reserve(static_cast<size_t>(n) * 7);

    auto idx = [&](int i, int j, int k) { return i + j * nx + k * nxy; };
    auto interior = [&](int i, int j, int k) {
        if (i <= 0 || i >= nx - 1 || j <= 0 || j >= ny - 1) return false;
        if (nz > 1 && (k <= 0 || k >= nz - 1)) return false;
        return true;
    };

    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                const int c = idx(i, j, k);
                rowPtr[c] = static_cast<int>(colInd.size());
                if (!interior(i, j, k)) {
                    colInd.push_back(c);
                    vals.push_back(1.0);
                    invDiag[c] = 1.0;
                    b[c] = 0.0; // Dirichlet u=0
                    continue;
                }
                // -Laplace stencil (SPD): center = 2*dim, off = -1
                const int dim = (nz > 1) ? 3 : 2;
                const double center = 2.0 * dim;
                // neighbors in order of increasing column for CSR hygiene
                struct Ent { int col; double v; };
                std::vector<Ent> ents;
                ents.push_back({c, center});
                if (i > 0) ents.push_back({idx(i - 1, j, k), -1.0});
                if (i < nx - 1) ents.push_back({idx(i + 1, j, k), -1.0});
                if (j > 0) ents.push_back({idx(i, j - 1, k), -1.0});
                if (j < ny - 1) ents.push_back({idx(i, j + 1, k), -1.0});
                if (nz > 1) {
                    if (k > 0) ents.push_back({idx(i, j, k - 1), -1.0});
                    if (k < nz - 1) ents.push_back({idx(i, j, k + 1), -1.0});
                }
                std::sort(ents.begin(), ents.end(), [](const Ent& a, const Ent& e) {
                    return a.col < e.col;
                });
                for (const auto& e : ents) {
                    colInd.push_back(e.col);
                    vals.push_back(e.v);
                }
                invDiag[c] = 1.0 / center;
                b[c] = 1.0; // unit RHS → nontrivial interior solution
            }
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
        err = clBuildProgram(program, 1, &device, "-cl-std=CL1.2 -cl-mad-enable", nullptr, nullptr);
        if (err != CL_SUCCESS) {
            size_t logSize = 0;
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
            std::vector<char> log(logSize + 1, 0);
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
            std::cerr << log.data() << "\n";
            die("build failed");
        }
        auto mk = [&](const char* n) {
            cl_kernel k = clCreateKernel(program, n, &err);
            checkCl(err, n);
            return k;
        };
        k_spmv = mk("spmv_csr");
        k_jacobi = mk("apply_jacobi");
        k_poly2 = mk("poly2_combine");
        k_axpy = mk("vec_axpy");
        k_xpay = mk("vec_xpay");
        k_copy = mk("vec_copy");
        k_set = mk("vec_set");
        k_resid = mk("vec_residual");
        k_sumsq = mk("reduce_sum_sq");
        k_dot = mk("reduce_dot");
    }

    ~Ocl()
    {
        auto rk = [](cl_kernel k) { if (k) clReleaseKernel(k); };
        rk(k_spmv); rk(k_jacobi); rk(k_poly2); rk(k_axpy); rk(k_xpay);
        rk(k_copy); rk(k_set); rk(k_resid); rk(k_sumsq); rk(k_dot);
        if (program) clReleaseProgram(program);
        if (queue) clReleaseCommandQueue(queue);
        if (ctx) clReleaseContext(ctx);
    }
};

template <typename T>
void set(cl_kernel k, cl_uint i, const T& v)
{
    checkCl(clSetKernelArg(k, i, sizeof(T), &v), "arg");
}

void enqueue1D(cl_command_queue q, cl_kernel k, size_t n, size_t lws = 256)
{
    size_t gws = ((n + lws - 1) / lws) * lws;
    checkCl(clEnqueueNDRangeKernel(q, k, 1, nullptr, &gws, &lws, 0, nullptr, nullptr), "ndr");
}

cl_mem mkBuf(cl_context ctx, cl_mem_flags flags, size_t bytes, void* host = nullptr)
{
    cl_int err = CL_SUCCESS;
    cl_mem m = clCreateBuffer(ctx, flags, bytes, host, &err);
    checkCl(err, "buffer");
    return m;
}

void cpuPcg(
    const std::vector<int>& rowPtr,
    const std::vector<int>& colInd,
    const std::vector<double>& vals,
    const std::vector<double>& invDiag,
    const std::vector<double>& b,
    std::vector<double>& x,
    bool poly2,
    double tol,
    int maxIters)
{
    const int n = static_cast<int>(b.size());
    auto spmv = [&](const std::vector<double>& v, std::vector<double>& y) {
        for (int i = 0; i < n; ++i) {
            double acc = 0.0;
            for (int k = rowPtr[i]; k < rowPtr[i + 1]; ++k)
                acc += vals[k] * v[colInd[k]];
            y[i] = acc;
        }
    };
    auto pre = [&](std::vector<double>& z, const std::vector<double>& r) {
        if (!poly2) {
            for (int i = 0; i < n; ++i) z[i] = invDiag[i] * r[i];
            return;
        }
        std::vector<double> t(n), w(n);
        for (int i = 0; i < n; ++i) t[i] = invDiag[i] * r[i];
        spmv(t, w);
        for (int i = 0; i < n; ++i) z[i] = 2.0 * t[i] - invDiag[i] * w[i];
    };

    x.assign(n, 0.0);
    std::vector<double> r(n), z(n), p(n), Ap(n);
    spmv(x, Ap);
    for (int i = 0; i < n; ++i) r[i] = b[i] - Ap[i];
    pre(z, r);
    p = z;
    double rzOld = std::inner_product(r.begin(), r.end(), z.begin(), 0.0);
    const double bnorm = std::sqrt(std::inner_product(b.begin(), b.end(), b.begin(), 0.0)) + 1e-30;
    for (int it = 0; it < maxIters; ++it) {
        spmv(p, Ap);
        double pAp = std::inner_product(p.begin(), p.end(), Ap.begin(), 0.0);
        const double alpha = rzOld / (pAp + 1e-300);
        for (int i = 0; i < n; ++i) {
            x[i] += alpha * p[i];
            r[i] -= alpha * Ap[i];
        }
        double r2 = std::inner_product(r.begin(), r.end(), r.begin(), 0.0);
        if (std::sqrt(r2) / bnorm < tol) break;
        pre(z, r);
        double rzNew = std::inner_product(r.begin(), r.end(), z.begin(), 0.0);
        const double beta = rzNew / (rzOld + 1e-300);
        for (int i = 0; i < n; ++i) p[i] = z[i] + beta * p[i];
        rzOld = rzNew;
    }
}

} // namespace

int main(int argc, char** argv)
{
    Args args = parseArgs(argc, argv);
    const std::string kpath = findKernels(args);

    std::vector<int> rowPtr, colInd;
    std::vector<double> vals, invDiag, b;
    buildPoissonCsr(args.nx, args.ny, args.nz, rowPtr, colInd, vals, invDiag, b);

    const int n = static_cast<int>(b.size());
    const int nnz = static_cast<int>(vals.size());
    std::cout << "csrOcl full-device CSR PCG\n"
              << "  kernels : " << kpath << "\n"
              << "  mesh    : " << args.nx << "x" << args.ny << "x" << args.nz
              << "  n=" << n << "  nnz=" << nnz
              << (args.nz == 1 ? " [2D CSR]" : " [3D CSR]") << "\n"
              << "  precond : " << (args.poly2 ? "poly2" : "jacobi") << "\n";

    Ocl ocl;
    ocl.init(readFile(kpath));

    const size_t bytes = static_cast<size_t>(n) * sizeof(double);
    const size_t bytesI = static_cast<size_t>(n + 1) * sizeof(int);
    const size_t bytesCol = static_cast<size_t>(nnz) * sizeof(int);
    const size_t bytesVal = static_cast<size_t>(nnz) * sizeof(double);

    auto t0 = std::chrono::steady_clock::now();

    // One-time upload of matrix + RHS + invDiag
    cl_mem dRow = mkBuf(ocl.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytesI, rowPtr.data());
    cl_mem dCol = mkBuf(ocl.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytesCol, colInd.data());
    cl_mem dVal = mkBuf(ocl.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytesVal, vals.data());
    cl_mem dInv = mkBuf(ocl.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, invDiag.data());
    cl_mem dB = mkBuf(ocl.ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, b.data());

    cl_mem dX = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dR = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dZ = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dP = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dAp = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);
    cl_mem dTmp = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);

    const size_t lws = 256;
    const size_t gwsRed = 256 * 64;
    const cl_uint nPart = 64;
    cl_mem dPartial = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, nPart * sizeof(double));

    std::uint64_t h2d = bytesI + bytesCol + bytesVal + bytes + bytes; // row,col,val,inv,b
    std::uint64_t d2hField = 0;
    std::uint64_t d2hScalar = 0;

    set(ocl.k_set, 0, dX);
    set(ocl.k_set, 1, 0.0);
    set(ocl.k_set, 2, n);
    enqueue1D(ocl.queue, ocl.k_set, n, lws);

    checkCl(clFinish(ocl.queue), "setup");
    auto tSetup = std::chrono::steady_clock::now();

    auto spmv = [&](cl_mem x, cl_mem y) {
        set(ocl.k_spmv, 0, dRow);
        set(ocl.k_spmv, 1, dCol);
        set(ocl.k_spmv, 2, dVal);
        set(ocl.k_spmv, 3, x);
        set(ocl.k_spmv, 4, y);
        set(ocl.k_spmv, 5, n);
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
    auto precond = [&](cl_mem z, cl_mem r) {
        if (!args.poly2) {
            jacobi(z, r);
            return;
        }
        jacobi(dTmp, r);
        spmv(dTmp, z);
        set(ocl.k_poly2, 0, z);
        set(ocl.k_poly2, 1, dTmp);
        set(ocl.k_poly2, 2, dInv);
        set(ocl.k_poly2, 3, n);
        enqueue1D(ocl.queue, ocl.k_poly2, n, lws);
    };
    auto dot = [&](cl_mem a, cl_mem bb) -> double {
        set(ocl.k_dot, 0, a);
        set(ocl.k_dot, 1, bb);
        set(ocl.k_dot, 2, dPartial);
        checkCl(clSetKernelArg(ocl.k_dot, 3, lws * sizeof(double), nullptr), "dot local");
        set(ocl.k_dot, 4, n);
        checkCl(clEnqueueNDRangeKernel(ocl.queue, ocl.k_dot, 1, nullptr, &gwsRed, &lws, 0, nullptr, nullptr), "dot");
        std::vector<double> h(nPart);
        checkCl(clEnqueueReadBuffer(ocl.queue, dPartial, CL_TRUE, 0, nPart * sizeof(double), h.data(), 0, nullptr, nullptr), "dot r");
        d2hScalar += nPart * sizeof(double);
        return std::accumulate(h.begin(), h.end(), 0.0);
    };
    auto sumsq = [&](cl_mem x) -> double {
        set(ocl.k_sumsq, 0, x);
        set(ocl.k_sumsq, 1, dPartial);
        checkCl(clSetKernelArg(ocl.k_sumsq, 2, lws * sizeof(double), nullptr), "sq local");
        set(ocl.k_sumsq, 3, n);
        checkCl(clEnqueueNDRangeKernel(ocl.queue, ocl.k_sumsq, 1, nullptr, &gwsRed, &lws, 0, nullptr, nullptr), "sq");
        std::vector<double> h(nPart);
        checkCl(clEnqueueReadBuffer(ocl.queue, dPartial, CL_TRUE, 0, nPart * sizeof(double), h.data(), 0, nullptr, nullptr), "sq r");
        d2hScalar += nPart * sizeof(double);
        return std::accumulate(h.begin(), h.end(), 0.0);
    };

    // r = b - A x0
    spmv(dX, dAp);
    set(ocl.k_resid, 0, dR);
    set(ocl.k_resid, 1, dB);
    set(ocl.k_resid, 2, dAp);
    set(ocl.k_resid, 3, n);
    enqueue1D(ocl.queue, ocl.k_resid, n, lws);
    precond(dZ, dR);
    copy(dP, dZ);
    double rzOld = dot(dR, dZ);
    const double bnorm = std::sqrt(sumsq(dB)) + 1e-30;
    int itUsed = 0;
    for (int it = 0; it < args.maxIters; ++it) {
        spmv(dP, dAp);
        const double pAp = dot(dP, dAp);
        const double alpha = rzOld / (pAp + 1e-300);
        axpy(dX, dP, alpha);
        axpy(dR, dAp, -alpha);
        itUsed = it + 1;
        const double rnorm = std::sqrt(sumsq(dR));
        if (rnorm / bnorm < args.tol) break;
        precond(dZ, dR);
        const double rzNew = dot(dR, dZ);
        const double beta = rzNew / (rzOld + 1e-300);
        xpay(dP, dZ, beta);
        rzOld = rzNew;
    }

    // true residual
    spmv(dX, dAp);
    set(ocl.k_resid, 0, dR);
    set(ocl.k_resid, 1, dB);
    set(ocl.k_resid, 2, dAp);
    set(ocl.k_resid, 3, n);
    enqueue1D(ocl.queue, ocl.k_resid, n, lws);
    const double rel = std::sqrt(sumsq(dR)) / bnorm;

    checkCl(clFinish(ocl.queue), "solve");
    auto tSolve = std::chrono::steady_clock::now();

    std::vector<double> xgpu(n);
    checkCl(clEnqueueReadBuffer(ocl.queue, dX, CL_TRUE, 0, bytes, xgpu.data(), 0, nullptr, nullptr), "dl");
    d2hField = bytes;
    auto tEnd = std::chrono::steady_clock::now();

    const double msSetup = std::chrono::duration<double, std::milli>(tSetup - t0).count();
    const double msSolve = std::chrono::duration<double, std::milli>(tSolve - tSetup).count();
    const double msDl = std::chrono::duration<double, std::milli>(tEnd - tSolve).count();

    double xmin = xgpu[0], xmax = xgpu[0];
    for (double v : xgpu) {
        xmin = std::min(xmin, v);
        xmax = std::max(xmax, v);
    }

    std::cout << "REL_RESIDUAL " << rel << "\n"
              << "PCG_ITERS " << itUsed << "\n"
              << "TIMING_MS setup=" << msSetup << " solve=" << msSolve
              << " download=" << msDl << "\n"
              << "TRAFFIC_BYTES h2d=" << h2d
              << " d2h_field=" << d2hField
              << " d2h_scalar=" << d2hScalar << "\n"
              << "x range [" << xmin << ", " << xmax << "]\n";

    int exitCode = 0;
    if (rel > args.tol * 10.0) {
        std::cerr << "FAIL: REL_RESIDUAL too large\n";
        exitCode = 3;
    } else {
        std::cout << "RESIDUAL check   : OK\n";
    }

    if (args.cpuCheck) {
        std::vector<double> xcpu;
        cpuPcg(rowPtr, colInd, vals, invDiag, b, xcpu, args.poly2, args.tol, args.maxIters);
        double maxAbs = 0.0;
        for (int i = 0; i < n; ++i)
            maxAbs = std::max(maxAbs, std::abs(xgpu[i] - xcpu[i]));
        std::cout << "MAX_ABS_ERR " << maxAbs << "\n";
        if (maxAbs > 1e-6) {
            std::cerr << "WARNING: GPU vs CPU disagree\n";
            if (maxAbs > 1e-2) exitCode = 2;
        } else {
            std::cout << "CPU check        : OK\n";
        }
    }

    cl_mem all[] = {dRow, dCol, dVal, dInv, dB, dX, dR, dZ, dP, dAp, dTmp, dPartial};
    for (cl_mem m : all) clReleaseMemObject(m);

    std::cout << "Done (CSR upload once; solve on device; one unload).\n";
    return exitCode;
}
