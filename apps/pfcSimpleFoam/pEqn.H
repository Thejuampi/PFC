{
    volScalarField rAU(1.0/UEqn.A());
    volVectorField HbyA(constrainHbyA(rAU*UEqn.H(), U, p));
    surfaceScalarField phiHbyA("phiHbyA", fvc::flux(HbyA));
    MRF.makeRelative(phiHbyA);
    adjustPhi(phiHbyA, U, p);

    tmp<volScalarField> rAtU(rAU);

    if (simple.consistent())
    {
        rAtU = 1.0/(1.0/rAU - UEqn.H1());
        phiHbyA +=
            fvc::interpolate(rAtU() - rAU)*fvc::snGrad(p)*mesh.magSf();
        HbyA -= (rAU - rAtU())*fvc::grad(p);
    }

    tUEqn.clear();

    // Update the pressure BCs to ensure flux consistency
    constrainPressure(p, U, phiHbyA, rAtU(), MRF);

    // Non-orthogonal pressure corrector loop
    while (simple.correctNonOrthogonal())
    {
        fvScalarMatrix pEqn
        (
            fvm::laplacian(rAtU(), p) == fvc::div(phiHbyA)
        );

        pEqn.setReference(pRefCell, pRefValue);

        if (useGpuP)
        {
            // Mode B: host assemble → GPU CSR PCG → write p
            const fileName binPath = runTime.path()/"matrix"/"pfc_gpu.bin";
            const fileName xPath = runTime.path()/"matrix"/"pfc_gpu.x";

            if (!writePfcBin(pEqn, binPath))
            {
                FatalErrorInFunction
                    << "Failed to write PFC1 binary for GPU pressure"
                    << exit(FatalError);
            }
            if (!runGpuPcg(binPath, xPath))
            {
                FatalErrorInFunction
                    << "GPU pressure solve failed. Check PFC_GPU_SCRIPT / "
                    << "scripts/pfc_gpu_pcg.sh and csrOcl build."
                    << exit(FatalError);
            }
            if (!readSolutionX(xPath, p.primitiveFieldRef()))
            {
                FatalErrorInFunction
                    << "Failed to read GPU solution " << xPath
                    << exit(FatalError);
            }
            p.correctBoundaryConditions();
        }
        else
        {
            pEqn.solve();
        }

        if (simple.finalNonOrthogonalIter())
        {
            phi = phiHbyA - pEqn.flux();
        }
    }

    #include "continuityErrs.H"

    // Explicitly relax pressure for momentum corrector
    p.relax();

    // Momentum corrector
    U = HbyA - rAtU()*fvc::grad(p);
    U.correctBoundaryConditions();
    fvOptions.correct(U);
}
