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


#include "clsparse_pcg.h"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(clSPARSE_PCG, 0);

    lduMatrix::solver::addsymMatrixConstructorToTable<clSPARSE_PCG>
        addclSPARSE_PCGSymMatrixConstructorToTable_;

}

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

/**
 * @brief Foam::clSPARSE_PCG::clSPARSE_PCG
 * @param fieldName
 * @param matrix
 * @param interfaceBouCoeffs
 * @param interfaceIntCoeffs
 * @param interfaces
 * @param solverControls
 */
Foam::clSPARSE_PCG::clSPARSE_PCG(const Foam::word &fieldName, const Foam::lduMatrix &matrix, const FieldField<Foam::Field, scalar> &interfaceBouCoeffs, const FieldField<Foam::Field, scalar> &interfaceIntCoeffs, const Foam::lduInterfaceFieldPtrsList &interfaces, const Foam::dictionary &solverControls)
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
 * @brief Foam::clSPARSE_PCG::solve
 * @param psi
 * @param source
 * @param cmpt
 * @return
 */
Foam::solverPerformance Foam::clSPARSE_PCG::solve(Foam::scalarField &psi, const Foam::scalarField &source, const Foam::direction cmpt) const
{
    word precond_name = lduMatrix::preconditioner::getName(controlDict_);

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


    return solverPerf;

}

// ************************************************************************* //

