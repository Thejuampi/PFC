/*---------------------------------------------------------------------------*\
  pfcSimpleFoam — stock simpleFoam + Mode B GPU pressure (optional)

  Default: identical SIMPLE path (CPU OpenFOAM solvers).
  Enable GPU pressure with system/pfcGpuDict { enabled true; } or
  env PFC_GPU_PRESSURE=1.

  Pressure: assemble LDU on host → PFC1 binary → csrOcl on GPU → write p.
  Momentum / turbulence stay on host (v2a). Full device SIMPLE = v2b later.
  See docs/SIMPLE_GPU.md
\*---------------------------------------------------------------------------*/

#include "fvCFD.H"
#include "dynamicFvMesh.H"
#include "singlePhaseTransportModel.H"
#include "turbulentTransportModel.H"
#include "simpleControl.H"
#include "fvOptions.H"
#include "IFstream.H"

#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <unistd.h>
#ifdef _WIN32
#include <windows.h>
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace
{

bool gpuPressureEnabled(const Time& runTime)
{
    if (const char* e = std::getenv("PFC_GPU_PRESSURE"))
    {
        const std::string v(e);
        if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
        if (v == "0" || v == "false" || v == "no" || v == "off") return false;
    }

    const fileName dictPath = runTime.system()/"pfcGpuDict";
    if (isFile(dictPath))
    {
        dictionary d;
        IFstream is(dictPath);
        if (is.good())
        {
            d.read(is);
            return d.lookupOrDefault("enabled", false);
        }
    }
    return false;
}

// Pack fvScalarMatrix → PFC1 binary (CSR + RHS) for csrOcl --pfc-bin.
// Includes boundary diagonal/source contributions (same idea as OF solveSegregated).
bool writePfcBin(const fvScalarMatrix& pEqn, const fileName& path)
{
    const lduAddressing& addr = pEqn.lduAddr();
    const label n = addr.size();
    const labelUList& lowerAddr = addr.lowerAddr();
    const labelUList& upperAddr = addr.upperAddr();

    // Mutable copies: OF temporarily folds boundary into diag/source before solve
    scalarField diag(pEqn.diag());
    const scalarField& upper = pEqn.upper();
    const scalarField& lower = pEqn.lower();
    scalarField b(pEqn.source());

    // Fold boundary coeffs into diag + RHS (scalar pressure)
    const volScalarField& psi = pEqn.psi();
    forAll(psi.boundaryField(), patchi)
    {
        const fvPatch& pp = psi.boundaryField()[patchi].patch();
        const labelUList& fc = pp.faceCells();
        const scalarField& iC = pEqn.internalCoeffs()[patchi];
        const scalarField& bC = pEqn.boundaryCoeffs()[patchi];
        forAll(fc, facei)
        {
            diag[fc[facei]] += iC[facei];
            b[fc[facei]] += bC[facei];
        }
    }

    std::vector<std::vector<std::pair<label, scalar>>> rows(n);
    for (label i = 0; i < n; ++i)
    {
        rows[i].push_back({i, diag[i]});
    }
    forAll(upper, f)
    {
        const label u = upperAddr[f];
        const label l = lowerAddr[f];
        rows[l].push_back({u, upper[f]});
        rows[u].push_back({l, lower[f]});
    }

    label nnz = 0;
    for (label i = 0; i < n; ++i)
    {
        auto& e = rows[i];
        std::sort(e.begin(), e.end(),
            [](const auto& a, const auto& c) { return a.first < c.first; });
        std::vector<std::pair<label, scalar>> m;
        for (const auto& pe : e)
        {
            if (!m.empty() && m.back().first == pe.first)
                m.back().second += pe.second;
            else
                m.push_back(pe);
        }
        rows[i].swap(m);
        nnz += rows[i].size();
    }

    std::vector<int32_t> rowPtr(n + 1), colInd;
    std::vector<double> vals;
    colInd.reserve(nnz);
    vals.reserve(nnz);
    for (label i = 0; i < n; ++i)
    {
        rowPtr[i] = static_cast<int32_t>(colInd.size());
        for (const auto& pe : rows[i])
        {
            colInd.push_back(static_cast<int32_t>(pe.first));
            vals.push_back(static_cast<double>(pe.second));
        }
    }
    rowPtr[n] = static_cast<int32_t>(colInd.size());

    std::vector<double> bb(n);
    for (label i = 0; i < n; ++i) bb[i] = static_cast<double>(b[i]);

    mkDir(path.path());
    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out)
    {
        Info<< "ERROR: cannot write " << path << nl;
        return false;
    }
    const char magic[4] = {'P', 'F', 'C', '1'};
    const int32_t n32 = static_cast<int32_t>(n);
    const int32_t nnz32 = static_cast<int32_t>(vals.size());
    out.write(magic, 4);
    out.write(reinterpret_cast<const char*>(&n32), 4);
    out.write(reinterpret_cast<const char*>(&nnz32), 4);
    out.write(reinterpret_cast<const char*>(rowPtr.data()), (n + 1) * 4);
    out.write(reinterpret_cast<const char*>(colInd.data()), nnz32 * 4);
    out.write(reinterpret_cast<const char*>(vals.data()), nnz32 * 8);
    out.write(reinterpret_cast<const char*>(bb.data()), n * 8);
    return static_cast<bool>(out);
}

bool readSolutionX(const fileName& path, scalarField& psi)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return false;
    int32_t n = 0;
    in.read(reinterpret_cast<char*>(&n), 4);
    if (n != psi.size())
    {
        Info<< "ERROR: x size " << n << " != psi " << psi.size() << nl;
        return false;
    }
    std::vector<double> x(n);
    in.read(reinterpret_cast<char*>(x.data()), static_cast<size_t>(n) * 8);
    if (!in) return false;
    for (label i = 0; i < n; ++i) psi[i] = static_cast<scalar>(x[i]);
    return true;
}

// Mode B solve handoff:
//   1) Prefer direct scripts/pfc_gpu_pcg.sh if WSL can exec Windows csrOcl.exe
//   2) Else file-watch worker on Windows host:
//        write matrix/pfc_gpu.request → wait for pfc_gpu.done | pfc_gpu.fail
bool runGpuPcg(const fileName& binPath, const fileName& xPath)
{
    const fileName matrixDir = binPath.path();
    const fileName reqPath = matrixDir/"pfc_gpu.request";
    const fileName donePath = matrixDir/"pfc_gpu.done";
    const fileName failPath = matrixDir/"pfc_gpu.fail";

    // Clean previous handshake
    rm(reqPath);
    rm(donePath);
    rm(failPath);
    rm(xPath);

    // Optional: try shell bridge first (works when WSL interop runs .exe)
    if (const char* sh = std::getenv("PFC_GPU_SCRIPT"))
    {
        const std::string cmd =
            std::string("bash \"") + sh + "\" \"" + binPath + "\" \"" + xPath + "\"";
        Info<< "PFC Mode B try script: " << cmd << nl;
        const int rc = std::system(cmd.c_str());
        if (rc == 0 && isFile(xPath))
        {
            Info<< "PFC Mode B: GPU via script OK\n";
            return true;
        }
        Info<< "PFC Mode B: script failed rc=" << rc
            << " — falling back to file-watch worker\n";
    }

    // File-watch: Windows scripts/pfc_gpu_worker.ps1 must be running
    Info<< "PFC Mode B: file-watch request → " << reqPath << nl;
    Info<< "  (start worker on Windows: "
        << "powershell -File scripts/pfc_gpu_worker.ps1)\n";

    {
        std::ofstream req(reqPath.c_str());
        req << "solve\n";
    }

    // Timeout: large matrices can take minutes
    const int maxWaitSec =
        (std::getenv("PFC_GPU_TIMEOUT_SEC")
            ? std::atoi(std::getenv("PFC_GPU_TIMEOUT_SEC"))
            : 600);

    for (int t = 0; t < maxWaitSec * 4; ++t)
    {
        if (isFile(failPath))
        {
            Info<< "ERROR: GPU worker failed (see matrix/pfc_gpu.fail)\n";
            return false;
        }
        if (isFile(donePath) && isFile(xPath))
        {
            Info<< "PFC Mode B: GPU via worker OK\n";
            return true;
        }
        // 250 ms poll
#ifdef _WIN32
        Sleep(250);
#else
        usleep(250000);
#endif
    }

    Info<< "ERROR: GPU worker timeout after " << maxWaitSec << "s\n";
    return false;
}

} // namespace

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

int main(int argc, char *argv[])
{
    argList::addNote
    (
        "simpleFoam + optional Mode B GPU pressure (PFC). "
        "Enable: system/pfcGpuDict or PFC_GPU_PRESSURE=1"
    );

    #include "postProcess.H"

    #include "addCheckCaseOptions.H"
    #include "setRootCaseLists.H"
    #include "createTime.H"
    #include "createDynamicFvMesh.H"
    #include "createControl.H"
    #include "createFields.H"
    #include "initContinuityErrs.H"

    turbulence->validate();

    const bool useGpuP = gpuPressureEnabled(runTime);
    if (useGpuP)
    {
        Info<< "PFC Mode B: GPU pressure ENABLED\n" << endl;
    }
    else
    {
        Info<< "PFC Mode B: GPU pressure off (stock CPU p solver)\n" << endl;
    }

    Info<< "\nStarting time loop\n" << endl;

    while (simple.loop())
    {
        Info<< "Time = " << runTime.timeName() << nl << endl;

        mesh.controlledUpdate();

        if (mesh.changing())
        {
            MRF.update();
        }

        // --- Pressure-velocity SIMPLE corrector
        {
            #include "UEqn.H"
            #include "pEqn.H"
        }

        laminarTransport.correct();
        turbulence->correct();

        runTime.write();

        runTime.printExecutionTime(Info);
    }

    Info<< "End\n" << endl;

    return 0;
}

// ************************************************************************* //
