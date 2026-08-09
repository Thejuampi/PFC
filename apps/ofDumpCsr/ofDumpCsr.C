/*---------------------------------------------------------------------------*\
  Dump the assembled pressure fvMatrix from a simpleFoam-like case as
  Matrix Market CSR (+ RHS) for apps/csrOcl full-device PCG.

  Run from a case directory that already has mesh + 0/ fields:
    ofDumpCsr

  Writes: matrix/of_p.mtx  matrix/of_p.rhs
\*---------------------------------------------------------------------------*/

#include "fvCFD.H"
#include "simpleControl.H"
#include "fvOptions.H"
#include <fstream>
#include <vector>
#include <algorithm>

int main(int argc, char *argv[])
{
    argList::addNote("Dump pressure Poisson-like matrix as Matrix Market for csrOcl");

    #include "setRootCase.H"
    #include "createTime.H"
    // Prefer latest written fields (non-zero p from a finished simpleFoam run)
    #include "createMesh.H"

    runTime.setTime(runTime.endTime(), 0);
    instantList times = runTime.times();
    if (times.size() > 1)
    {
        runTime.setTime(times.last(), times.size() - 1);
        Info<< "Using time directory " << runTime.timeName() << nl;
    }

    simpleControl simple(mesh);

    #include "createFields.H"

    // Assemble Laplacian on p (same LDU pattern as pressure Poisson).
    fvScalarMatrix pEqn
    (
        fvm::laplacian(p)
    );
    // Fix reference so pure Neumann-ish systems stay invertible
    pEqn.setReference(0, scalar(0));

    // Extract LDU → COO for Matrix Market
    const lduAddressing& addr = pEqn.lduAddr();
    const label n = addr.size();
    const labelUList& lowerAddr = addr.lowerAddr();
    const labelUList& upperAddr = addr.upperAddr();
    const scalarField& diag = pEqn.diag();
    const scalarField& upper = pEqn.upper();
    const scalarField& lower = pEqn.lower();

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
            [](const auto& a, const auto& b) { return a.first < b.first; });
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

    // Manufactured RHS: b = A * p  so device solution should recover p
    // (uses internal field; boundary effects are in the matrix diag/coeffs)
    const scalarField& pInt = p.primitiveField();
    std::vector<scalar> b(n, 0.0);
    for (label i = 0; i < n; ++i)
    {
        scalar acc = 0.0;
        for (const auto& pe : rows[i])
        {
            acc += pe.second * pInt[pe.first];
        }
        b[i] = acc;
    }

    // Guard zero/near-zero diagonals (should not happen after setReference)
    label nZero = 0;
    for (label i = 0; i < n; ++i)
    {
        if (mag(diag[i]) < SMALL)
        {
            ++nZero;
        }
    }
    if (nZero)
    {
        Info<< "WARNING: " << nZero << " near-zero diagonal entries\n";
    }

    mkDir("matrix");
    {
        std::ofstream out("matrix/of_p.mtx");
        out.precision(16);
        out << "%%MatrixMarket matrix coordinate real general\n";
        out << "% OpenFOAM fvMatrix laplacian(p) from ofDumpCsr; RHS = A*p\n";
        out << n << " " << n << " " << nnz << "\n";
        for (label i = 0; i < n; ++i)
        {
            for (const auto& pe : rows[i])
            {
                out << (i + 1) << " " << (pe.first + 1) << " " << pe.second << "\n";
            }
        }
    }
    {
        std::ofstream out("matrix/of_p.rhs");
        out.precision(16);
        out << n << "\n";
        for (label i = 0; i < n; ++i)
        {
            out << b[i] << "\n";
        }
    }

    Info<< "Wrote matrix/of_p.mtx and matrix/of_p.rhs  n=" << n
        << " nnz=" << nnz << nl;
    Info<< "Solve with: csrOcl --mtx matrix/of_p.mtx --rhs matrix/of_p.rhs" << nl;

    return 0;
}
