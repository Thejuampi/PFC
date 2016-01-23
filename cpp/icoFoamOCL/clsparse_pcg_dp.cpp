/**
*  Autor: Juan Pablo Lescano
*  Licencia por definir
*/

/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/


#include "clsparse_pcg_dp.h"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(clSPARSE_PCG_DP, 0);

    lduMatrix::solver::addsymMatrixConstructorToTable<clSPARSE_PCG_DP>
        addclSPARSE_PCG_DPSymMatrixConstructorToTable_;

}

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

/**
 * @brief Foam::clSPARSE_PCG_DP::clSPARSE_PCG_DP
 * @param fieldName
 * @param matrix
 * @param interfaceBouCoeffs
 * @param interfaceIntCoeffs
 * @param interfaces
 * @param solverControls
 */
Foam::clSPARSE_PCG_DP::clSPARSE_PCG_DP
(
    const word& fieldName,
    const lduMatrix& matrix,
    const FieldField<Field, scalar>& interfaceBouCoeffs,
    const FieldField<Field, scalar>& interfaceIntCoeffs,
    const lduInterfaceFieldPtrsList& interfaces,
    const dictionary& solverControls
)
:
    lduMatrix::solver
    (
        fieldName,
        matrix,
        interfaceBouCoeffs,
        interfaceIntCoeffs,
        interfaces,
        solverControls
    )
{}

// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

/**
 * @brief Foam::clSPARSE_PCG_DP::solve
 * @param psi
 * @param source
 * @param cmpt
 * @return
 */
Foam::solverPerformance Foam::clSPARSE_PCG_DP::solve(Foam::scalarField &psi, const Foam::scalarField &source, const Foam::direction cmpt) const
{
    word precond_name = lduMatrix::preconditioner::getName(controlDict_);
    word solverPrintMode = controlDict_.lookupOrDefault<word>("SolverPrintMode", "QUIET");
    solverPerformance solverPerf(typeName + '(' + precond_name + ')', fieldName_);

    register label nCells = psi.size();

    scalarField pA(nCells);
    scalarField wA(nCells);

    // --- Calculate A.psi
    matrix_.Amul(wA, psi, interfaceBouCoeffs_, interfaces_, cmpt);

    // --- Calculate initial residual field
    scalarField rA(source - wA);

    // --- Calculate normalisation factor
    scalar normFactor = this->normFactor(psi, source, wA, pA);

    // --- Calculate normalised residual norm
    solverPerf.initialResidual() = gSumMag(rA)/normFactor;
    solverPerf.finalResidual() = solverPerf.initialResidual();

    if (!solverPerf.checkConvergence(tolerance_, relTol_)) {

        //TODO (juan): sacar la matriz del namespace y declararla aca, antes de usarla.



        clSparseUtils::importarMatrizDP(matrix(), &(clSparseUtils::g_A));
        clSparseUtils::importarVectorOpenFoam(source, &(clSparseUtils::g_b));
        clSparseUtils::importarVectorOpenFoam(psi, &(clSparseUtils::g_x));

        clSParseSolverControl solverControl = nullptr;
        if(precond_name == "CLSPARSE_DIAGONAL"){
            solverControl = clsparseCreateSolverControl(DIAGONAL, maxIter_, relTol_, tolerance_);
        } else {
            solverControl = clsparseCreateSolverControl(NOPRECOND, maxIter_, relTol_, tolerance_);
        }

        // We can set different print modes of the solver status:
        // QUIET - print no messages (default)
        // NORMAL - print summary
        // VERBOSE - per iteration status;

        if(solverPrintMode == "QUIET"){
            clsparseSolverPrintMode(solverControl, QUIET);
        } else if(solverPrintMode == "NORMAL"){
            clsparseSolverPrintMode(solverControl, NORMAL);
        } else if(solverPrintMode == "VERBOSE"){
            clsparseSolverPrintMode(solverControl,  VERBOSE);
        } else { // Si se ingresa un valor erroneo, toma QUIET por defecto
            clsparseSolverPrintMode(solverControl, QUIET);
        }
        /*
         * FIXME! (juan) : Problema con el tipo de datos (float o double)
         * status = clsparse___S___csrcg(&x, &A, &b, solverControl, control);
         * status = clsparse___D___csrcg(&x, &A, &b, solverControl, control);
         *
        */


        clSparseUtils::cl_status = clsparseDcsrcg(&clSparseUtils::g_x, &clSparseUtils::g_A, &clSparseUtils::g_b, solverControl, clSparseUtils::g_clSparseControl);

        //release solver control structure after finishing execution;
        clsparseReleaseSolverControl(solverControl);

    }


    return solverPerf;

}

// ************************************************************************* //

