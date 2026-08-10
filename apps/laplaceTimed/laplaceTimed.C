/*---------------------------------------------------------------------------*\
  Application
      laplaceTimed

  Description
      Minimal timed Laplace solve for CPU baseline benchmarking.

      Closest modern equivalent of the original PFC Laplace.C:
      - pure fvm::laplacian (no ddt), so wall time is dominated by the
        linear solver rather than the transient scheme
      - uses stock OpenFOAM matrix assembly + CPU linear solvers only
      - no GPU / OpenCL / Paralution dependencies

      Build (inside an OpenFOAM shell, from this directory):
          wmake

      Run against cases/laplaceCpu (or a copy):
          laplaceTimed -case ../../cases/laplaceCpu

\*---------------------------------------------------------------------------*/

#include "fvCFD.H"
#include "simpleControl.H"

int main(int argc, char *argv[])
{
    argList::addNote
    (
        "Timed pure-Laplace solve (CPU baseline, no GPU)."
    );

    #include "setRootCase.H"
    #include "createTime.H"
    #include "createMesh.H"

    simpleControl simple(mesh);

    #include "createFields.H"

    Info<< "\n=== laplaceTimed CPU baseline ===\n" << endl;

    while (simple.loop())
    {
        Info<< "Time = " << runTime.timeName() << nl << endl;

        const scalar t0 = runTime.elapsedCpuTime();
        const scalar c0 = runTime.elapsedClockTime();

        while (simple.correctNonOrthogonal())
        {
            // Pure Laplace (matches original PFC intent).
            fvScalarMatrix TEqn
            (
                fvm::laplacian(DT, T)
            );
            TEqn.solve();
        }

        const scalar t1 = runTime.elapsedCpuTime();
        const scalar c1 = runTime.elapsedClockTime();

        Info<< "SolveCpuTime   = " << (t1 - t0) << " s" << nl
            << "SolveClockTime = " << (c1 - c0) << " s" << nl << endl;

        runTime.write();
        runTime.printExecutionTime(Info);
    }

    Info<< "End\n" << endl;
    return 0;
}

// ************************************************************************* //
