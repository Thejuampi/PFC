/*
 * GradienteConjugado.cpp
 *
 *  Created on: 4 de abr. de 2016
 *      Author: juan
 */

#include "GradienteConjugado.h"
#include <memory>

GradienteConjugado::GradienteConjugado() {
	// TODO Auto-generated constructor stub

}

cl_int GradienteConjugado::ejecutarGradienteConjugadoCSR(clSparseDenseFoamVector* x, const clSparseFoamMatrix* A, const clSparseDenseFoamVector* b, clSParseSolverControl solverControl,
		clsparseControl control) {
	using T = cl_double;

	preconditioner = std::shared_ptr<ClPreconditionerHandler<T>>(new DiagonalPreconditioner<T>) ;



	return CL_SUCCESS;
}

GradienteConjugado::~GradienteConjugado() {
	// TODO Auto-generated destructor stub
}

