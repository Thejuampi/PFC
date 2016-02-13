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

//#include "clSPARSE.h"
#include <clSPARSE.h>
#include "clSparseUtils.h"
#include "clsparse_pcg_dp.H"

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
Foam::clSPARSE_PCG_DP::clSPARSE_PCG_DP(const word& fieldName,
		const lduMatrix& matrix,
		const FieldField<Field, scalar>& interfaceBouCoeffs,
		const FieldField<Field, scalar>& interfaceIntCoeffs,
		const lduInterfaceFieldPtrsList& interfaces,
		const dictionary& solverControls) :
		lduMatrix::solver(fieldName, matrix, interfaceBouCoeffs,
				interfaceIntCoeffs, interfaces, solverControls)
{

//	cl_status status = CL_SUCCESS;
//	cl_status = cl::Platform::get(&m_platforms);
//	int platform_id = getPlatformId();
//	platform_id = getPlatformId();
//	//TODO (juan) usar puntero?
//	m_platform = m_platforms[platform_id];
//	cl_status = m_platform.getDevices(CL_DEVICE_TYPE_GPU, &g_devices);
//	cl_device_id device_id = getDeviceId();
//    device_id = getDeviceId();
//    m_device = g_devices[device_id];
//    m_context = cl::Context(m_device);
//    m_queue(m_context, m_device);
//    status = clsparseSetup();
//    clsparseStatus p_clSparceStatus = clsparseSuccess;
//    cl_command_queue clCommandQueue = &m_queue();
//    m_clSparseControl = clsparseCreateControl(clCommandQueue, &p_clSparceStatus);

    //Ver cuantas veces es necesario hacer el init() de los vectores y/o matrices

}

cl_platform_id Foam::clSPARSE_PCG_DP::getPlatformId() {
	return 0;
}

cl_device_id Foam::clSPARSE_PCG_DP::getDeviceId() {
	//TODO (juan) Modificar esto para que obtenga el id de MPI?
	return 0;
}

// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

/**
 * @brief Foam::clSPARSE_PCG_DP::solve
 * @param psi
 * @param source
 * @param cmpt
 * @return
 */

//using namespace Foam;

Foam::solverPerformance Foam::clSPARSE_PCG_DP::solve
(
    scalarField& psi,
    const scalarField& source,
    const direction cmpt
) const
{
//
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
	solverPerf.initialResidual() = gSumMag(rA) / normFactor;
	solverPerf.finalResidual() = solverPerf.initialResidual();

	if (!solverPerf.checkConvergence(tolerance_, relTol_)) {

		clsparseCsrMatrix cls_matrix;
		cldenseVector cls_b;
		cldenseVector cls_x;

		clsparseInitVector(&cls_b);
		clsparseInitVector(&cls_x);
		clsparseInitCsrMatrix(&cls_matrix);

		importarMatrizDP(matrix(), &cls_matrix, &(m_context()), &(m_queue()), m_clSparseControl);
		importarVectorOpenFoam(source, &cls_b, &(m_context()), &(m_queue()) );
		importarVectorOpenFoam(psi, &cls_x, &(m_context()), &(m_queue()) );

		clSParseSolverControl solverControl = nullptr;
		if (precond_name == "CLSPARSE_DIAGONAL") {
			solverControl = clsparseCreateSolverControl(DIAGONAL, maxIter_, relTol_, tolerance_);
		} else {
			solverControl = clsparseCreateSolverControl(NOPRECOND, maxIter_, relTol_, tolerance_);
		}

		// We can set different print modes of the solver status:
		// QUIET - print no messages (default)
		// NORMAL - print summary
		// VERBOSE - per iteration status;

		if (solverPrintMode == "QUIET") {
			clsparseSolverPrintMode(solverControl, QUIET);
		} else if (solverPrintMode == "NORMAL") {
			clsparseSolverPrintMode(solverControl, NORMAL);
		} else if (solverPrintMode == "VERBOSE") {
			clsparseSolverPrintMode(solverControl, VERBOSE);
		} else { // Si se ingresa un valor erroneo, toma QUIET por defecto
			clsparseSolverPrintMode(solverControl, QUIET);
		}
		/*
		 * FIXME! (juan) : Problema con el tipo de datos (float o double)
		 * status = clsparse___S___csrcg(&x, &A, &b, solverControl, control);
		 * status = clsparse___D___csrcg(&x, &A, &b, solverControl, control);
		 *
		 */
		clsparseDcsrcg(
				&cls_x,
				&cls_matrix,
				&cls_b, solverControl,
				m_clSparseControl
			);

		//release solver control structure after finishing execution;
		clsparseReleaseSolverControl(solverControl);

	}

	return solverPerf;
//
}

// ************************************************************************* //

