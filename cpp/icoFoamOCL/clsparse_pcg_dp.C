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
//#include "clSparseUtils.h"
#include "clsparse_pcg_dp.H"
#include "clSparseFoamMatrix.h"
#include "clSparseDenseFoamVector.h"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

#define DD

namespace Foam {
defineTypeNameAndDebug(clSPARSE_PCG_DP, 0);

lduMatrix::solver::addsymMatrixConstructorToTable<clSPARSE_PCG_DP> addclSPARSE_PCG_DPSymMatrixConstructorToTable_;
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
Foam::clSPARSE_PCG_DP::clSPARSE_PCG_DP(const word& fieldName, const lduMatrix& matrix, const FieldField<Field, scalar>& interfaceBouCoeffs, const FieldField<Field, scalar>& interfaceIntCoeffs,
		const lduInterfaceFieldPtrsList& interfaces, const dictionary& solverControls) :
		lduMatrix::solver(fieldName, matrix, interfaceBouCoeffs, interfaceIntCoeffs, interfaces, solverControls) {

	std::vector<cl::Platform> platforms;
	cl_status = cl::Platform::get(&platforms);

//	verificarError(cl_status);

	auto platform_id = getPlatformId();
	//TODO (juan) usar puntero?
	cl::Platform platform = platforms[platform_id];
//	cl_status = platform.getDevices(CL_DEVICE_TYPE_CPU, &m_devices);
	cl_status = platform.getDevices(CL_DEVICE_TYPE_GPU, &m_devices);
//	verificarError(cl_status);

	auto device_id = getDeviceId();
	device_id = getDeviceId();
	m_device = m_devices[device_id];
	m_context = cl::Context(m_device);
	m_queue = cl::CommandQueue(m_context, m_device);

	clsparseStatus p_clSparseStatus = clsparseSuccess;
	p_clSparseStatus = clsparseSetup();
	cl_command_queue &clCommandQueue = m_queue();
	m_clSparseControl = clsparseCreateControl(clCommandQueue, &p_clSparseStatus);

	//Ver cuantas veces es necesario hacer el init() de los vectores y/o matrices

}

typedef struct solverStruct {
	// current solver iteration;
	cl_int nIters;

	// maximum solver iterations;
	cl_int maxIters;

	// preconditioner type
	PRECONDITIONER preconditioner;

	// required relative tolerance
	cl_double relativeTolerance;

	// required absolute tolerance
	cl_double absoluteTolerance;

	cl_double initialResidual;

	cl_double currentResidual;

	PRINT_MODE printMode;
};

std::size_t Foam::clSPARSE_PCG_DP::getPlatformId() {
	return 0;
}

std::size_t Foam::clSPARSE_PCG_DP::getDeviceId() {
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
Foam::solverPerformance Foam::clSPARSE_PCG_DP::solve(Foam::scalarField& psi, const Foam::scalarField& source, const Foam::direction cmpt) const {
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

		cl_context context = m_context();
		cl_command_queue queue = m_queue();
		clSparseFoamMatrix cls_matrix(matrix(), context, queue, m_clSparseControl);
		clSparseDenseFoamVector cls_b(source, context, queue, false);
		clSparseDenseFoamVector cls_x(psi, context, queue, true);

		clSParseSolverControl solverControl = nullptr;
		if (precond_name == "CLSPARSE_DIAGONAL") {
			solverControl = clsparseCreateSolverControl(DIAGONAL, maxIter_, relTol_, tolerance_);
		} else {
			solverControl = clsparseCreateSolverControl(NOPRECOND, maxIter_, relTol_, tolerance_);
		}

		if (solverPrintMode == "QUIET") {
			clsparseSolverPrintMode(solverControl, QUIET);
		} else if (solverPrintMode == "NORMAL") {
			clsparseSolverPrintMode(solverControl, NORMAL);
		} else if (solverPrintMode == "VERBOSE") {
			clsparseSolverPrintMode(solverControl, VERBOSE);
		} else { // Si se ingresa un valor erroneo, toma QUIET por defecto
			clsparseSolverPrintMode(solverControl, QUIET);
		}

		clsparseDcsrcg(&cls_x, &cls_matrix, &cls_b, solverControl, m_clSparseControl);

		cls_x.exportar(psi, queue);

		solverStruct* str = (solverStruct*) solverControl;

		solverPerf.finalResidual() = str->currentResidual / normFactor;    //ls.GetCurrentResidual() / normFactor; // divide by normFactor, see lduMatrixSolver.C
		solverPerf.nIterations() = str->nIters;  //ls.GetIterationCount();
		solverPerf.checkConvergence(tolerance_, relTol_);

		clsparseReleaseSolverControl(solverControl);

	}

	return solverPerf;
//
}

// ************************************************************************* //

