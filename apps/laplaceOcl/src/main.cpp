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
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {

enum class Precond { Jacobi, Rbgs, Poly2 };

// Device-side working set estimate (must match buffers allocated in main):
// 14 double fields (T,b,x,r,z,p,Ap,invDiag,dia0..4,tmp) + mark uchar + partials.
inline std::uint64_t estimateDeviceBytes(std::uint64_t nCells)
{
    constexpr std::uint64_t kDoubles = 14;
    constexpr std::uint64_t kPartials = 64; // reduction workspace
    return nCells * (kDoubles * sizeof(double) + 1) + kPartials * sizeof(double);
}

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
    // poly2: Neumann / SPAI-ish M^{-1}≈ 2 D^{-1} - D^{-1} A D^{-1} (SPD-friendly for CG)
    Precond precond = Precond::Poly2;
    int precondSweeps = 2; // only for rbgs (not recommended with CG; kept for experiments)
    double memFrac = 0.0; // if >0, auto nx=ny so working set ≈ memFrac * device global mem
    bool cpuCheck = true;
    bool writeCsv = true;
    bool quietSteps = false;
    std::string kernelPath;
    std::string outCsv = "T_gpu.csv";
};

// Host↔device traffic accounting (matrix/fields stay on device).
struct Traffic {
    std::uint64_t h2dBytes = 0;       // intentional uploads (0 in full-device path)
    std::uint64_t d2hFieldBytes = 0;  // final T download
    std::uint64_t d2hScalarBytes = 0; // residual / reduction partials
    std::uint64_t scalarReads = 0;    // number of reduction readbacks
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
        else if (s == "--precond") {
            std::string p = need("--precond");
            if (p == "jacobi") a.precond = Precond::Jacobi;
            else if (p == "rbgs") a.precond = Precond::Rbgs;
            else if (p == "poly2") a.precond = Precond::Poly2;
            else die("--precond must be jacobi|poly2|rbgs");
        } else if (s == "--precond-sweeps") a.precondSweeps = std::stoi(need("--precond-sweeps"));
        else if (s == "--mem-frac") a.memFrac = std::stod(need("--mem-frac"));
        else if (s == "--kernels") a.kernelPath = need("--kernels");
        else if (s == "--out") a.outCsv = need("--out");
        else if (s == "--no-csv") a.writeCsv = false;
        else if (s == "--no-cpu-check") a.cpuCheck = false;
        else if (s == "--quiet") a.quietSteps = true;
        else if (s == "--help" || s == "-h") {
            std::cout
                << "laplaceOcl — device-resident diffusion (OpenCL)\n"
                << "  --nx N --ny N --steps N --dt D --DT D\n"
                << "  --mem-frac F   auto square mesh so device buffers ≈ F * GPU VRAM\n"
                << "                 (e.g. 0.5 → ~50% of global mem)\n"
                << "  --tol T --max-iters N --fixed-iters N\n"
                << "      (fixed-iters>0: no residual host check each PCG iter)\n"
                << "  --precond jacobi|poly2|rbgs   (default poly2; rbgs not SPD — weak with CG)\n"
                << "  --precond-sweeps N      (only rbgs; default 2)\n"
                << "  --kernels path/to/laplace.cl --out T_gpu.csv --no-csv\n"
                << "  --no-cpu-check --quiet\n"
                << "\nLifecycle: assemble+solve on GPU; one field download at end.\n"
                << "Residual control may still read small reduction buffers (not the matrix).\n";
            std::exit(0);
        } else {
            die("unknown arg: " + s);
        }
    }
    if (a.precondSweeps < 1) die("--precond-sweeps must be >= 1");
    if (a.memFrac < 0.0 || a.memFrac > 0.95) die("--mem-frac must be in [0, 0.95]");
    if (a.memFrac == 0.0 && (a.nx < 3 || a.ny < 3)) die("nx,ny must be >= 3");
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
    std::uint64_t globalMemBytes = 0;
    std::uint64_t maxAllocBytes = 0;

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
    cl_kernel k_poly2{};
    cl_kernel k_rbgs{};
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
        cl_ulong gmem = 0;
        clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(gmem), &gmem, nullptr);
        cl_ulong maxAlloc = 0;
        clGetDeviceInfo(device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(maxAlloc), &maxAlloc, nullptr);
        globalMemBytes = static_cast<std::uint64_t>(gmem);
        maxAllocBytes = static_cast<std::uint64_t>(maxAlloc);
        std::cout << "OpenCL device: " << name << " (" << vendor << ")\n"
                  << "  global_mem : " << (globalMemBytes / (1024.0 * 1024.0 * 1024.0)) << " GiB\n"
                  << "  max_alloc  : " << (maxAllocBytes / (1024.0 * 1024.0 * 1024.0)) << " GiB\n";

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
        k_poly2 = mk("poly2_combine");
        k_rbgs = mk("rbgs_sweep");
        k_resid = mk("vec_residual");
        k_sumsq = mk("reduce_sum_sq");
        k_dot = mk("reduce_dot");
    }

    ~Ocl()
    {
        auto relK = [](cl_kernel k) { if (k) clReleaseKernel(k); };
        relK(k_mark); relK(k_init); relK(k_assemble); relK(k_rhs); relK(k_spmv);
        relK(k_axpy); relK(k_xpay); relK(k_copy); relK(k_scal); relK(k_set);
        relK(k_jacobi); relK(k_poly2); relK(k_rbgs); relK(k_resid); relK(k_sumsq); relK(k_dot);
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

    auto applyPrecond = [&](std::vector<double>& z, const std::vector<double>& r) {
        if (a.precond == Precond::Jacobi) {
            for (int c = 0; c < n; ++c) z[c] = invD[c] * r[c];
            return;
        }
        if (a.precond == Precond::Poly2) {
            // z = 2 D^{-1} r - D^{-1} A D^{-1} r
            std::vector<double> t(n), w(n);
            for (int c = 0; c < n; ++c) t[c] = invD[c] * r[c];
            spmv(t, w);
            for (int c = 0; c < n; ++c) z[c] = 2.0 * t[c] - invD[c] * w[c];
            return;
        }
        // RBGS (experimental; not SPD — may hurt CG)
        std::fill(z.begin(), z.end(), 0.0);
        auto oneColor = [&](int color) {
            for (int c = 0; c < n; ++c) {
                const int i = c % nx;
                const int j = c / nx;
                if (((i + j) & 1) != color) continue;
                if (!isInterior(c)) {
                    z[c] = r[c];
                    continue;
                }
                double sigma = 0.0;
                if (c >= nx) sigma += dia0[c] * z[c - nx];
                if (i > 0) sigma += dia1[c] * z[c - 1];
                if (i < nx - 1) sigma += dia3[c] * z[c + 1];
                if (c + nx < n) sigma += dia4[c] * z[c + nx];
                z[c] = (r[c] - sigma) / dia2[c];
            }
        };
        for (int s = 0; s < a.precondSweeps; ++s) {
            oneColor(0);
            oneColor(1);
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
        applyPrecond(z, r);
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
            applyPrecond(z, r);
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

    const char* precondName =
        (args.precond == Precond::Jacobi) ? "jacobi" :
        (args.precond == Precond::Poly2) ? "poly2" : "rbgs";

    const std::string kpath = findKernels(args);
    std::cout << "laplaceOcl full-device lifecycle\n"
              << "  kernels  : " << kpath << "\n";
    Ocl ocl;
    ocl.init(readFile(kpath));

    // Auto mesh to target a fraction of device global memory (working set).
    if (args.memFrac > 0.0) {
        if (ocl.globalMemBytes == 0) die("device global mem unknown");
        const std::uint64_t target = static_cast<std::uint64_t>(args.memFrac * ocl.globalMemBytes);
        // bytes ≈ 113 * n  → n ≈ target / 113; square mesh
        std::uint64_t nTarget = target / 113;
        if (nTarget < 9) nTarget = 9;
        // Cap single double buffer under max alloc (n * 8 <= maxAlloc * 0.9)
        if (ocl.maxAllocBytes > 0) {
            const std::uint64_t maxN = static_cast<std::uint64_t>(ocl.maxAllocBytes * 0.9 / sizeof(double));
            if (nTarget > maxN) nTarget = maxN;
        }
        int side = static_cast<int>(std::floor(std::sqrt(static_cast<double>(nTarget))));
        if (side < 3) side = 3;
        // keep even-ish for RB coloring comfort
        if (side % 2) ++side;
        args.nx = side;
        args.ny = side;
        // Huge meshes: CPU check is not practical
        if (static_cast<std::uint64_t>(args.nx) * static_cast<std::uint64_t>(args.ny) > 2'000'000ULL
            && args.cpuCheck) {
            std::cout << "  note     : disabling CPU check (mesh too large for host PCG)\n";
            args.cpuCheck = false;
        }
    }

    const int nx = args.nx;
    const int ny = args.ny;
    const std::uint64_t n64 = static_cast<std::uint64_t>(nx) * static_cast<std::uint64_t>(ny);
    if (n64 > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        die("mesh too large for 32-bit cell index path");
    }
    const int n = static_cast<int>(n64);
    const double dx = args.Lx / (nx - 1);
    const double dy = args.Ly / (ny - 1);
    const std::uint64_t devBytes = estimateDeviceBytes(n64);
    const double memFracUsed =
        ocl.globalMemBytes ? (static_cast<double>(devBytes) / ocl.globalMemBytes) : 0.0;

    std::cout << "  mesh     : " << nx << " x " << ny << " (" << n << " cells)\n"
              << "  steps    : " << args.steps << "  dt=" << args.dt << "  DT=" << args.DT << "\n"
              << "  domain   : " << args.Lx << " x " << args.Ly << "  dx=" << dx << " dy=" << dy << "\n"
              << "  precond  : " << precondName << "  sweeps=" << args.precondSweeps << "\n"
              << "DEVICE_BUF_BYTES " << devBytes
              << "  MEM_FRAC_USED " << memFracUsed
              << "  (target_frac=" << args.memFrac << ")\n";

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

    Traffic traffic;
    // Full-device path: mesh/fields generated on GPU → intentional H2D field uploads = 0.

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

    checkCl(clFinish(ocl.queue), "clFinish setup");
    auto tSetup = std::chrono::steady_clock::now();

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

    auto rbgsSweep = [&](cl_mem z, cl_mem rhs, int color) {
        set(ocl.k_rbgs, 0, z);
        set(ocl.k_rbgs, 1, d0);
        set(ocl.k_rbgs, 2, d1);
        set(ocl.k_rbgs, 3, d2);
        set(ocl.k_rbgs, 4, d3);
        set(ocl.k_rbgs, 5, d4);
        set(ocl.k_rbgs, 6, rhs);
        set(ocl.k_rbgs, 7, dMark);
        set(ocl.k_rbgs, 8, nx);
        set(ocl.k_rbgs, 9, n);
        set(ocl.k_rbgs, 10, color);
        enqueue1D(ocl.queue, ocl.k_rbgs, n, lws);
    };

    cl_mem dTmp = mkBuf(ocl.ctx, CL_MEM_READ_WRITE, bytes);

    auto applyPrecond = [&](cl_mem z, cl_mem r) {
        if (args.precond == Precond::Jacobi) {
            jacobi(z, r);
            return;
        }
        if (args.precond == Precond::Poly2) {
            // t = D^{-1} r ; z = A t ; z = 2 t - D^{-1} (A t)
            jacobi(dTmp, r);
            spmv(dTmp, z);
            set(ocl.k_poly2, 0, z);
            set(ocl.k_poly2, 1, dTmp);
            set(ocl.k_poly2, 2, dInv);
            set(ocl.k_poly2, 3, n);
            enqueue1D(ocl.queue, ocl.k_poly2, n, lws);
            return;
        }
        // RBGS experimental (not SPD — may hurt CG)
        set(ocl.k_set, 0, z);
        set(ocl.k_set, 1, 0.0);
        set(ocl.k_set, 2, n);
        enqueue1D(ocl.queue, ocl.k_set, n, lws);
        for (int s = 0; s < args.precondSweeps; ++s) {
            rbgsSweep(z, r, 0);
            rbgsSweep(z, r, 1);
        }
    };

    auto noteScalarRead = [&]() {
        traffic.d2hScalarBytes += static_cast<std::uint64_t>(nPart) * sizeof(double);
        traffic.scalarReads += 1;
    };

    auto dot = [&](cl_mem a, cl_mem b) -> double {
        set(ocl.k_dot, 0, a);
        set(ocl.k_dot, 1, b);
        set(ocl.k_dot, 2, dPartial);
        set(ocl.k_dot, 4, n);
        checkCl(clSetKernelArg(ocl.k_dot, 3, lws * sizeof(double), nullptr), "dot local");
        checkCl(clEnqueueNDRangeKernel(ocl.queue, ocl.k_dot, 1, nullptr, &gwsRed, &lws, 0, nullptr, nullptr), "dot");
        std::vector<double> h(nPart);
        checkCl(clEnqueueReadBuffer(ocl.queue, dPartial, CL_TRUE, 0, nPart * sizeof(double), h.data(), 0, nullptr, nullptr),
            "dot read");
        noteScalarRead();
        return std::accumulate(h.begin(), h.end(), 0.0);
    };

    auto sumsq2 = [&](cl_mem x) -> double {
        set(ocl.k_sumsq, 0, x);
        set(ocl.k_sumsq, 1, dPartial);
        checkCl(clSetKernelArg(ocl.k_sumsq, 2, lws * sizeof(double), nullptr), "sumsq local");
        set(ocl.k_sumsq, 3, n);
        checkCl(clEnqueueNDRangeKernel(ocl.queue, ocl.k_sumsq, 1, nullptr, &gwsRed, &lws, 0, nullptr, nullptr), "sumsq");
        std::vector<double> h(nPart);
        checkCl(clEnqueueReadBuffer(ocl.queue, dPartial, CL_TRUE, 0, nPart * sizeof(double), h.data(), 0, nullptr, nullptr),
            "sumsq read");
        noteScalarRead();
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

        applyPrecond(dZ, dR);
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

            applyPrecond(dZ, dR);
            const double rzNew = dot(dR, dZ);
            const double beta = rzNew / (rzOld + 1e-300);
            xpay(dP, dZ, beta); // p = z + beta p
            rzOld = rzNew;
        }
        totalPcgIters += itUsed;
        copy(dT, dX);

        if (!args.quietSteps &&
            ((step + 1) % std::max(1, args.steps / 5) == 0 || step == args.steps - 1)) {
            std::cout << "  step " << (step + 1) << "/" << args.steps
                      << "  pcgIters=" << itUsed << "\n";
        }
    }

    // True residual of last linear solve: r = b - A x  (dB = last RHS, dT = last x)
    spmv(dT, dAp);
    set(ocl.k_resid, 0, dR);
    set(ocl.k_resid, 1, dB);
    set(ocl.k_resid, 2, dAp);
    set(ocl.k_resid, 3, n);
    enqueue1D(ocl.queue, ocl.k_resid, n, lws);
    const double finalRnorm = std::sqrt(sumsq2(dR));
    const double finalBnorm = std::sqrt(sumsq2(dB)) + 1e-30;
    const double relResidual = finalRnorm / finalBnorm;
    std::cout << "REL_RESIDUAL " << relResidual
              << "  (||b-Ax||/||b||, last step)\n";

    checkCl(clFinish(ocl.queue), "clFinish solve");
    auto tSolve = std::chrono::steady_clock::now();

    // ---- single field download ----
    std::vector<double> Tgpu(n);
    checkCl(clEnqueueReadBuffer(ocl.queue, dT, CL_TRUE, 0, bytes, Tgpu.data(), 0, nullptr, nullptr), "download T");
    traffic.d2hFieldBytes += static_cast<std::uint64_t>(bytes);

    auto tEnd = std::chrono::steady_clock::now();

    const double msSetup = std::chrono::duration<double, std::milli>(tSetup - t0).count();
    const double msSolve = std::chrono::duration<double, std::milli>(tSolve - tSetup).count();
    const double msDl = std::chrono::duration<double, std::milli>(tEnd - tSolve).count();
    const double msTotal = std::chrono::duration<double, std::milli>(tEnd - t0).count();

    double tmin = Tgpu[0], tmax = Tgpu[0], tsum = 0.0;
    for (double v : Tgpu) {
        tmin = std::min(tmin, v);
        tmax = std::max(tmax, v);
        tsum += v;
    }

    std::cout << "TIMING_MS setup=" << msSetup
              << " solve=" << msSolve
              << " download=" << msDl
              << " total=" << msTotal << "\n"
              << "TRAFFIC_BYTES h2d=" << traffic.h2dBytes
              << " d2h_field=" << traffic.d2hFieldBytes
              << " d2h_scalar=" << traffic.d2hScalarBytes
              << " scalar_reads=" << traffic.scalarReads << "\n"
              << "PRECOND " << precondName << " sweeps=" << args.precondSweeps << "\n"
              << "PCG_ITERS total=" << totalPcgIters << "\n"
              << "T range         : [" << tmin << ", " << tmax << "]  mean=" << (tsum / n) << "\n";

    // Hybrid-2015 estimate for contrast: each step would bounce A(5*n) + x + b + x_sol
    const std::uint64_t hybridPerStep =
        static_cast<std::uint64_t>(n) * sizeof(double) * (5 /*A dia*/ + 1 /*x*/ + 1 /*b*/ + 1 /*sol*/);
    const std::uint64_t hybridEst = hybridPerStep * static_cast<std::uint64_t>(args.steps);
    std::cout << "HYBRID_EST_BYTES_per_run≈" << hybridEst
              << "  (if A,x,b recopied every step; not what we do)\n";

    if (args.writeCsv) {
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

    int exitCode = 0;

    // Residual gate: when not using fixed-iters, the last solve must meet tol.
    if (args.fixedIters <= 0 && relResidual > args.tol * 10.0) {
        std::cerr << "FAIL: REL_RESIDUAL " << relResidual << " > 10*tol (" << (args.tol * 10.0) << ")\n";
        exitCode = 3;
    } else if (args.fixedIters <= 0) {
        std::cout << "RESIDUAL check   : OK\n";
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
        std::cout << "CPU_MS " << msCpu << "\n"
                  << "MAX_ABS_ERR " << maxAbs << "\n"
                  << "MAX_REL_ERR " << maxRel << "\n"
                  << "SPEEDUP_vs_cpu " << (msCpu / std::max(msTotal, 1e-9)) << "\n";
        if (maxAbs > 1e-4) {
            std::cerr << "WARNING: GPU vs CPU disagree beyond 1e-4\n";
            if (maxAbs > 1.0) exitCode = 2;
        } else {
            std::cout << "CPU check        : OK\n";
        }
    }

    // release buffers
    cl_mem all[] = {dT, dB, dX, dR, dZ, dP, dAp, dMark, dInv, d0, d1, d2, d3, d4, dPartial, dTmp};
    for (cl_mem m : all) clReleaseMemObject(m);

    std::cout << "Done (full-device assemble+solve; single final unload).\n";
    return exitCode;
}
