/*
 * GradienteConjugado.h
 *
 *  Created on: 4 de abr. de 2016
 *      Author: juan
 */

#ifndef GRADIENTECONJUGADO_H_
#define GRADIENTECONJUGADO_H_

#include <CL/cl.h>
#include "clSparseFoamMatrix.h"
#include "clSparseDenseFoamVector.h"

class GradienteConjugado {
public:
	GradienteConjugado();

	cl_int ejecutarGradienteConjugadoCSR(clSparseDenseFoamVector *x, const clSparseFoamMatrix *A, const clSparseDenseFoamVector *b, clSParseSolverControl solverControl, clsparseControl control);

	~GradienteConjugado();
};

#endif /* GRADIENTECONJUGADO_H_ */
